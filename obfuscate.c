#include "mmap.h"
#include "obfuscator.h"

#include <aesrand.h>
#include <stdio.h>
#include <unistd.h>

void usage()
{
    printf("Usage: obfuscate [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-l\tScurity parameter (default=10).\n");
    printf("\t-f\tUse fake multilinear map for testing.\n");
    puts("");
}

int main (int argc, char **argv)
{
    int fake = 0;
    ul lambda = 10;
    int arg;
    while ((arg = getopt(argc, argv, "fl:")) != -1) {
        switch (arg) {
            case 'f':
                fake = 1;
                break;
            case 'l':
                lambda = atol(optarg);
                break;
            default:
                usage();
                return 0;
        }
    }

    char *acirc_filename;
    if (optind >= argc) {
        fprintf(stderr, "[obfuscate] error: circuit required\n");
        usage();
        exit(1);
    } else if (optind == argc - 1) {
        acirc_filename = argv[optind];
    } else {
        fprintf(stderr, "[obfuscate] error: unexpected argument \"%s\"\n", argv[optind + 1]);
        usage();
        exit(1);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // all right, lets get to it!

    acirc *c = acirc_from_file(acirc_filename);
    size_t delta = acirc_delta(c);

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu nrefs=%lu delta=%lu\n",
           c->ninputs, c->nconsts, c->ngates, c->nrefs, delta);

    printf("obfuscation: fake=%d lambda=%lu kappa=%lu\n",
           fake, lambda, delta + 2*c->ninputs);

    aes_randstate_t rng;
    aes_randinit(rng);

    secret_params *sp = secret_params_create(c, lambda, rng, fake);
    obfuscation  *obf = obfuscate(c, sp, rng);
    public_params *pp = public_params_create(sp);

    acirc_clear(c); free(c);
    aes_randclear(rng);
    secret_params_destroy(sp);
    public_params_destroy(pp);
    obfuscation_destroy(obf);
}
