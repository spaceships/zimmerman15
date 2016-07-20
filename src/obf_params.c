#include "obf_params.h"

#include <assert.h>

void obf_params_init (obf_params *op, char* acirc_filename, int fake)
{
    op->c = zim_malloc(sizeof(acirc));
    acirc_init(op->c);
    acirc_parse(op->c, acirc_filename);

    op->delta = acirc_max_const_degree(op->c);
    for (size_t i = 0; i < op->c->ninputs; i++) {
        op->delta += acirc_max_var_degree(op->c, i);
    }

    op->fake = fake;
}

void obf_params_clear (obf_params *op)
{
    acirc_clear(op->c);
    free(op->c);
}

void obf_params_write (FILE *fp, const obf_params *op)
{
    ulong_write(fp, op->delta);
    PUT_NEWLINE(fp);
    int_write(fp, op->fake);
}

void obf_params_read (obf_params *op, char *acirc_filename, FILE *fp)
{
    op->c = zim_malloc(sizeof(acirc));
    acirc_init(op->c);
    acirc_parse(op->c, acirc_filename);
    GET_NEWLINE(fp);
    ulong_read(&(op->delta), fp);
    GET_NEWLINE(fp);
    assert(fscanf(fp, "%d", &(op->fake)) > 0);
}
