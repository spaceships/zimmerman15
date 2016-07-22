#include "evaluator.h"
#include "mmap.h"
#include "obfuscator.h"
#include <stdio.h>
#include <unistd.h>

void usage()
{
    printf("Usage: evaluate [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-l\tScurity parameter (default=10).\n");
    printf("\t-o\tSpecify obfuscation input file.\n");
    puts("");
}

int main (int argc, char **argv)
{
    ul lambda = 10;
    int input_filename_set = 0;
    char input_filename [1024];
    int arg;
    while ((arg = getopt(argc, argv, "l:o:")) != -1) {
        if (arg == 'l') {
            lambda = atol(optarg);
        }
        else if (arg == 'o') {
            strcpy(input_filename, optarg);
            input_filename_set = 1;
        } else {
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
    // lets do this

    acirc *c = acirc_from_file(acirc_filename);

    if (!input_filename_set) {
        char prefix[1024];
        memcpy(prefix, acirc_filename, dot - acirc_filename);
        prefix[dot - acirc_filename] = '\0';
        sprintf(input_filename, "%s.%lu.zim", prefix, lambda);
    }
    printf("reading obfuscation from %s\n", input_filename);
    FILE *obf_fp = fopen(input_filename, "r");
    if (obf_fp == NULL) {
        fprintf(stderr, "[obfuscate] error: could not open \"%s\"\n", input_filename);
        exit(EXIT_FAILURE);
    }
    obfuscation *obf = obfuscation_read(obf_fp);
    fclose(obf_fp);

    printf("evaluating...\n");
    int res[c->noutputs];
    for (int i = 0; i < c->ntests; i++) {
        evaluate(res, c, c->testinps[i], obf);
        bool test_ok = ARRAY_EQ(res, c->testouts[i], c->noutputs);
        if (!test_ok)
            printf("\033[1;41m");
        printf("test %d input=", i);
        array_printstring_rev(c->testinps[i], c->ninputs);
        printf(" expected=");
        array_printstring_rev(c->testouts[i], c->noutputs);
        printf(" got=");
        array_printstring_rev(res, c->noutputs);
        if (!test_ok)
            printf("\033[0m");
        puts("");
    }

    acirc_destroy(c);
    obfuscation_destroy(obf);
}
