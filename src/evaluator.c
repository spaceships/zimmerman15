#include "evaluator.h"

#include "mmap.h"
#include <threadpool.h>
#include <assert.h>
#include <pthread.h>

typedef struct ref_list_node {
    acircref ref;
    struct ref_list_node *next;
} ref_list_node;

typedef struct {
    ref_list_node *first;
    pthread_mutex_t *lock;
} ref_list;

ref_list* ref_list_create ();
void ref_list_destroy (ref_list *list);
void ref_list_push (ref_list *list, acircref ref);

void get_dependents (ref_list **deps, acirc *c);

// statefully raise encodings to the union of their indices
static void raise_encodings (encoding *x, encoding *y, obfuscation *obf);
static void raise_encoding  (encoding *x, obf_index *target, obfuscation *obf);

////////////////////////////////////////////////////////////////////////////////

typedef struct work_args {
    acircref ref;
    acirc *c;
    int *inputs;
    obfuscation *obf;
    int *known;
    int *mine;
    int *ready;
    encoding **cache;
    ref_list **dependents;
    threadpool *pool;
} work_args;

typedef struct finish_args {
    acirc *c;
    int *inputs;
    obfuscation *obf;
    encoding **cache;
    size_t k;
    int *rop;
} finish_args;

void obf_eval_ref (void* wargs);
void obf_finish (void* fargs);

////////////////////////////////////////////////////////////////////////////////

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

    ref_list* dependents [c->nrefs];
    int ready [c->nrefs];
    for (size_t i = 0; i < c->nrefs; i++) {
        dependents[i] = ref_list_create();
        ready[i] = 0;
    }
    get_dependents(dependents, c);

    threadpool *pool = threadpool_create(THREADPOOL_NCORES);
    pthread_mutex_t deps_lock;
    pthread_mutex_init(&deps_lock, NULL);

    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->ops[ref];
        if (!(op == XINPUT || op == YINPUT)) {
            continue;
        }
        work_args *args = zim_malloc(sizeof(work_args));
        args->ref = ref;
        args->c = c;
        args->inputs = inputs;
        args->obf = obf;
        args->known = known;
        args->mine = mine;
        args->ready = ready;
        args->cache = cache;
        args->dependents = dependents;
        args->pool = pool;
        threadpool_add_job(pool, obf_eval_ref, args);
    }

    pthread_mutex_destroy(&deps_lock);
    threadpool_destroy(pool); // wait for everyone to finish, could replace with a barrier

    pool = threadpool_create(THREADPOOL_NCORES);
    for (size_t k = 0; k < obf->noutputs; k++) {
        finish_args *fargs = zim_malloc(sizeof(finish_args));
        fargs->c = c;
        fargs->inputs = inputs;
        fargs->obf = obf;
        fargs->cache = cache;
        fargs->k = k;
        fargs->rop = rop;
        threadpool_add_job(pool, obf_finish, fargs);
    }
    threadpool_destroy(pool);

    for (size_t i = 0; i < c->nrefs; i++) {
        ref_list_destroy(dependents[i]);
    }

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

////////////////////////////////////////////////////////////////////////////////

ref_list* ref_list_create ()
{
    ref_list *list = zim_malloc(sizeof(ref_list));
    list->first = NULL;
    list->lock = zim_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(list->lock, NULL);
    return list;
}

void ref_list_destroy (ref_list *list)
{
    ref_list_node *cur = list->first;
    while (cur != NULL) {
        ref_list_node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    pthread_mutex_destroy(list->lock);
    free(list->lock);
    free(list);
}

static ref_list_node* ref_list_node_create (acircref ref)
{
    ref_list_node *new = zim_malloc(sizeof(ref_list_node));
    new->next = NULL;
    new->ref  = ref;
    return new;
}

void ref_list_push (ref_list *list, acircref ref)
{
    ref_list_node *cur = list->first;
    if (cur == NULL) {
        list->first = ref_list_node_create(ref);
        return;
    }
    while (1) {
        if (cur->ref == ref) {
            return;
        }
        if (cur->next == NULL) {
            cur->next = ref_list_node_create(ref);
            return;
        }
        cur = cur->next;
    }
}

void get_dependents (ref_list **deps, acirc *c)
{
    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->ops[ref];
        if (op == XINPUT || op == YINPUT) {
            continue;
        }

        acircref x = c->args[ref][0];
        acircref y = c->args[ref][1];

        ref_list_push(deps[x], ref);
        ref_list_push(deps[y], ref);
    }
}

void obf_eval_ref(void* wargs)
{
    acircref ref     = ((work_args*)wargs)->ref;
    acirc *c         = ((work_args*)wargs)->c;
    int *inputs      = ((work_args*)wargs)->inputs;
    obfuscation *obf = ((work_args*)wargs)->obf;
    int *known       = ((work_args*)wargs)->known;
    int *mine        = ((work_args*)wargs)->mine;
    int *ready       = ((work_args*)wargs)->ready;
    encoding **cache = ((work_args*)wargs)->cache;
    ref_list **deps  = ((work_args*)wargs)->dependents;
    threadpool *pool = ((work_args*)wargs)->pool;

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

    ref_list_node *cur = deps[ref]->first;
    while (cur != NULL) {
        pthread_mutex_lock(deps[cur->ref]->lock);
        ready[cur->ref] += 1;
        if (ready[cur->ref] == 2) {
            work_args *newargs = zim_malloc(sizeof(work_args));
            *newargs = *(work_args*)wargs;
            newargs->ref = cur->ref;
            threadpool_add_job(pool, obf_eval_ref, (void*)newargs);
        }
        pthread_mutex_unlock(deps[cur->ref]->lock);
        cur = cur->next;
    }

    free((work_args*)wargs);
}


void obf_finish (void* fargs)
{
    acirc *c         = ((finish_args*)fargs)->c;
    int *inputs      = ((finish_args*)fargs)->inputs;
    obfuscation *obf = ((finish_args*)fargs)->obf;
    encoding **cache = ((finish_args*)fargs)->cache;
    size_t k         = ((finish_args*)fargs)->k;
    int *rop         = ((finish_args*)fargs)->rop;

    acircref root = c->outrefs[k];

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

    free((finish_args*)fargs);
}
