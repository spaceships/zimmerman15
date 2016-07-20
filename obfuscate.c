#include "mmap.h"
#include "obf_params.h"

#include <aesrand.h>
#include <stdio.h>
#include <unistd.h>


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
                abort();
        }
    }

    char *acirc_filename;
    if (optind >= argc) {
        fprintf(stderr, "[error] circuit required\n");
        exit(1);
    } else if (optind == argc - 1) {
        acirc_filename = argv[optind];
    } else {
        fprintf(stderr, "[error] unkonwn argument \"%s\"\n", argv[optind]);
    }

    obf_params op;
    obf_params_init(&op, acirc_filename, fake);
    printf("lambda=%lu fake=%d delta=%lu\n", lambda, op.fake, op.delta);

    aes_randstate_t rng;
    aes_randinit(rng);

    secret_params sp;
    secret_params_init(&sp, &op, lambda, rng);

    aes_randclear(rng);
    obf_params_clear(&op);
    secret_params_clear(&sp);
}
