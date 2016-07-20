#ifndef __ZIMMERMAN_OBF_INDEX__
#define __ZIMMERMAN_OBF_INDEX__

#include "obf_params.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

typedef struct {
    ul *pows;
    size_t nzs;
    size_t n;       // number of inputs to the circuit
} obf_index;

void obf_index_init    (obf_index *ix, obf_params *op);
void obf_index_clear   (obf_index *ix);
void obf_index_destroy (obf_index *ix);

void obf_index_add (obf_index *rop, obf_index *x, obf_index *y);
void obf_index_set (obf_index *rop, const obf_index *x);
bool obf_index_eq  (const obf_index *x, const obf_index *y);

void obf_index_print (obf_index *ix);
void obf_index_read  (obf_index *ix, FILE *fp);
void obf_index_write (FILE *fp, obf_index *ix);

obf_index* obf_index_create_toplevel (obf_params *p);

#endif