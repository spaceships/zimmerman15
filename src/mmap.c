#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

secret_params* secret_params_create (acirc *c, size_t lambda, aes_randstate_t rng, int fake)
{
    secret_params *sp = zim_malloc(sizeof(secret_params));

    sp->fake = fake;
    sp->toplevel = obf_index_create_toplevel(c);

    size_t kappa = acirc_delta(c) + 2*c->ninputs;

    if (sp->fake) {
        mpz_t *moduli = zim_malloc(2 * sizeof(mpz_t));
        for (int i = 0; i < 2; i++) {
            mpz_init(moduli[i]);
            mpz_urandomb_aes(moduli[i], rng, lambda);
        }
        sp->moduli = moduli;
    }

    else {
        sp->clt_st = zim_malloc(sizeof(clt_state));
        int flags = CLT_FLAG_DEFAULT | CLT_FLAG_VERBOSE;
        clt_state_init(sp->clt_st, kappa, lambda, sp->toplevel->nzs,
                       (const int*) sp->toplevel->pows, flags, rng);
    }

    return sp;
}

void secret_params_destroy (secret_params *sp)
{
    obf_index_destroy(sp->toplevel);

    if (sp->fake) {
        mpz_vect_destroy(sp->moduli, 2);
    }

    else {
        clt_state_clear(sp->clt_st);
        free(sp->clt_st);
    }

    free(sp);
}

mpz_t* get_moduli (secret_params *s)
{
    if (s->fake) {
        return s->moduli;
    } else {
        return s->clt_st->gs;
    }
}

////////////////////////////////////////////////////////////////////////////////

public_params* public_params_create (secret_params *sp)
{
    public_params *pp = zim_malloc(sizeof(public_params));

    pp->fake = sp->fake;
    pp->toplevel = sp->toplevel;
    pp->my_toplevel = 0;
    pp->my_moduli = 0;

    if (pp->fake){
        pp->moduli = sp->moduli;
        pp->my_clt_pp = 0;
    } else {
        pp->clt_pp = zim_malloc(sizeof(clt_pp));
        clt_pp_init(pp->clt_pp, sp->clt_st);
        pp->my_clt_pp = 1;
    }

    return pp;
}

void public_params_destroy (public_params *pp)
{
    if (pp->my_toplevel) {
        obf_index_destroy(pp->toplevel);
    }
    if (pp->my_moduli) {
        mpz_vect_destroy(pp->moduli, 2);
    }
    if (pp->my_clt_pp) {
        clt_pp_clear(pp->clt_pp);
        free(pp->clt_pp);
    }
    free(pp);
}

int public_params_eq (public_params *pp1, public_params *pp2)
{
    assert(obf_index_eq(pp1->toplevel, pp2->toplevel));
    assert(pp1->fake == pp2->fake);
    if (pp1->fake) {
        assert(mpz_vect_eq(pp1->moduli, pp2->moduli, NSLOTS));
    } else {
        assert(mpz_eq(pp1->clt_pp->x0,  pp2->clt_pp->x0));
        assert(mpz_eq(pp1->clt_pp->pzt, pp2->clt_pp->pzt));
        assert(pp1->clt_pp->nu == pp1->clt_pp->nu);
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// encodings

encoding* encode (mpz_t inp0, mpz_t inp1, const obf_index *ix, secret_params *sp, aes_randstate_t rng)
{
    encoding *x = zim_malloc(sizeof(encoding));

    x->index = obf_index_copy(ix);
    x->fake = sp->fake;

    if (sp->fake) {
        x->slots = zim_malloc(NSLOTS * sizeof(mpz_t));
        mpz_inits(x->slots[0], x->slots[1], NULL);
        mpz_set(x->slots[0], inp0);
        mpz_set(x->slots[1], inp1);
    } else {
        mpz_init(x->clt);
        mpz_t *inps = mpz_vect_create(2);
        mpz_set(inps[0], inp0);
        mpz_set(inps[1], inp1);
        clt_encode(x->clt, sp->clt_st, NSLOTS, inps, (const int*) ix->pows, rng);
        mpz_vect_destroy(inps, 2);
    }

    return x;
}

encoding* encoding_create (size_t n, int fake)
{
    encoding *x = zim_malloc(sizeof(encoding));
    x->index = obf_index_create(n);
    x->fake = fake;
    if (x->fake) {
        x->slots = mpz_vect_create(2);
    }
    else {
        mpz_init(x->clt);
    }
    return x;
}

encoding* encoding_copy (encoding *x)
{
    encoding *res = encoding_create(x->index->n, x->fake);
    obf_index_set(res->index, x->index);
    if (x->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_set(res->slots[i], x->slots[i]);
        }
    } else {
        mpz_set(res->clt, x->clt);
    }
    return res;
}

void encoding_destroy (encoding *x)
{
    obf_index_destroy(x->index);
    if (x->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_clear(x->slots[i]);
        }
        free(x->slots);
    } else {
        mpz_clear(x->clt);
    }
    free(x);
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    obf_index_add(rop->index, x->index, y->index);
    if (p->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
            mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
        }
    } else {
        mpz_mul(rop->clt, x->clt, y->clt);
        mpz_mod(rop->clt, rop->clt, p->clt_pp->x0);
    }
}

