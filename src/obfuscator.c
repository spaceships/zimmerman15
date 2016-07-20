#include "obfuscator.h"

// typedef struct {
//     size_t n;           // number of inputs
//     size_t m;           // number of constants
//     mpz_t **xhatib;     // i \in [n], b \in {0,1}
//     mpz_t **uhatib;
//     mpz_t *yhatj;       // j \in [m]
//     mpz_t vhat;
//     mpz_t **zhatib;
//     mpz_t **whatib;
// } obfuscation;

obfuscation* obfuscate (acirc *c, aes_randstate_t rng)
{
    obfuscation *obf = zim_malloc(sizeof(obfuscation));
    obf->n = c->ninputs;
    obf->m = c->nconsts;



    return obf;
}

void obfuscation_destroy (obfuscation *obf)
{
    for (ul i = 0; i < obf->n; i++) {
        mpz_vect_destroy(obf->xhatib[i], 2);
        mpz_vect_destroy(obf->uhatib[i], 2);
        mpz_vect_destroy(obf->zhatib[i], 2);
        mpz_vect_destroy(obf->whatib[i], 2);
    }
    free(obf->xhatib);
    free(obf->uhatib);
    free(obf->zhatib);
    free(obf->whatib);
    mpz_vect_destroy(obf->yhatj, obf->m);
    mpz_clear(obf->vhat);
}

void obfuscation_write (FILE *fp, obfuscation *obf);
void obfuscation_read  (obfuscation *obf, FILE *fp);
