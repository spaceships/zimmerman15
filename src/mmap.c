#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <threadpool.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

secret_params* secret_params_create (const mmap_vtable *mmap, acirc *c, size_t lambda, size_t ncores, aes_randstate_t rng)
{
    secret_params *sp = zim_malloc(sizeof(secret_params));
    sp->toplevel = obf_index_create_toplevel(c);
    size_t kappa = acirc_delta(c) + 2*c->ninputs;

    sp->sk = zim_malloc(mmap->sk->size);
    mmap->sk->init(sp->sk, lambda, kappa, sp->toplevel->nzs, (int *) sp->toplevel->pows, 2, ncores, rng, true);

    return sp;
}

void secret_params_destroy (const mmap_vtable *mmap, secret_params *sp)
{
    obf_index_destroy(sp->toplevel);

    mmap->sk->clear(sp->sk);
    free(sp->sk);
    free(sp);
}

mpz_t * get_moduli (const mmap_vtable *mmap, secret_params *sp)
{
    fmpz_t *fmoduli;
    mpz_t *moduli;
    const size_t nslots = mmap->sk->nslots(sp->sk);

    fmoduli = mmap->sk->plaintext_fields(sp->sk);
    moduli = calloc(nslots, sizeof(mpz_t));
    for (size_t i = 0; i < nslots; ++i) {
        mpz_init(moduli[i]);
        fmpz_get_mpz(moduli[i], fmoduli[i]);
        fmpz_clear(fmoduli[i]);
    }
    free(fmoduli);
    return moduli;
}

////////////////////////////////////////////////////////////////////////////////

public_params* public_params_create (const mmap_vtable *mmap, secret_params *sp)
{
    public_params *pp = zim_malloc(sizeof(public_params));
    pp->toplevel = sp->toplevel;
    pp->toplevel_local = false;
    pp->pp = mmap->sk->pp(sp->sk);
    return pp;
}

void public_params_destroy (public_params *pp)
{
    if (pp->toplevel_local)
        obf_index_destroy(pp->toplevel);
    free(pp);
}

////////////////////////////////////////////////////////////////////////////////
// encodings

encoding* encode (const mmap_vtable *mmap, mpz_t inp0, mpz_t inp1, const obf_index *ix, secret_params *sp)
{
    encoding *x = zim_malloc(sizeof(encoding));
    fmpz_t inps[2];

    fmpz_init(inps[0]);
    fmpz_init(inps[1]);
    fmpz_set_mpz(inps[0], inp0);
    fmpz_set_mpz(inps[1], inp1);

    x->index = obf_index_copy(ix);
    mmap->enc->init(&x->enc, mmap->sk->pp(sp->sk));
    mmap->enc->encode(&x->enc, sp->sk, 2, inps, (int *) ix->pows);

    fmpz_clear(inps[0]);
    fmpz_clear(inps[1]);

    return x;
}

encoding* encoding_create (const mmap_vtable *mmap, public_params *pp, size_t n)
{
    encoding *x = zim_malloc(sizeof(encoding));
    x->index = obf_index_create(n);
    mmap->enc->init(&x->enc, pp->pp);
    return x;
}

encoding* encoding_copy (const mmap_vtable *mmap, public_params *pp, encoding *x)
{
    encoding *res = encoding_create(mmap, pp, x->index->n);
    obf_index_set(res->index, x->index);
    mmap->enc->set(&res->enc, &x->enc);
    return res;
}

void encoding_destroy (const mmap_vtable *mmap, encoding *x)
{
    if (x->index)
        obf_index_destroy(x->index);
    mmap->enc->clear(&x->enc);
    free(x);
}

void encoding_mul (const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p)
{
    obf_index_add(rop->index, x->index, y->index);
    mmap->enc->mul(&rop->enc, p->pp, &x->enc, &y->enc);
}

void encoding_add (const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
    mmap->enc->add(&rop->enc, p->pp, &x->enc, &y->enc);
}

void encoding_sub(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
    mmap->enc->sub(&rop->enc, p->pp, &x->enc, &y->enc);
}

int encoding_is_zero (const mmap_vtable *mmap, encoding *x, public_params *p)
{
    if(!obf_index_eq(x->index, p->toplevel)) {
        puts("this index:");
        obf_index_print(x->index);
        puts("top index:");
        obf_index_print(p->toplevel);
        assert(obf_index_eq(x->index, p->toplevel));
    }
    return mmap->enc->is_zero(&x->enc, p->pp);
}

////////////////////////////////////////////////////////////////////////////////
// serialization

public_params* public_params_read (const mmap_vtable *mmap, FILE *fp)
{
    public_params *const pp = zim_malloc(sizeof(public_params));
    if ((pp->toplevel = obf_index_read(fp)) == NULL || GET_NEWLINE(fp)) {
        fprintf(stderr, "[%s] failed to read obf_index!\n", __func__);
        public_params_destroy(pp);
        return NULL;
    }
    pp->toplevel_local = true;
    pp->pp = zim_malloc(mmap->pp->size);
    mmap->pp->fread(pp->pp, fp);
    return pp;
}

void public_params_write (const mmap_vtable *mmap, FILE *const fp, public_params *pp)
{
    obf_index_write(fp, pp->toplevel);
    (void) PUT_NEWLINE(fp);
    mmap->pp->fwrite(pp->pp, fp);
}

void encoding_print (encoding *x)
{
    puts("=encoding=");
    obf_index_print(x->index);
}

encoding* encoding_read (const mmap_vtable *mmap, public_params *pp, FILE *fp)
{
    encoding *x = zim_calloc(1, sizeof(encoding));
    if ((x->index = obf_index_read(fp)) == NULL || GET_SPACE(fp)) {
        fprintf(stderr, "[%s] failed to read obf_index!\n", __func__);
        encoding_destroy(mmap, x);
        return NULL;
    }
    mmap->enc->init(&x->enc, pp->pp);
    mmap->enc->fread(&x->enc, fp);
    return x;
}

void encoding_write (const mmap_vtable *mmap, FILE *fp, encoding *x)
{
    obf_index_write(fp, x->index);
    (void) PUT_SPACE(fp);
    mmap->enc->fwrite(&x->enc, fp);
}

