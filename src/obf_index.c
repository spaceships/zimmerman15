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
    array_add_ui(rop->pows, x->pows, y->pows, rop->nzs);
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
    return array_eq_ui(x->pows, y->pows, x->nzs);
}

void obf_index_print (obf_index *ix)
{
    if (ix->pows[0])
        printf("Y^%lu ", ix->pows[0]);

    int base = 1;
    for (int i = base; i < base + 2*ix->n; i+=2) {
        if (ix->pows[i])
            printf("X_{%lu,0}^%lu ", ix->n - i, ix->pows[i]);
        if (ix->pows[i+1])
            printf("X_{%lu,1}^%lu ", ix->n - i+1, ix->pows[i+1]);
    }

    base += 2*ix->n;
    for (int i = base; i < base + ix->n; i++) {
        if (ix->pows[i])
            printf("Z_%lu ", ix->pows[i]);
    }

    base += ix->n;
    for (int i = base; i < base + ix->n; i++) {
        if (ix->pows[i])
            printf("W_%lu ", ix->pows[i]);
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
