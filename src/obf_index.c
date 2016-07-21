#include "obf_index.h"

#include <assert.h>

void obf_index_init (obf_index *ix, size_t n)
{
    ix->n = n;
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

obf_index* obf_index_create (size_t n)
{
    obf_index *ix = zim_malloc(sizeof(obf_index));
    obf_index_init(ix, n);
    return ix;
}

obf_index* obf_index_copy (const obf_index *ix)
{
    obf_index *new = zim_malloc(sizeof(obf_index));
    obf_index_init(new, ix->n);
    obf_index_set(new, ix);
    return new;
}

void obf_index_add (obf_index *rop, obf_index *x, obf_index *y)
{
    assert((x->nzs == y->nzs) == rop->nzs);
    ARRAY_ADD(rop->pows, x->pows, y->pows, rop->nzs);
}

void obf_index_pow (obf_index *rop, obf_index *x, ul pow)
{
    assert(x->nzs == rop->nzs);
    if (pow == 0) {
        free(rop->pows);
        rop->pows = zim_calloc(rop->nzs, sizeof(ul));
    } else {
        for (ul i = 1; i < pow; i++) {
            ARRAY_ADD(rop->pows, x->pows, x->pows, rop->nzs);
        }
    }
}

void obf_index_set (obf_index *rop, const obf_index *x)
{
    assert(rop->nzs == x->nzs);
    for (int i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

bool obf_index_eq (const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    return ARRAY_EQ(x->pows, y->pows, x->nzs);
}

void obf_index_print (obf_index *ix)
{
    array_print_ui(ix->pows, ix->nzs);
    puts("");
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

obf_index* obf_index_create_toplevel (acirc *c)
{
    obf_index *ix = obf_index_create(c->ninputs);
    IX_Y(ix) = acirc_max_const_degree(c);
    for (size_t i = 0; i < ix->n; i++) {
        size_t d = acirc_max_var_degree(c, i);
        IX_X(ix, i, 0) = d;
        IX_X(ix, i, 1) = d;
        IX_Z(ix, i) = 1;
        IX_W(ix, i) = 1;
    }
    return ix;
}

obf_index* obf_index_create_x (size_t n, size_t i, size_t b)
{
    obf_index *ix = obf_index_create(n);
    IX_X(ix, i, b) = 1;
    return ix;
}

obf_index* obf_index_create_y (size_t n)
{
    obf_index *ix = obf_index_create(n);
    IX_Y(ix) = 1;
    return ix;
}

obf_index* obf_index_create_z (size_t n, size_t i)
{
    obf_index *ix = obf_index_create(n);
    IX_Z(ix, i) = 1;
    return ix;
}

obf_index* obf_index_create_w (size_t n, size_t i)
{
    obf_index *ix = obf_index_create(n);
    IX_W(ix, i) = 1;
    return ix;
}
