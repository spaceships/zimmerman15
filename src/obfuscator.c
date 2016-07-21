#include "obfuscator.h"

obfuscation* obfuscate (acirc *c, secret_params *sp, aes_randstate_t rng)
{
    obfuscation *obf = zim_malloc(sizeof(obfuscation));
    int n = obf->ninputs  = c->ninputs;
    int m = obf->nconsts  = c->nconsts;
    int o = obf->noutputs = c->noutputs;

    mpz_t zero, one;
    mpz_inits(zero, one, NULL);
    mpz_set_ui(zero, 0);
    mpz_set_ui(one, 1);

    mpz_t alpha [n];
    mpz_t gamma [n][2];
    mpz_t delta [n][2];

    obf->xhat = zim_malloc(n * sizeof(mpz_t*));
    obf->uhat = zim_malloc(n * sizeof(mpz_t*));
    obf->zhat = zim_malloc(n * sizeof(mpz_t*));
    obf->what = zim_malloc(n * sizeof(mpz_t*));

    for (size_t i = 0; i < n; i++) {
        mpz_inits(alpha[i], gamma[i][0], gamma[i][1], delta[i][0], delta[i][1], NULL);

        mpz_randomm_inv_aes(alpha[i],    rng, get_moduli(sp)[1]);
        mpz_randomm_inv_aes(gamma[i][0], rng, get_moduli(sp)[1]);
        mpz_randomm_inv_aes(gamma[i][1], rng, get_moduli(sp)[1]);
        mpz_randomm_inv_aes(delta[i][0], rng, get_moduli(sp)[0]);
        mpz_randomm_inv_aes(delta[i][1], rng, get_moduli(sp)[0]);

        obf->xhat[i] = zim_malloc(2 * sizeof(encoding*));
        obf->uhat[i] = zim_malloc(2 * sizeof(encoding*));
        obf->zhat[i] = zim_malloc(2 * sizeof(encoding*));
        obf->what[i] = zim_malloc(2 * sizeof(encoding*));

        ul d = acirc_max_var_degree(c, i); // used for creating the zhat encodings

        for (ul b = 0; b <= 1; b++) {
            mpz_t b_mpz;
            mpz_init_set_ui(b_mpz, b);

            // create the xhat and uhat encodings
            obf_index *ix_x = obf_index_create_x(n, i, b);
            obf->xhat[i][b] = encode(b_mpz, alpha[i], ix_x, sp, rng);
            obf->uhat[i][b] = encode(one,   one,      ix_x, sp, rng);
            obf_index_destroy(ix_x);

            // create the zhat encodings
            obf_index *ix_z = obf_index_create(n);
            IX_X(ix_z, i, 1-b) = 1;
            obf_index_pow(ix_z, ix_z, d);
            IX_Z(ix_z, i) = 1;
            IX_W(ix_z, i) = 1;
            obf->zhat[i][b] = encode(delta[i][b], gamma[i][b], ix_z, sp, rng);
            obf_index_destroy(ix_z);

            // create the what encodings
            obf_index *ix_w = obf_index_create_w(n, i);
            obf->what[i][b] = encode(zero, gamma[i][b], ix_w, sp, rng);
            obf_index_destroy(ix_w);

            mpz_clear(b_mpz);
        }
    }

    // create the yhat and vhat encodings
    mpz_t beta [m];
    mpz_t y;
    mpz_init(y);
    obf_index *ix_y = obf_index_create_y(n);
    obf->yhat = zim_malloc(m * sizeof(encoding*));
    for (size_t j = 0; j < m; j++) {
        mpz_init(beta[j]);
        mpz_randomm_inv_aes(beta[j], rng, get_moduli(sp)[1]);

        mpz_set_ui(y, c->consts[j]);
        obf->yhat[j] = encode(y, beta[j], ix_y, sp, rng);
    }
    mpz_clear(y);
    obf->vhat = encode(one, one, ix_y, sp, rng); // set vhat
    obf_index_destroy(ix_y);

    mpz_t Cstar [o];
    obf->Chatstar = zim_malloc(o * sizeof(encoding*));
    for (size_t k = 0; k < o; k++) {
        mpz_init(Cstar[k]);
        acirc_eval_mpz_mod(Cstar[k], c, c->outrefs[k], alpha, beta, get_moduli(sp)[1]);

        obf_index *ix_c = obf_index_create(n);
        IX_Y(ix_c) = acirc_const_degree(c, k);

        for (size_t i = 0; i < n; i++) {
            ul d = acirc_var_degree(c, k, i);
            IX_X(ix_c, i, 0) = d;
            IX_X(ix_c, i, 1) = d;
            IX_Z(ix_c, i) = 1;
        }

        obf->Chatstar[k] = encode(zero, Cstar[k], ix_c, sp, rng);
        obf_index_destroy(ix_c);
    }

    // cleanup
    mpz_clears(zero, one, NULL);

    for (size_t i = 0; i < n; i++)
        mpz_clears(alpha[i], gamma[i][0], gamma[i][1], delta[i][0], delta[i][1], NULL);
    for (size_t j = 0; j < m; j++)
        mpz_clear(beta[j]);
    for (size_t k = 0; k < o; k++)
        mpz_clear(Cstar[k]);

    return obf;
}

void obfuscation_destroy (obfuscation *obf)
{
    for (size_t i = 0; i < obf->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            encoding_destroy(obf->xhat[i][b]);
            encoding_destroy(obf->uhat[i][b]);
            encoding_destroy(obf->zhat[i][b]);
            encoding_destroy(obf->what[i][b]);
        }
        free(obf->xhat[i]);
        free(obf->uhat[i]);
        free(obf->zhat[i]);
        free(obf->what[i]);
    }
    free(obf->xhat);
    free(obf->uhat);
    free(obf->zhat);
    free(obf->what);
    for (size_t j = 0; j < obf->noutputs; j++) {
        encoding_destroy(obf->yhat[j]);
    }
    free(obf->yhat);
    encoding_destroy(obf->vhat);
    for (size_t i = 0; i < obf->noutputs; i++) {
        encoding_destroy(obf->Chatstar[i]);
    }
    free(obf->Chatstar);

    free(obf);
}

void obfuscation_write (FILE *fp, obfuscation *obf);
void obfuscation_read  (obfuscation *obf, FILE *fp);
