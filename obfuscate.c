#include "mmap.h"
#include "obfuscator.h"

#include <aesrand.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

void usage()
{
    printf("Usage: obfuscate [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-l\tScurity parameter (default=10).\n");
    printf("\t-f\tUse fake multilinear map for testing.\n");
    printf("\t-o\tSpecify obfuscation output file.\n");
    puts("");
}

int main (int argc, char **argv)
{
    int fake = 0;
    ul lambda = 10;
    int output_filename_set = 0;
    char output_filename [1024];
    int arg;
    while ((arg = getopt(argc, argv, "fl:o:")) != -1) {
        if (arg == 'f') {
            fake = 1;
        }
        else if (arg == 'l') {
            lambda = atol(optarg);
        }
        else if (arg == 'o') {
            strcpy(output_filename, optarg);
            output_filename_set = 1;
        }
        else {
            usage();
            exit(EXIT_FAILURE);
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

    char *dot = strstr(acirc_filename, ".acirc");
    if (dot == NULL) {
        fprintf(stderr, "[obfuscate] error: unknown circuit format \"%s\"", acirc_filename);
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

    if (!output_filename_set) {
        char prefix[1024];
        memcpy(prefix, acirc_filename, dot - acirc_filename);
        prefix[dot - acirc_filename] = '\0';
        sprintf(output_filename, "%s.%lu.zim", prefix, lambda);
    }
    FILE *obf_fp = fopen(output_filename, "w");
    obfuscation_write(obf_fp, obf);

    acirc_destroy(c);
    aes_randclear(rng);
    secret_params_destroy(sp);
    obfuscation_destroy(obf);
}
