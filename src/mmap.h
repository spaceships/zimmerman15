#ifndef __ZIMMERMAN_MMAP__
#define __ZIMMERMAN_MMAP__

#include "obf_index.h"
#include "obf_params.h"
#include "aesrand.h"
#include <clt13.h>
#include <stdlib.h>

#define NSLOTS 2

typedef struct {
    obf_params *op;
    obf_index *toplevel;
    clt_state *clt_st;
    mpz_t *moduli;          // fake moduli
} secret_params;

typedef struct {
    obf_params *op;
    obf_index *toplevel;
    clt_pp *clt_pp;

    int my_op;
    int my_toplevel;
    int my_clt_pp;

    mpz_t *moduli;          // fake moduli
    int my_moduli;
} public_params;

typedef struct {
    obf_index *index;
    clt_elem_t clt;
    mpz_t *slots;           // fake slots
    int fake;
} encoding;

void secret_params_init (secret_params *sp, obf_params *op, size_t lambda, aes_randstate_t rng);
void secret_params_clear (secret_params *pp);

mpz_t* get_moduli (secret_params *s);

void public_params_init  (public_params *p, secret_params *s);
void public_params_clear (public_params *pp);

void encoding_init  (encoding *x, obf_params *p);
void encoding_clear (encoding *x);
void encoding_set   (encoding *rop, encoding *x);

void encode (
    encoding *x,
    mpz_t *inps,
    size_t nins,
    const obf_index *ix,
    secret_params *p,
    aes_randstate_t rng
);

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_sub (encoding *rop, encoding *x, encoding *y, public_params *p);
int  encoding_eq  (encoding *x, encoding *y);

int encoding_is_zero (encoding *x, public_params *p);

void public_params_read  (public_params *pp, FILE *const fp, obf_params *op);
void public_params_write (FILE *const fp, public_params *pp);
void encoding_read  (encoding *x, FILE *const fp);
void encoding_write (FILE *const fp, encoding *x);

#endif
