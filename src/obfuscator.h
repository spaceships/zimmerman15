#ifndef __ZIMMERMAN_OBFUSCATOR__
#define __ZIMMERMAN_OBFUSCATOR__

#include "util.h"
#include <acirc.h>
#include <clt13.h>
#include <gmp.h>

typedef struct {
    size_t n;           // number of inputs
    size_t m;           // number of constants
    mpz_t **xhatib;     // i \in [n], b \in {0,1}
    mpz_t **uhatib;
    mpz_t *yhatj;       // j \in [m]
    mpz_t vhat;
    mpz_t **zhatib;
    mpz_t **whatib;
} obfuscation;

obfuscation* obfuscate (acirc *c, aes_randstate_t rng);

void obfuscation_destroy (obfuscation *obf);
void obfuscation_write   (FILE *fp, obfuscation *obf);
void obfuscation_read    (obfuscation *obf, FILE *fp);

#endif
