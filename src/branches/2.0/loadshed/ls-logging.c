/*
 * Copyright (c) 2007 Universitat Politecnica de Catalunya
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 * * Neither the name of Intel Corporation nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

/* #define LS_LOGGING */

#ifndef LS_LOGGING
#define log_prederr_start(x)
#define log_prederr_line(x, y, z)
#define log_prederr_end()
#define log_global_ls_values_pre1(a, b, c, d, e)
#define log_global_ls_values_pre2(a, b)
#define log_global_ls_values_post(x)
#define log_feats(x)
#else

#include <math.h>

#define PREDERR_FILE "/tmp/como_prederr.log"
#define FEATS_FILE "/tmp/como_feats.log"
#define GLOBAL_FILE "/tmp/como_lsglobals.log"

static FILE *prederr_f = NULL;
static FILE *feats_f = NULL;
static FILE *global_f = NULL;

static void UNUSED
log_prederr_start(array_t *mdls)
{
    int j;

    if (prederr_f != NULL)
        return;

    prederr_f = fopen(PREDERR_FILE, "w");
    if (prederr_f == NULL)
        error("cannot open " PREDERR_FILE " for writing\n");

    for (j = 0; j < mdls->len; j++) {
        mdl_t *mdl = array_at(mdls, mdl_t *, j);
        fprintf(prederr_f, "%s%s_pred\t%s_actual\t%s_actual_scaled"
                "\t%s_err\t%s_npkts\t%s_srate", j == 0 ? "" : "\t",
                mdl->name, mdl->name, mdl->name, mdl->name, mdl->name,
                mdl->name);
    }
    fprintf(prederr_f, "\n");
}

static void UNUSED
log_prederr_line(batch_t *batch, mdl_icapture_t *ic, int idx)
{
    float err = fabs(1 - (double)ic->ls.last_pred * ic->ls.srate /
              (double) ic->ls.prof->tsc_cycles->value);
    if (ic->ls.srate == 0)
        err = 0;

    fprintf(prederr_f, "%s%f\t%llu\t%llu\t%f\t%d\t%f", idx == 0 ? "" : "\t",
            (double)ic->ls.last_pred * ic->ls.srate,
            ic->ls.prof->tsc_cycles->value,
            (uint64_t)((double)ic->ls.prof->tsc_cycles->value / ic->ls.srate),
            err,
            batch->count,
            ic->ls.srate);
}

static void UNUSED
log_prederr_end(void)
{
    fprintf(prederr_f, "\n");
}

static void UNUSED
log_global_ls_values_pre1(uint64_t ca_overhead, uint64_t select_cycles,
    uint64_t mdl_cycles, uint64_t avail_cycles, uint64_t orig_avail_cycles)
{
    if (global_f == NULL) { /* open output file and write header */
        global_f = fopen(GLOBAL_FILE, "w");
        if (global_f == NULL)
            error("cannot open " GLOBAL_FILE " for writing\n");

        fprintf(global_f, "ca_overhead\tselect_cycles\tmdl_cycles"
                "\tavail_cycles\torig_avail_cycles\tsrate\tshed_ewma\n");
    }

    fprintf(global_f, "%llu\t%llu\t%llu\t%llu\t%llu", ca_overhead, select_cycles,
            mdl_cycles, avail_cycles, orig_avail_cycles);
}

static void UNUSED
log_global_ls_values_pre2(double srate, double shed_ewma)
{
    fprintf(global_f, "\t%f\t%f\n", srate, shed_ewma);
}

static void UNUSED
log_global_ls_values_post(void)
{

}

static void UNUSED
log_feats(feat_t *feats)
{
    int i;

    if (feats_f == NULL) { /* open output file and write header */
        feats_f = fopen(FEATS_FILE, "w");
        if (feats_f == NULL)
            error("cannot open " FEATS_FILE " for writing\n");

        for (i = 0; i < NUM_FEATS; i++)
            fprintf(feats_f, "%s%s", i == 0 ? "" : "\t", feats[i].name);
        fprintf(feats_f, "\n");
    }

    /* log fature values */
    for (i = 0; i < NUM_FEATS; i++)
        fprintf(feats_f, "%s%f", i == 0 ? "" : "\t", feats[i].value);
    fprintf(feats_f, "\n");
}

#endif

