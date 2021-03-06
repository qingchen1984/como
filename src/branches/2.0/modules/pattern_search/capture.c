/*
 * Copyright (c) 2004-2008, Intel Corporation
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
 * $Id: capture.c 1226 2008-01-31 17:52:13Z jsanjuas $
 */

#include <string.h>		/* memcpy */

#include "pattern_search.h"
#include "module.h"
#include "data.h"

void *
ca_init(mdl_t *self, timestamp_t ts)
{
    config_t *cfg = mdl_get_config(self, config_t);
    pattern_search_t *psearch;

    psearch = (pattern_search_t *) mdl_malloc(self, sizeof(pattern_search_t));
    pattern_search_initialize(psearch, cfg->pattern);

    return psearch;
}

void
capture(mdl_t *self, pkt_t *pkt, pattern_search_t *s, double srate)
{
    config_t *cfg;
    record_t *r;
    int len;

    if (! pattern_search(s, COMO(payload), COMO(caplen), NULL))
        return;

    cfg = mdl_get_config(self, config_t);
    r = mdl_alloc_tuple(self, record_t);

    r->ts = COMO(ts);
    len = (COMO(caplen) > cfg->snaplen) ? cfg->snaplen : COMO(caplen);
    memcpy(r->buf, pkt, sizeof(pkt_t)); 
    ((pkt_t *) r->buf)->payload = NULL;
    ((pkt_t *) r->buf)->caplen = len;
    memcpy(r->buf + sizeof(pkt_t), COMO(payload), len);
    r->len = len + sizeof(pkt_t);
}