void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
    if (p->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
            mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
        }
    } else {
        mpz_add(rop->clt, x->clt, y->clt);
    }
}

void encoding_sub(encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(obf_index_eq(x->index, y->index));
    obf_index_set(rop->index, x->index);
    if (p->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_sub(rop->slots[i], x->slots[i], y->slots[i]);
            mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
        }
    } else {
        mpz_sub(rop->clt, x->clt, y->clt);
    }
}

int encoding_eq (encoding *x, encoding *y)
{
    if (!obf_index_eq(x->index, y->index))
        return 0;
    if (x->fake) {
        for (int i = 0; i < NSLOTS; i++)
            if (mpz_cmp(x->slots[i], y->slots[i]) != 0)
                return 0;
    } else {
        if (mpz_cmp(x->clt, y->clt) != 0)
            return 0;
    }
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
    if (p->fake) {
        ret = true;
        for (int i = 0; i < NSLOTS; i++)
            ret &= mpz_sgn(x->slots[i]) == 0;
    } else {
        ret = clt_is_zero(p->clt_pp, x->clt);
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// serialization

public_params* public_params_read (FILE *fp)
{
    public_params *pp = zim_malloc(sizeof(public_params));
    int_read(&pp->fake, fp);
    GET_NEWLINE(fp);
    pp->toplevel = zim_malloc(sizeof(obf_index));
    obf_index_read(pp->toplevel, fp);
    pp->my_toplevel = 1;
    GET_NEWLINE(fp);

    if (pp->fake) {
        pp->moduli = zim_malloc(NSLOTS * sizeof(mpz_t));
        for (int i = 0; i < NSLOTS; i++) {
            mpz_init(pp->moduli[i]);
        }
        assert(clt_vector_fread(fp, pp->moduli, NSLOTS) == 0);
        pp->my_moduli = 1;
        pp->my_clt_pp = 0;
    }

    else {
        pp->clt_pp = zim_malloc(sizeof(clt_pp));
        assert(clt_pp_fread(fp, pp->clt_pp) == 0);
        pp->my_moduli = 0;
        pp->my_clt_pp = 1;
    }
    return pp;
}

void public_params_write (FILE *const fp, public_params *pp)
{
    int_write(fp, pp->fake);
    PUT_NEWLINE(fp);
    obf_index_write(fp, pp->toplevel);
    PUT_NEWLINE(fp);

    if (pp->fake) {
        assert(clt_vector_fsave(fp, pp->moduli, NSLOTS) == 0);
    }

    else {
        assert(clt_pp_fsave(fp, pp->clt_pp) == 0);
    }
}

void encoding_print (encoding *x)
{
    puts("=encoding=");
    obf_index_print(x->index);
    printf("fake=%d\n", x->fake);
    if (x->fake) {
        gmp_printf("[%Zd, %Zd]\n", x->slots[0], x->slots[1]);
    } else {
        gmp_printf("%Zd\n", x->clt);
    }
}

encoding* encoding_read (FILE *fp)
{
    encoding *x = zim_malloc(sizeof(encoding));
    int_read(&(x->fake), fp);
    GET_SPACE(fp);
    x->index = zim_malloc(sizeof(obf_index));
    obf_index_read(x->index, fp);
    GET_SPACE(fp);
    if (x->fake) {
        x->slots = zim_malloc(NSLOTS * sizeof(mpz_t));
        for (int i = 0; i < NSLOTS; i++) {
            mpz_init(x->slots[i]);
        }
        assert(clt_vector_fread(fp, x->slots, NSLOTS) == 0);
    }
    else {
        mpz_init(x->clt);
        assert(clt_elem_fread(fp, x->clt) == 0);
    }
    return x;
}

void encoding_write (FILE *fp, encoding *x)
{
    int_write(fp, x->fake);
    PUT_SPACE(fp);
    obf_index_write(fp, x->index);
    PUT_SPACE(fp);
    if (x->fake) {
        assert(clt_vector_fsave(fp, x->slots, NSLOTS) == 0);
    }
    else {
        assert(clt_elem_fsave(fp, x->clt) == 0);
    }
}

