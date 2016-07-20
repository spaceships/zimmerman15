#include "mmap.h"
#include "obf_params.h"

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
    int c;
    ul lambda = 10;
    while ((c = getopt (argc, argv, "fl:")) != -1) {
        switch (c) {
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

    obf_params op;
    obf_params_init(&op, acirc_filename, fake);

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu nrefs=%lu delta=%lu\n",
           op.c->ninputs, op.c->nconsts, op.c->ngates, op.c->nrefs, op.delta);

    printf("obfuscation: fake=%d lambda=%lu kappa=%lu\n",
           fake, lambda, op.delta + 2 * op.c->ninputs);

    aes_randstate_t rng;
    aes_randinit(rng);

    secret_params sp;
    secret_params_init(&sp, &op, lambda, rng);




    aes_randclear(rng);
    obf_params_clear(&op);
    secret_params_clear(&sp);
}
