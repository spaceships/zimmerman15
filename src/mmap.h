#ifndef __ZIMMERMAN_MMAP__
#define __ZIMMERMAN_MMAP__

#include "obf_index.h"
#include "aesrand.h"
#include <acirc.h>
#include <mmap/mmap.h>
#include <stdlib.h>

#define NSLOTS 2

typedef struct {
    obf_index *toplevel;
    mmap_sk *sk;
} secret_params;

typedef struct {
    obf_index *toplevel;
    bool toplevel_local;
    mmap_pp *pp;
} public_params;

typedef struct {
    obf_index *index;
    mmap_enc enc;
} encoding;

secret_params* secret_params_create (const mmap_vtable *mmap, acirc *c, size_t lambda, size_t ncores, aes_randstate_t rng);
void secret_params_destroy (const mmap_vtable *mmap, secret_params *sp);
mpz_t* get_moduli (const mmap_vtable *mmap, secret_params *sp);

public_params* public_params_create (const mmap_vtable *mmap, secret_params *sp);
void public_params_destroy (public_params *pp);

encoding* encode (const mmap_vtable *mmap, mpz_t inp0, mpz_t inp2, const obf_index *ix, secret_params *sp);

encoding* encoding_create (const mmap_vtable *mmap, public_params *pp, size_t n);
encoding* encoding_copy (const mmap_vtable *mmap, public_params *pp, encoding *x);
void encoding_destroy (const mmap_vtable *mmap, encoding *x);
void encoding_mul (const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_add (const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_sub (const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
int encoding_is_zero (const mmap_vtable *mmap, encoding *x, public_params *p);

public_params* public_params_read (const mmap_vtable *mmap, FILE *fp);
void public_params_write (const mmap_vtable *mmap, FILE *fp, public_params *pp);

void encoding_print (encoding *x);
encoding* encoding_read (const mmap_vtable *mmap, public_params *pp, FILE *fp);
void encoding_write (const mmap_vtable *mmap, FILE *fp, encoding *x);

#endif
