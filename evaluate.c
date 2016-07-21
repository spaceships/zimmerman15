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
    int output_filename_set = 0;
    char output_filename [1024];
    int arg;
    while ((arg = getopt(argc, argv, "l:o:")) != -1) {
        if (arg == 'l') {
            lambda = atol(optarg);
        }
        else if (arg == 'o') {
            strcpy(output_filename, optarg);
            output_filename_set = 1;
        } else {
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

    ////////////////////////////////////////////////////////////////////////////////
    // lets do this

    acirc *c = acirc_from_file(acirc_filename);

    if (!output_filename_set)
        sprintf(output_filename, "%s.%lu.zim", acirc_filename, lambda);
    FILE *obf_fp = fopen(output_filename, "r");
    obfuscation *obf = obfuscation_read(obf_fp);

    int res[c->noutputs];
    for (int i = 0; i < c->ntests; i++) {
        evaluate(res, c->testinps[i], obf);
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
