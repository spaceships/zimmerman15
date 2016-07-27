#include "evaluator.h"

#include "mmap.h"
#include <assert.h>

// statefully raise encodings to the union of their indices
static void raise_encodings (encoding *x, encoding *y, obfuscation *obf);
static void raise_encoding  (encoding *x, obf_index *target, obfuscation *obf);

void evaluate (int *rop, acirc *c, int *inputs, obfuscation *obf)
{
    int known [c->nrefs];
    int mine  [c->nrefs]; // whether this function allocated the encoding
    encoding *cache [c->nrefs];

    for (size_t i = 0; i < c->nrefs; i++) {
        known[i] = 0;
        mine[i]  = 0;
        cache[i] = NULL;
    }

    for (int k = 0; k < obf->noutputs; k++) {
        print_progress(k, obf->noutputs);

        acircref root = c->outrefs[k];

        acirc_topo_levels *topo = acirc_topological_levels(c, root);

        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            #pragma omp parallel for
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {

                acircref ref = topo->levels[lvl][i];
                if (known[ref])
                    continue;

                acirc_operation op = c->ops[ref];
                acircref *args     = c->args[ref];

                encoding *res;

                if (op == XINPUT) {
                    size_t xid = args[0];
                    res = obf->xhat[xid][inputs[xid]];
                    mine[ref] = 0;
                }

                else if (op == YINPUT) {
                    size_t yid = args[0];
                    res = obf->yhat[yid];
                    mine[ref] = 0;
                }

                else { // else op is some kind of gate, allocate the result
                    res = encoding_create(c->ninputs, obf->pp->fake);
                    mine[ref] = 1;

                    encoding *x = cache[args[0]]; // which exist due to topo order
                    encoding *y = cache[args[1]];

                    assert(x != NULL);
                    assert(y != NULL);

                    if (op == MUL) {
                        encoding_mul(res, x, y, obf->pp);
                    }

                    else {
                        encoding *tmp_x = encoding_copy(x);
                        encoding *tmp_y = encoding_copy(y);
                        raise_encodings(tmp_x, tmp_y, obf);

                        if (op == ADD) {
                            encoding_add(res, tmp_x, tmp_y, obf->pp);
                        }
                        else if (op == SUB) {
                            encoding_sub(res, tmp_x, tmp_y, obf->pp);
                        }

                        encoding_destroy(tmp_x);
                        encoding_destroy(tmp_y);
                    }
                }

                known[ref] = 1;
                cache[ref] = res;
            }
        }
        acirc_topo_levels_destroy(topo);
        encoding *outwire = encoding_copy(cache[root]);
        encoding *tmp     = encoding_copy(obf->Chatstar[k]);

        for (size_t i = 0; i < c->ninputs; i++) {
            encoding_mul(outwire, outwire, obf->zhat[i][inputs[i]][k], obf->pp);
        }

        for (size_t i = 0; i < c->ninputs; i++) {
            encoding_mul(tmp, tmp, obf->what[i][inputs[i]][k], obf->pp);
        }

        if (!obf_index_eq(obf->pp->toplevel, tmp->index)) {
            printf("\n[evaluate] tmp did not reach toplevel\n");
            obf_index_print(tmp->index);
            obf_index_print(obf->pp->toplevel);
            exit(EXIT_FAILURE);
        }
        if (!obf_index_eq(obf->pp->toplevel, outwire->index)) {
            printf("\n[evaluate] outwire did not reach toplevel\n");
            obf_index_print(outwire->index);
            obf_index_print(obf->pp->toplevel);
            exit(EXIT_FAILURE);
        }

        encoding_sub(outwire, outwire, tmp, obf->pp);
        rop[k] = !encoding_is_zero(outwire, obf->pp);

        encoding_destroy(outwire);
        encoding_destroy(tmp);
    }

    print_progress(obf->noutputs, obf->noutputs);
    puts("");

    for (acircref x = 0; x < c->nrefs; x++) {
        if (known[x] && mine[x]) {
            encoding_destroy(cache[x]);
        }
    }
}

// statefully raise encodings to the union of their indices
static void raise_encodings (encoding *x, encoding *y, obfuscation *obf)
{
    obf_index *target = obf_index_union(x->index, y->index);
    raise_encoding(x, target, obf);
    raise_encoding(y, target, obf);
    obf_index_destroy(target);
}

static void raise_encoding (encoding *x, obf_index *target, obfuscation *obf)
{
    obf_index *diff = obf_index_difference(target, x->index);
    for (size_t i = 0; i < obf->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            for (size_t p = 0; p < IX_X(diff, i, b); p++)
                encoding_mul(x, x, obf->uhat[i][b], obf->pp);
        }
    }
    for (size_t p = 0; p < IX_Y(diff); p++)
        encoding_mul(x, x, obf->vhat, obf->pp);
    obf_index_destroy(diff);
}
