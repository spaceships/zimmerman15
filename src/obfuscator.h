#ifndef __ZIMMERMAN_OBFUSCATOR__
#define __ZIMMERMAN_OBFUSCATOR__

#include "mmap.h"
#include "util.h"
#include <acirc.h>
#include <clt13.h>
#include <gmp.h>

#define NUM_ENCODINGS(c) ( \
        (c)->ninputs * 2 * 2 + \
        (c)->ninputs * 2 * (c)->noutputs * 2 + \
        (c)->nconsts + \
        1 + \
        (c)->noutputs \
    )

typedef struct {
    size_t ninputs;         // n
    size_t nconsts;         // m
    size_t noutputs;        // o
    public_params *pp;
    encoding ***xhat;       // [n][2]
    encoding ***uhat;       // [n][2]
    encoding ****zhat;      // [n][2][o]
    encoding ****what;      // [n][2][o]
    encoding **yhat;        // [m]
    encoding *vhat;         // [1]
    encoding **Chatstar;    // [o]
} obfuscation;

obfuscation* obfuscate (acirc *c, secret_params *sp, aes_randstate_t rng);

void obfuscation_destroy (obfuscation *obf);

void obfuscation_write (FILE *fp, obfuscation *obf);
obfuscation* obfuscation_read (FILE *fp);

int obf_eq (obfuscation *obf1, obfuscation *obf2); // for checking the serialization

#endif
