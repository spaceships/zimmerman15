#ifndef __ZIMMERMAN_MMAP__
#define __ZIMMERMAN_MMAP__

#include "obf_index.h"
#include "aesrand.h"
#include <acirc.h>
#include <clt13.h>
#include <stdlib.h>

#define NSLOTS 2

typedef struct {
    obf_index *toplevel;
    clt_state *clt_st;

    int fake;
    mpz_t *moduli;          // fake moduli
} secret_params;

typedef struct {
    obf_index *toplevel;
    clt_pp *clt_pp;
    mpz_t *moduli;          // fake moduli

    int fake;
    int my_toplevel;
    int my_clt_pp;
    int my_moduli;
} public_params;

typedef struct {
    obf_index *index;
    clt_elem_t clt;

    int fake;
    mpz_t *slots;           // fake slots
} encoding;

secret_params* secret_params_create (acirc *c, size_t lambda, aes_randstate_t rng, int fake);
void secret_params_destroy (secret_params *pp);

mpz_t* get_moduli (secret_params *s);

public_params* public_params_create (secret_params *s);
void public_params_clear (public_params *pp);
void public_params_destroy (public_params *pp);

encoding* encode (mpz_t inp0, mpz_t inp2, const obf_index *ix, secret_params *sp, aes_randstate_t rng);
void encoding_set (encoding *rop, encoding *x);
void encoding_destroy (encoding *x);

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_sub (encoding *rop, encoding *x, encoding *y, public_params *p);
int  encoding_eq  (encoding *x, encoding *y);

int encoding_is_zero (encoding *x, public_params *p);

void public_params_read  (public_params *pp, FILE *const fp);
void public_params_write (FILE *const fp, public_params *pp);
void encoding_read  (encoding *x, FILE *const fp);
void encoding_write (FILE *const fp, encoding *x);

#endif
