#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void secret_params_init (
    secret_params *p,
    obf_params *op,
    obf_index *toplevel,
    size_t lambda,
    aes_randstate_t rng
) {
    p->op = op;
    p->toplevel = toplevel;

#if FAKE_MMAP
    mpz_t *moduli = zim_malloc(2 * sizeof(mpz_t));
    for (int i = 0; i < 2; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb_aes(moduli[i], rng, 16);
    }
    p->moduli = moduli;
#endif
    size_t kappa = op->delta + 2 * op->c->ninputs;
    printf("kappa=%lu\n", kappa);
#if !FAKE_MMAP
    p->clt_st = zim_malloc(sizeof(clt_state));
    clt_state_init(
        p->clt_st,
        kappa,
        lambda,
        toplevel->nzs,
        (const int*) toplevel->pows,
        CLT_FLAG_DEFAULT | CLT_FLAG_VERBOSE,
        rng
    );
#endif
}

void secret_params_clear (secret_params *p)
{
    obf_index_destroy(p->toplevel);
#if FAKE_MMAP
    for (int i = 0; i < 2; i++) {
        mpz_clear(p->moduli[i]);
    }
    free(p->moduli);
#else
    clt_state_clear(p->clt_st);
    free(p->clt_st);
#endif
}

void public_params_init (public_params *p, secret_params *s)
{
    p->toplevel = s->toplevel;
    p->op = s->op;
#if FAKE_MMAP
    p->moduli = s->moduli;
#else
    p->clt_pp = zim_malloc(sizeof(clt_pp));
    clt_pp_init(p->clt_pp, s->clt_st);
#endif
}

void public_params_clear (public_params *p)
{
#if !FAKE_MMAP
    clt_pp_clear(p->clt_pp);
    free(p->clt_pp);
#endif
}

mpz_t* get_moduli (secret_params *s)
{
#if FAKE_MMAP
    return s->moduli;
#else
    return s->clt_st->gs;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, obf_params *p)
{
    x->index = zim_malloc(sizeof(obf_index));
    obf_index_init(x->index, p);
    x->nslots = 2;
    #if FAKE_MMAP
    x->slots = zim_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
    #else
    mpz_init(x->clt);
    #endif
}

void encoding_clear (encoding *x)
{
    obf_index_clear(x->index);
    free(x->index);
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++) {
        mpz_clear(x->slots[i]);
    }
    free(x->slots);
#else
    mpz_clear(x->clt);
#endif
}

void encoding_set (encoding *rop, encoding *x)
{
    rop->nslots = x->nslots;
    obf_index_set(rop->index, x->index);
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++) {
        mpz_set(rop->slots[i], x->slots[i]);
    }
#else
    mpz_set(rop->clt, x->clt);
#endif
}

void encode (
    encoding *x,
    mpz_t *inps,
    size_t nins,
    const obf_index *ix,
    secret_params *p,
    aes_randstate_t rng
){
    assert(nins == x->nslots);
    obf_index_set(x->index, ix);
#if FAKE_MMAP
    for (int i = 0; i < nins; i++) {
        mpz_set(x->slots[i], inps[i]);
    }
#else
    clt_encode(x->clt, p->clt_st, nins, inps, (const int*) ix->pows, rng);
#endif
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    obf_index_add(rop->index, x->index, y->index);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_mul(rop->clt, x->clt, y->clt);
    mpz_mod(rop->clt, rop->clt, p->clt_pp->x0);
#endif
}

void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_add(rop->clt, x->clt, y->clt);
#endif
}

void encoding_sub(encoding *rop, encoding *x, encoding *y, public_params *p)
{
    if (!obf_index_eq(x->index, y->index)) {
        printf("[encoding_sub] unequal indices!\nx=\n");
        obf_index_print(x->index);
        printf("y=\n");
        obf_index_print(y->index);
    }
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_sub(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_sub(rop->clt, x->clt, y->clt);
#endif
}

int encoding_eq (encoding *x, encoding *y)
{
    if (!obf_index_eq(x->index, y->index))
        return 0;
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++)
        if (mpz_cmp(x->slots[i], y->slots[i]) != 0)
            return 0;
#else
    if (mpz_cmp(x->clt, y->clt) != 0)
        return 0;
#endif
    return 1;
}

int encoding_is_zero (encoding *x, public_params *p)
{
    bool ret;
    if(!obf_index_eq(x->index, p->toplevel)) {
        puts("this index:");
        obf_index_print(x->index);
        puts("top index:");
        obf_index_print(p->toplevel);
        assert(obf_index_eq(x->index, p->toplevel));
    }
#if FAKE_MMAP
    ret = true;
    for (int i = 0; i < x->nslots; i++)
        ret &= mpz_sgn(x->slots[i]) == 0;
#else
    ret = clt_is_zero(p->clt_pp, x->clt);
#endif
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void secret_params_read  (secret_params *x, FILE *const fp);
void secret_params_write (FILE *const fp, secret_params *x);
void public_params_read  (public_params *x, FILE *const fp);
void public_params_write (FILE *const fp, public_params *x);

void encoding_read (encoding *x, FILE *const fp)
{
    x->index = zim_malloc(sizeof(obf_index));
    obf_index_read(x->index, fp);
    GET_SPACE(fp);
    ulong_read(&x->nslots, fp);
    GET_SPACE(fp);
#if FAKE_MMAP
    x->slots = zim_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
    clt_vector_fread(fp, x->slots, x->nslots);
#else
    mpz_init(x->clt);
    clt_elem_fread(fp, x->clt);
#endif
}

void encoding_write (FILE *const fp, encoding *x)
{
    obf_index_write(fp, x->index);
    PUT_SPACE(fp);
    ulong_write(fp, x->nslots);
    PUT_SPACE(fp);
#if FAKE_MMAP
    clt_vector_fsave(fp, x->slots, x->nslots);
#else
    clt_elem_fsave(fp, x->clt);
#endif
}
