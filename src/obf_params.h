#ifndef __ZIMMERMAN_OBF_PARAMS__
#define __ZIMMERMAN_OBF_PARAMS__

#include "util.h"
#include <acirc.h>
#include <stdio.h>

typedef struct {
    acirc* c;
    ul delta;
    int fake;
} obf_params;

void obf_params_init  (obf_params *op, char *acirc_filename, int fake);
void obf_params_clear (obf_params *op);

void obf_params_write (FILE *fp, const obf_params *op);
void obf_params_read  (obf_params *op, char *acirc_filename, FILE *fp);

#endif
