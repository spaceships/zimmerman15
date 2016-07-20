#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void secret_params_init (
    secret_params *p,
    obf_params *op,
    size_t lambda,
    aes_randstate_t rng
) {
    p->op = op;
    p->toplevel = obf_index_create_toplevel(op);

    size_t kappa = op->delta + 2 * op->c->ninputs;

    if (op->fake) {
        mpz_t *moduli = zim_malloc(2 * sizeof(mpz_t));
        for (int i = 0; i < 2; i++) {
            mpz_init(moduli[i]);
            mpz_urandomb_aes(moduli[i], rng, 16);
        }
        p->moduli = moduli;
    }

    else {
        p->clt_st = zim_malloc(sizeof(clt_state));
        int flags = CLT_FLAG_DEFAULT | CLT_FLAG_VERBOSE;
        clt_state_init(p->clt_st, kappa, lambda, p->toplevel->nzs,
                       (const int*) p->toplevel->pows, flags, rng);
    }
}

void secret_params_clear (secret_params *p)
{
    obf_index_destroy(p->toplevel);

    if (p->op->fake) {
        for (int i = 0; i < 2; i++) {
            mpz_clear(p->moduli[i]);
        }
        free(p->moduli);
    }

    else {
        clt_state_clear(p->clt_st);
        free(p->clt_st);
    }
}

void public_params_init (public_params *p, secret_params *s)
{
    p->op = s->op;
    p->toplevel = s->toplevel;
    if (p->op->fake){
        p->moduli = s->moduli;
    } else {
        p->clt_pp = zim_malloc(sizeof(clt_pp));
        clt_pp_init(p->clt_pp, s->clt_st);
    }

    p->my_op = 0;
    p->my_toplevel = 0;
    p->my_moduli = 0;
    p->my_clt_pp = 1;
}

void public_params_clear (public_params *p)
{
    if (p->my_toplevel) {
        obf_index_clear(p->toplevel);
        free(p->toplevel);
    }

    if (p->my_moduli) {
        mpz_vect_destroy(p->moduli, 2);
    }

    if (p->my_clt_pp) {
        clt_pp_clear(p->clt_pp);
        free(p->clt_pp);
    }
}

mpz_t* get_moduli (secret_params *s)
{
    if (s->op->fake) {
        return s->moduli;
    } else {
        return s->clt_st->gs;
    }
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, obf_params *p)
{
    x->index = zim_malloc(sizeof(obf_index));
    obf_index_init(x->index, p);
    x->fake = p->fake;
    if (x->fake) {
        x->slots = zim_malloc(NSLOTS * sizeof(mpz_t));
        for (int i = 0; i < NSLOTS; i++) {
            mpz_init(x->slots[i]);
        }
    } else {
        mpz_init(x->clt);
    }
}

void encoding_clear (encoding *x)
{
    obf_index_clear(x->index);
    free(x->index);
    if (x->fake) {
        for (int i = 0; i < NSLOTS; i++) {
            mpz_clear(x->slots[i]);
        }
        free(x->slots);
    } else {
        mpz_clear(x->clt);
    }
}

void encoding_set (encoding *rop, encoding *x)
{
    obf_index_set(rop->index, x->index);
    if (x->fake) {
        for (int i = 0; NSLOTS; i++) {
            mpz_set(rop->slots[i], x->slots[i]);
        }
    } else {
        mpz_set(rop->clt, x->clt);
    }
}

void encode (
    encoding *x,
    mpz_t *inps,
    size_t nins,
    const obf_index *ix,
    secret_params *p,
    aes_randstate_t rng
){
    assert(nins == NSLOTS);
    obf_index_set(x->index, ix);
    if (p->op->fake) {
        for (int i = 0; i < nins; i++) {
            mpz_set(x->slots[i], inps[i]);
        }
    } else {
        clt_encode(x->clt, p->clt_st, nins, inps, (const int*) ix->pows, rng);
    }
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    obf_index_add(rop->index, x->index, y->index);
    if (p->op->fake) {
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
    if (p->op->fake) {
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
    if (p->op->fake) {
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
    if (p->op->fake) {
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

void public_params_read (public_params *pp, FILE *const fp, obf_params *op)
{
    pp->op = op;

    pp->toplevel = zim_malloc(sizeof(obf_index));
    obf_index_read(pp->toplevel, fp);
    pp->my_toplevel = 1;
    GET_NEWLINE(fp);

    if (op->fake) {
        pp->moduli = zim_malloc(NSLOTS * sizeof(mpz_t));
        for (int i = 0; i < NSLOTS; i++) {
            mpz_init(pp->moduli[i]);
        }
        clt_vector_fread(fp, pp->moduli, NSLOTS);
        pp->my_moduli = 1;
        pp->my_clt_pp = 0;
    }

    else {
        pp->clt_pp = zim_malloc(sizeof(clt_pp));
        clt_pp_fread(fp, pp->clt_pp);
        pp->my_moduli = 0;
        pp->my_clt_pp = 1;
    }
}


void public_params_write (FILE *const fp, public_params *pp)
{
    obf_index_write(fp, pp->toplevel);
    PUT_NEWLINE(fp);

    if (pp->op->fake) {
        clt_vector_fsave(fp, pp->moduli, NSLOTS);
    }

    else {
        clt_pp_fsave(fp, pp->clt_pp);
    }
}

void encoding_read (encoding *x, FILE *const fp)
{
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
        clt_vector_fread(fp, x->slots, NSLOTS);
    }

    else {
        mpz_init(x->clt);
        clt_elem_fread(fp, x->clt);
    }
}

void encoding_write (FILE *const fp, encoding *x)
{
    int_write(fp, x->fake);
    PUT_SPACE(fp);

    obf_index_write(fp, x->index);
    PUT_SPACE(fp);

    if (x->fake) {
        clt_vector_fsave(fp, x->slots, NSLOTS);
    }

    else {
        clt_elem_fsave(fp, x->clt);
    }
}
