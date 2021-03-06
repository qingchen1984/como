/*
 * Copyright (c) 2004-2007, Intel Corporation
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

#include <sys/types.h>
#include <string.h>		/* bcopy */
#include <stdio.h>		/* fprintf, stderr */

#include "module.h"
#include "data.h"
#include "stdpkt.h"		/* ethernet headers, etc. */
#include "pcap.h"		/* bpf_int32, etc. */
#include "printpkt.h"

#define PRETTYFMT 		0
#define PCAPFMT			1

enum {
    FORMAT_PRETTY,
    FORMAT_PCAP
};

QUERY_FORMATS_BEGIN
    { FORMAT_PRETTY, "pretty", "text/plain" },
    { FORMAT_PCAP, "pcap", "application/binary" },
QUERY_FORMATS_END

DEFAULT_FORMAT = "pretty";

char buffer[65536];

void *
qu_init(mdl_t *self, int format_id, hash_t *args)
{
    size_t len;

    if (format_id == FORMAT_PCAP) {
        len = print_pcap_file_header(buffer);
        mdl_write(self, buffer, len);
    }

    return NULL;
}

void
qu_finish(mdl_t *self, int format_id, void *state)
{
    /* nothing to do here */
}

void
print_rec(mdl_t *self, int format_id, record_t *r, void * state)
{
    pkt_t *pkt = (pkt_t *) r->buf;
    size_t len = 0;

    /*
     * we need to fix pkt->payload before using the pkt, as
     * the pointer has changed. Using pointers inside the records
     * does not work unless we can correct them.
     */
    pkt->payload = (char *)&r->buf[sizeof(pkt_t)];

    switch(format_id) {
        case FORMAT_PCAP:
            len = print_pkt_pcap(pkt, buffer);
            break;
        case FORMAT_PRETTY:
            len = print_pkt_pretty(pkt, buffer, PRINTPKT_L2 | PRINTPKT_L3);
            len += sprintf(buffer + len, "\n");
            break;
    }

    mdl_write(self, buffer, len);
}


void
replay(mdl_t * self, record_t *r, void *state)
{
    mdl_write(self, (char *)r->buf, r->len);
}

