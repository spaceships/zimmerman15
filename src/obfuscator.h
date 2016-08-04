#ifndef __ZIMMERMAN_OBFUSCATOR__
#define __ZIMMERMAN_OBFUSCATOR__

#include "mmap.h"
#include "util.h"
#include <acirc.h>
#include <clt13.h>
#include <gmp.h>

#define NUM_ENCODINGS(C, NPOWERS) ( \
        (C)->ninputs * 2 + \
        (C)->ninputs * 2 * (NPOWERS) + \
        (C)->ninputs * 2 * (C)->noutputs * 2 + \
        (C)->nconsts + \
        (NPOWERS) + \
        (C)->noutputs \
    )

typedef struct {
    size_t ninputs;         // n
    size_t nconsts;         // m
    size_t noutputs;        // o
    size_t npowers;         // how many powers of 2 u's and v's we give out
    public_params *pp;
    encoding ***xhat;       // [n][2]
    encoding ****uhat;      // [n][2][npowers]
    encoding ****zhat;      // [n][2][o]
    encoding ****what;      // [n][2][o]
    encoding **yhat;        // [m]
    encoding **vhat;        // [npowers]
    encoding **Chatstar;    // [o]
} obfuscation;

obfuscation* obfuscate (acirc *c, secret_params *sp, size_t npowers, aes_randstate_t rng);

void obfuscation_destroy (obfuscation *obf);

void obfuscation_write (FILE *fp, obfuscation *obf);
obfuscation* obfuscation_read (FILE *fp);

int obf_eq (obfuscation *obf1, obfuscation *obf2); // for checking the serialization

#endif
