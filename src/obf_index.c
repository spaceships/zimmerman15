#include "obf_index.h"

#include <assert.h>

static void obf_index_init (obf_index *ix, size_t n)
{
    ix->n = n;
    ix->nzs = 4 * ix->n + 1;
    ix->pows = zim_calloc(ix->nzs, sizeof(ul));
}

obf_index* obf_index_create (size_t n)
{
    obf_index *ix = zim_calloc(1, sizeof(obf_index));
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

void obf_index_destroy (obf_index *ix)
{
    free(ix->pows);
    free(ix);
}

////////////////////////////////////////////////////////////////////////////////

void obf_index_add (obf_index *rop, obf_index *x, obf_index *y)
{
    assert(x->nzs == y->nzs);
    assert(y->nzs == rop->nzs);
    ARRAY_ADD(rop->pows, x->pows, y->pows, rop->nzs);
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

////////////////////////////////////////////////////////////////////////////////

obf_index* obf_index_union (obf_index *x, obf_index *y)
{
    assert(x->nzs == y->nzs);
    obf_index *res = obf_index_create(x->n);
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = MAX(x->pows[i], y->pows[i]);
    }
    return res;
}

obf_index* obf_index_difference (obf_index *x, obf_index *y)
{
    assert(x->nzs == y->nzs);
    obf_index *res = obf_index_create(x->n);
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = x->pows[i] - y->pows[i];
        assert(res->pows[i] >= 0);
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////

void obf_index_print (obf_index *ix)
{
    puts("=obf_index=");
    array_print_ui(ix->pows, ix->nzs);
    puts("");
    printf("n=%lu nzs=%lu\n", ix->n, ix->nzs);
}

obf_index *obf_index_read (FILE *fp)
{
    obf_index *ix = zim_calloc(1, sizeof(obf_index));
    if (ulong_read(&(ix->nzs), fp) || GET_SPACE(fp)) {
        fprintf(stderr, "[%s] failed to read nzs!\n", __func__);
        obf_index_destroy(ix);
        return NULL;
    }
    if (ulong_read(&(ix->n), fp) || GET_SPACE(fp)) {
        fprintf(stderr, "[%s] failed to read n!\n", __func__);
        obf_index_destroy(ix);
        return NULL;
    }
    ix->pows = zim_malloc(ix->nzs * sizeof(ul));
    for (size_t i = 0; i < ix->nzs; i++) {
        if (ulong_read(&(ix->pows[i]), fp) || GET_SPACE(fp)) {
            fprintf(stderr, "[%s] failed to read n!\n", __func__);
            obf_index_destroy(ix);
            return NULL;
        }
    }
    return ix;
}

int obf_index_write (FILE *fp, obf_index *ix)
{
    if (ulong_write(fp, ix->nzs) || PUT_SPACE(fp)) {
        fprintf(stderr, "[%s] failed to write nzs!\n", __func__);
        return 1;
    }
    if (ulong_write(fp, ix->n) || PUT_SPACE(fp)) {
        fprintf(stderr, "[%s] failed to write nzs!\n", __func__);
        return 1;
    }    
    for (size_t i = 0; i < ix->nzs; i++) {
        if (ulong_write(fp, ix->pows[i]) || PUT_SPACE(fp)) {
            fprintf(stderr, "[%s] failed to write pows!\n", __func__);
            return 1;
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

obf_index* obf_index_create_toplevel (acirc *c)
{
    obf_index *ix = obf_index_create(c->ninputs);
    IX_Y(ix) = acirc_max_const_degree(c);
#pragma omp parallel for
    for (size_t i = 0; i < ix->n; i++) {
        size_t d = acirc_max_var_degree(c, i);
        IX_X(ix, i, 0) = d;
        IX_X(ix, i, 1) = d;
        IX_Z(ix, i) = 1;
        IX_W(ix, i) = 1;
    }
    return ix;
}
