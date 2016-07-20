#include "obf_index.h"

#include <assert.h>

void obf_index_init (obf_index *ix, obf_params *op)
{
    ix->n = op->c->ninputs;
    ix->nzs = 4 * ix->n + 1;
    ix->pows = zim_calloc(ix->nzs, sizeof(ul));
}

void obf_index_clear (obf_index *ix)
{
    free(ix->pows);
}

void obf_index_destroy (obf_index *ix)
{
    obf_index_clear(ix);
    free(ix);
}

void obf_index_add (obf_index *rop, obf_index *x, obf_index *y)
{
    assert((x->nzs == y->nzs) == rop->nzs);
    ARRAY_ADD(rop->pows, x->pows, y->pows, rop->nzs);
}

void obf_index_set (obf_index *rop, const obf_index *x)
{
    assert(rop->nzs == x->nzs);
    for (int i = 0; i < x->nzs; i++)
        rop[i] = x[i];
}

bool obf_index_eq (const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    return ARRAY_EQ(x->pows, y->pows, x->nzs);
}

void obf_index_print (obf_index *ix)
{
    if (IX_Y(ix))
        printf("Y^%lu ", IX_Y(ix));

    for (size_t i = 0; i < ix->n; i++) {
        if (IX_X(ix, i, 0))
            printf("X_{%lu,0}^%lu ", i, IX_X(ix, i, 0));

        if (IX_X(ix, i, 1))
            printf("X_{%lu,1}^%lu ", i, IX_X(ix, i, 1));
    }

    for (size_t i = 0; i < ix->n; i++) {
        if (IX_Z(ix, i))
            printf("Z_%lu ", IX_Z(ix, i));
    }

    for (size_t i = 0; i < ix->n; i++) {
        if (IX_W(ix, i))
            printf("W_%lu ", IX_W(ix, i));
    }
}

void obf_index_read (obf_index *ix, FILE *fp)
{
    ulong_read(&(ix->nzs), fp);
    GET_SPACE(fp);
    ix->pows = zim_malloc(ix->nzs * sizeof(ul));
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_read(&(ix->pows[i]), fp);
        if (i != ix->nzs-1)
            GET_SPACE(fp);
    }
}

void obf_index_write (FILE *fp, obf_index *ix)
{
    ulong_write(fp, ix->nzs);
    PUT_SPACE(fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_write(fp, ix->pows[i]);
        if (i != ix->nzs-1)
            PUT_SPACE(fp);
    }
}

////////////////////////////////////////////////////////////////////////////////

obf_index* obf_index_create_toplevel (obf_params *p)
{
    obf_index *ix = zim_malloc(sizeof(obf_index));
    obf_index_init(ix, p);
    IX_Y(ix) = acirc_max_const_degree(p->c);
    for (size_t i = 0; i < ix->n; i++) {
        size_t d = acirc_max_var_degree(p->c, i);
        IX_X(ix, i, 0) = d;
        IX_X(ix, i, 1) = d;
        IX_Z(ix, i) = 1;
        IX_W(ix, i) = 1;
    }
    return ix;
}