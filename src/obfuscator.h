#ifndef __ZIMMERMAN_OBFUSCATOR__
#define __ZIMMERMAN_OBFUSCATOR__

#include "mmap.h"
#include "util.h"
#include <acirc.h>
#include <clt13.h>
#include <gmp.h>

typedef struct {
    size_t ninputs;
    size_t nconsts;
    size_t noutputs;
    public_params *pp;
    encoding ***xhat;     // i \in [ninputs], b \in {0,1}
    encoding ***uhat;
    encoding ****zhat;
    encoding ****what;
    encoding **yhat;       // j \in [nconsts]
    encoding *vhat;
    encoding **Chatstar;   // o \in [noutputs]
} obfuscation;

obfuscation* obfuscate (acirc *c, secret_params *sp, aes_randstate_t rng);

void obfuscation_destroy (obfuscation *obf);

void obfuscation_write (FILE *fp, obfuscation *obf);
obfuscation* obfuscation_read (FILE *fp);

int obf_eq (obfuscation *obf1, obfuscation *obf2); // for checking the serialization

#endif
