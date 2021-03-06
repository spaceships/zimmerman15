#include "mmap.h"
#include "obfuscator.h"

#include <aesrand.h>
#include <assert.h>
#include <acirc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <mmap/mmap_clt.h>
#include <mmap/mmap_dummy.h>

void usage()
{
    printf("Usage: obfuscate [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-l\tScurity parameter (default=10).\n");
    printf("\t-f\tUse fake multilinear map for testing.\n");
    printf("\t-o\tSpecify obfuscation output file.\n");
    printf("\t-p\tSpecify how many powers of 2 to to give out for u_i's and v (default=8).\n");
    puts("");
}

int main (int argc, char **argv)
{
    ul lambda = 10;
    ul npowers = 8;
    int output_filename_set = 0;
    char output_filename [1024];
    int arg;
    int fake = 0;
    const mmap_vtable *mmap = &clt_vtable;
    while ((arg = getopt(argc, argv, "fl:o:p:")) != -1) {
        if (arg == 'f') {
            mmap = &dummy_vtable;
            fake = 1;
        }
        else if (arg == 'l') {
            lambda = atol(optarg);
        }
        else if (arg == 'o') {
            strcpy(output_filename, optarg);
            output_filename_set = 1;
        }
        else if (arg == 'p') {
            npowers = atoi(optarg);
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
        exit(EXIT_FAILURE);
    } else if (optind == argc - 1) {
        acirc_filename = argv[optind];
    } else {
        fprintf(stderr, "[obfuscate] error: unexpected argument \"%s\"\n", argv[optind + 1]);
        usage();
        exit(EXIT_FAILURE);
    }

    char *dot = strstr(acirc_filename, ".acirc");
    if (dot == NULL) {
        fprintf(stderr, "[obfuscate] error: unknown circuit format \"%s\"\n", acirc_filename);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // all right, lets get to it!

    acirc *c = acirc_from_file(acirc_filename);
    size_t delta = acirc_delta(c);

    printf("// circuit: ninputs=%lu noutputs=%lu nconsts=%lu ngates=%lu nrefs=%lu delta=%lu\n",
           c->ninputs, c->noutputs, c->nconsts, c->ngates, c->nrefs, delta);

    printf("// obfuscation: lambda=%lu kappa=%lu npowers=%lu\n",
           lambda, delta + 2*c->ninputs, npowers);

    aes_randstate_t rng;
    aes_randinit(rng);

    puts("initializing secret params...");
    secret_params *sp = secret_params_create(mmap, c, lambda, 0, rng);
    puts("obfuscating...");
    obfuscation *obf = obfuscate(mmap, c, sp, npowers, rng);

    if (!output_filename_set) {
        char prefix[1024];
        memcpy(prefix, acirc_filename, dot - acirc_filename);
        prefix[dot - acirc_filename] = '\0';
        if (fake) {
            sprintf(output_filename, "%s.fake.zim", prefix);
        } else {
            sprintf(output_filename, "%s.%lu.zim", prefix, lambda);
        }
    }
    FILE *obf_fp = fopen(output_filename, "wb");
    if (obf_fp == NULL) {
        fprintf(stderr, "[obfuscate] error: could not open \"%s\"\n", output_filename);
        exit(EXIT_FAILURE);
    }
    obfuscation_write(mmap, obf_fp, obf);
    fclose(obf_fp);

    acirc_destroy(c);
    aes_randclear(rng);
    secret_params_destroy(mmap, sp);
    obfuscation_destroy(mmap, obf);
}
