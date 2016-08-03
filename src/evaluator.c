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

typedef struct work_args {
    acircref ref;
    acirc *c;
    int *inputs;
    obfuscation *obf;
    int *mine;
    int *ready;
    encoding **cache;
    ref_list **deps;
    threadpool *pool;
    int *rop;
} work_args;

static void obf_eval_worker (void* wargs);

static ref_list* ref_list_create ();
static void ref_list_destroy (ref_list *list);
static void ref_list_push    (ref_list *list, acircref ref);
static void raise_encodings  (encoding *x, encoding *y, obfuscation *obf);
static void raise_encoding   (encoding *x, obf_index *target, obfuscation *obf);

////////////////////////////////////////////////////////////////////////////////

void evaluate (int *rop, acirc *c, int *inputs, obfuscation *obf)
{
    encoding *cache [c->nrefs]; // evaluated intermediate nodes
    ref_list* deps [c->nrefs];  // each list contains refs of nodes dependent on this one
    int mine  [c->nrefs];       // whether the evaluator allocated an encoding in cache
    int ready [c->nrefs];       // number of children who have been evaluated already

    for (size_t i = 0; i < c->nrefs; i++) {
        cache[i] = NULL;
        deps [i] = ref_list_create();
        mine [i] = 0;
        ready[i] = 0;
    }

    // populate dependents lists
    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->ops[ref];
        if (op == XINPUT || op == YINPUT)
            continue;
        acircref x = c->args[ref][0];
        acircref y = c->args[ref][1];
        ref_list_push(deps[x], ref);
        ref_list_push(deps[y], ref);
    }

    threadpool *pool = threadpool_create(NCORES);

    // start threads evaluating the circuit inputs- they will signal their
    // parents to start, recursively, until the output is reached.
    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->ops[ref];
        if (!(op == XINPUT || op == YINPUT)) {
            continue;
        }
        // allocate each argstruct here, otherwise we will overwrite
        // it each time we add to the job list. The worker will free.
        work_args *args = zim_malloc(sizeof(work_args));
        args->ref    = ref;
        args->c      = c;
        args->inputs = inputs;
        args->obf    = obf;
        args->mine   = mine;
        args->ready  = ready;
        args->cache  = cache;
        args->deps   = deps;
        args->pool   = pool;
        args->rop    = rop;
        threadpool_add_job(pool, obf_eval_worker, args);
    }

    // threadpool_destroy waits for all the jobs to finish
    threadpool_destroy(pool);

    // cleanup
    for (size_t i = 0; i < c->nrefs; i++) {
        ref_list_destroy(deps[i]);
        if (mine[i]) {
            encoding_destroy(cache[i]);
        }
    }
}

void obf_eval_worker(void* wargs)
{
    acircref ref     = ((work_args*)wargs)->ref; // the particular ref to evaluate right now
    acirc *c         = ((work_args*)wargs)->c;
    int *inputs      = ((work_args*)wargs)->inputs;
    obfuscation *obf = ((work_args*)wargs)->obf;
    int *mine        = ((work_args*)wargs)->mine;
    int *ready       = ((work_args*)wargs)->ready;
    encoding **cache = ((work_args*)wargs)->cache;
    ref_list **deps  = ((work_args*)wargs)->deps;
    threadpool *pool = ((work_args*)wargs)->pool;
    int *rop         = ((work_args*)wargs)->rop;

    acirc_operation op = c->ops[ref];
    acircref *args     = c->args[ref];
    encoding *res;

    // if the ref is input or const, return the approprite encoding from the obfuscation
    if (op == XINPUT) {
        size_t xid = args[0];
        res = obf->xhat[xid][inputs[xid]];
        mine[ref] = 0; // the evaluator didn't allocate this encoding
    }
    else if (op == YINPUT) {
        size_t yid = args[0];
        res = obf->yhat[yid];
        mine[ref] = 0;
    }

    // otherwise the ref is some kind of gate: allocate the encoding & eval
    else {
        res = encoding_create(c->ninputs, obf->pp->fake);
        mine[ref] = 1; // the evaluator allocated this encoding

        // the encodings of the args exist since the ref's children signalled it
        encoding *x = cache[args[0]];
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

    // set the result in the cache
    cache[ref] = res;

    // signal parents that this ref is done
    ref_list_node *cur = deps[ref]->first;
    while (cur != NULL) {
        pthread_mutex_lock(deps[cur->ref]->lock);
        ready[cur->ref] += 1; // ready[ref] indicates how many of ref's children are evaluated
        if (ready[cur->ref] == 2) {
            work_args *newargs = zim_malloc(sizeof(work_args));
            *newargs = *(work_args*)wargs;
            newargs->ref = cur->ref;
            threadpool_add_job(pool, obf_eval_worker, (void*)newargs);
        }
        pthread_mutex_unlock(deps[cur->ref]->lock);
        cur = cur->next;
    }
    free((work_args*)wargs);

    // addendum: is this ref an output bit? if so, we should zero test it.
    int k;
    int ref_is_output = 0;
    for (size_t i = 0; i < c->noutputs; i++) {
        if (ref == c->outrefs[i]) {
            k = i;
            ref_is_output = 1;
            break;
        }
    }
    if (ref_is_output) {
        encoding *outwire = encoding_copy(res);
        encoding *tmp     = encoding_copy(obf->Chatstar[k]);

        for (size_t i = 0; i < c->ninputs; i++)
            encoding_mul(outwire, outwire, obf->zhat[i][inputs[i]][k], obf->pp);
        for (size_t i = 0; i < c->ninputs; i++)
            encoding_mul(tmp, tmp, obf->what[i][inputs[i]][k], obf->pp);

        assert(obf_index_eq(obf->pp->toplevel, tmp->index));
        assert(obf_index_eq(obf->pp->toplevel, outwire->index));

        encoding_sub(outwire, outwire, tmp, obf->pp);
        rop[k] = !encoding_is_zero(outwire, obf->pp);

        encoding_destroy(outwire);
        encoding_destroy(tmp);
    }
}

////////////////////////////////////////////////////////////////////////////////
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
// ref list utils

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
