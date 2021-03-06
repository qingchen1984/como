/*
 * Copyright (c) 2004-2006, Intel Corporation
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


#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <assert.h>

#define LOG_DOMAIN "sniffer-pcap"
#include "como.h"
#include "sniffers.h"
#include "pcap.h"

#include "capbuf.c"

/*
 * SNIFFER  ---    pcap files 
 *
 * Reads pcap trace files. 
 *
 */

#define PCAP_MIN_BUFSIZE	(me->sniff.max_pkts * sizeof(pkt_t))
#define PCAP_MAX_BUFSIZE	(PCAP_MIN_BUFSIZE + (2*1024*1024))
#define PCAP_DEFAULT_MAPSIZE	(16*1024*1024)	// 16 MB
#define PCAP_MIN_MAPSIZE	(1*1024*1024)	// 1 MB
#define PCAP_MAX_MAPSIZE	(32*1024*1024)	// 32 MB

typedef struct mmap_area {
    char *              base;           /* base pointer of the area */
    off_t               nread;          /* read packets index */
    off_t               off;            /* start offset */
    off_t               remap;          /* remap offset */
} mmap_area_t;

struct pcap_me {
    sniffer_t		sniff;		/* common fields, must be the first */
    enum COMOTYPE	type;		/* como layer type */
    enum LINKTYPE	l2type;		/* link layer type */
    int			swap_byte_order; /* pcap headers are in the other
					   byte order */
    to_como_radio_fn	to_como_radio;  /* pointer to the radio header
					   conversion function */
    off_t		file_size;	/* size fo trace file */
    int			snaplen; 	/* capture length */
    capbuf_t		capbuf;
    mmap_area_t         mmaps[2];       /* mmapped areas of the trace file */
    uint8_t             cur;            /* current mmapped area */
    size_t		map_size;	/* size of mmap */
    off_t		remap_sz;
};

/* libpcap magic */
#define PCAP_MAGIC 0xa1b2c3d4

/* 
 * PCAP header. It precedes every packet in the file. 
 */
typedef struct pcap_hdr {
    struct timeval ts; 		/* time stamp */
    int caplen;        		/* length of portion present */
    int len;			/* length this packet (on the wire) */
} pcap_hdr_t;


/* 
 * swapl(), swaps()
 * 
 * swap bytes from network byte order to host byte order 
 * cannot use ntoh macros because we do really need to swap now. 
 */
static inline uint32_t 
swapl(uint32_t x) 
{
    return (((x & 0xff) << 24) | ((x & 0x0000ff00) << 8) | 
	    ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff));
}

static inline uint32_t 
swaps(uint16_t x) 
{
    return (((x & 0x00ff) << 8) | ((x >> 8) & 0x00ff));
}

static int sniffer_start(sniffer_t * s);
static int sniffer_next(sniffer_t * s, int max_pkts, timestamp_t max_ivl,
			pkt_t * first_ref_pkt, int * dropped_pkts);
static void sniffer_stop(sniffer_t * s);



/*
 * -- sniffer_init
 * 
 */
static sniffer_t *
sniffer_init(const char * device, const char * args, alc_t * alc)
{
    struct pcap_me *me;
    struct pcap_file_header pf;
    struct stat trace_stat;
    size_t sz;
    
    me = alc_new0(alc, struct pcap_me);

    me->sniff.device = alc_strdup(alc, device);

    /* XXX damores 22-03-07
     * this max_ptks value could be insufficient to get a full timebin,
     * depending on the monitored link speed.
     */
    me->sniff.max_pkts = 16384;
    me->sniff.flags = SNIFF_FILE | SNIFF_SELECT;
    me->map_size = PCAP_DEFAULT_MAPSIZE;
    me->cur = 1;

    if (args) { 
	/* process input arguments */
	char *p;

	if ((p = strstr(args, "mapsize=")) != NULL) {
	    me->map_size = atoi(p + 8);
	    me->map_size = ROUND_32(me->map_size);
	    if (me->map_size < PCAP_MIN_MAPSIZE) {
	    	me->map_size = PCAP_MIN_MAPSIZE;
	    }
	    if (me->map_size > PCAP_MAX_MAPSIZE) {
		me->map_size = PCAP_MAX_MAPSIZE;
	    }
	}
    }

    /* open the trace file */
    me->sniff.fd = open(device, O_RDONLY);
    if (me->sniff.fd < 0) {
	warn("error while opening file %s: %s\n",
	       device, strerror(errno));
	goto error;
    }
    
    /* get the trace file size */
    if (fstat(me->sniff.fd, &trace_stat) < 0) {
	warn("failed to stat file %s: %s\n",
	       device, strerror(errno));
	goto error;
    }
    me->file_size = trace_stat.st_size;
    me->sniff.filesize = me->file_size;

    /* read the pcap file header */    
    sz = sizeof(struct pcap_file_header);
    if ((size_t) read(me->sniff.fd, &pf, sz) != sz) {
	warn("error while reading file %s: %s\n",
	       device, strerror(errno));
	goto error;
    }
    
    /* check the pcap file header and byte endianness */
    if (pf.magic != PCAP_MAGIC) { 
        if (pf.magic != swapl(PCAP_MAGIC)) {
            warn("invalid pcap file %s\n",
		   device);
	    goto error;
        } else {
            pf.version_major = swaps(pf.version_major);
            pf.version_minor = swaps(pf.version_minor);
            pf.thiszone = swapl(pf.thiszone);
            pf.sigfigs = swapl(pf.sigfigs);
            pf.snaplen = swapl(pf.snaplen);
            pf.linktype = swapl(pf.linktype);
            me->swap_byte_order = 1;
        }
    }
    me->snaplen = pf.snaplen;

    /* check data link type */
    switch (pf.linktype) { 
    case DLT_EN10MB: 
	notice("datalink Ethernet (%d)\n", pf.linktype); 
	me->type = COMOTYPE_LINK;
	me->l2type = LINKTYPE_ETH;
	break;
    case DLT_C_HDLC: 
	notice("datalink HDLC (%d)\n", pf.linktype); 
	me->type = COMOTYPE_LINK;
	me->l2type = LINKTYPE_HDLC;
	break;
    case DLT_IEEE802_11: 
	notice("datalink 802.11 (%d)\n", pf.linktype); 
	me->type = COMOTYPE_LINK;
	me->l2type = LINKTYPE_80211;
	break;
    case DLT_IEEE802_11_RADIO:
        notice("datalink 802.11_radiotap (%d)\n", pf.linktype);
        me->type = COMOTYPE_RADIO;
	me->l2type = LINKTYPE_80211;
        me->to_como_radio = radiotap_header_to_como_radio;
        break;
    case DLT_IEEE802_11_RADIO_AVS:
	notice("datalink 802.11 with AVS header (%d)\n", pf.linktype);
	me->type = COMOTYPE_LINK;
	me->l2type = LINKTYPE_80211;
	me->to_como_radio = avs_header_to_como_radio;
	break;
    case DLT_PRISM_HEADER:
	notice("datalink 802.11 with Prism header (%d)\n", pf.linktype);
	me->type = COMOTYPE_RADIO;
	me->l2type = LINKTYPE_80211;
	me->to_como_radio = avs_or_prism2_header_to_como_radio;
	break;
    default: 
	warn("unrecognized datalink (%d) in file %s\n", 
	     pf.linktype, device);
	goto error;
    }

    /* create the capture buffer */
    if (capbuf_init(&me->capbuf, args, NULL, PCAP_MIN_BUFSIZE,
		    PCAP_MAX_BUFSIZE) < 0)
	goto error;

    return (sniffer_t *) me;
error:
    if (me->sniff.fd >= 0) {
	close(me->sniff.fd);
    }
    alc_free(alc, me->sniff.device);
    alc_free(alc, me);
    return NULL;
}


static metadesc_t *
sniffer_setup_metadesc(sniffer_t * s, alc_t * alc)
{
    struct pcap_me *me = (struct pcap_me *) s;
    const headerinfo_t *lchi, *l2hi;
    metadesc_t *outmd;
    pkt_t *pkt;
    char protos[32]; /* protos string of metadesc template */

    /* setup output descriptor */
    outmd = metadesc_new(NULL, alc, 0);
    
    lchi = headerinfo_lookup_with_type_and_layer(me->type, LCOMO);
    l2hi = headerinfo_lookup_with_type_and_layer(me->l2type, L2);
    assert(lchi);
    assert(l2hi);
    
    snprintf(protos, sizeof(protos) - 1, "%s:%s:any:any",
	     lchi->name, l2hi->name);
    pkt = metadesc_tpl_add(outmd, protos);
    COMO(caplen) = me->snaplen;
    
    return outmd;
}


static int
mmap_next_region(struct pcap_me * me)
{
    uint8_t old, new;

    old = me->cur;
    new = ~(me->cur) & 0x01;

    if (me->mmaps[new].base != NULL) {
        munmap(me->mmaps[new].base, me->map_size);
    }
        
    /* mmap the next part of the trace file in a new memory region */
    me->mmaps[new].base = (char *) mmap(NULL, me->map_size, PROT_READ,
                                      MAP_PRIVATE, me->sniff.fd,
                                      me->mmaps[old].remap);
    
    if (me->mmaps[new].base == MAP_FAILED) {
	warn("mmap failed: %s\n", strerror(errno));
	return -1;
    }
    
    me->mmaps[new].off = me->mmaps[old].nread - me->mmaps[old].remap;
    me->mmaps[new].remap = me->mmaps[old].remap + me->remap_sz;
    me->mmaps[new].nread = me->mmaps[old].nread;
    
    me->cur = new;

    return 0;
}


/* 
 * -- sniffer_start
 * 
 * Open the pcap file and read the header. We support ethernet
 * frames only. Return the file descriptor and set the type variable. 
 * It returns 0 on success and -1 on failure.
 *
 */
static int
sniffer_start(sniffer_t * s) 
{
    struct pcap_me *me = (struct pcap_me *) s;
    int ps, mp; /* page size, maximum packet size */
    int r;
    
    /* mmap the trace file */
    if (mmap_next_region(me) < 0)
	return -1;

    me->mmaps[me->cur].nread = me->mmaps[me->cur].off =
        sizeof(struct pcap_file_header);
    
    /*
     * compute remap offset using the system pagesize and the maximum
     * packet length.
     */
    ps = getpagesize();
    mp = me->snaplen + sizeof(pcap_hdr_t);
    r = (mp / ps) * ps;
    if (mp % ps > 0) {
	r += ps;
    }
    me->remap_sz = (off_t) (me->map_size - r);
    me->mmaps[me->cur].remap = me->remap_sz;
    
    return 0;
}


/*
 * sniffer_next
 *
 * Fill a structure with a copy of the next packet and its metadata.
 * Each packet is preceded by the following header
 *
 */
static int
sniffer_next(sniffer_t * s, int max_pkts, timestamp_t max_ivl,
	     UNUSED pkt_t * first_ref_pkt, int * dropped_pkts) 
{
    struct pcap_me *me = (struct pcap_me *) s;
    pkt_t *pkt;                 /* CoMo record structure */
    int npkts;                  /* processed pkts */
    timestamp_t first_seen = 0;

    /* TODO: handle truncated traces */
    if (me->mmaps[me->cur].nread >= me->file_size) {
        return -1;       /* end of file, nothing left to do */
    }

    if (me->mmaps[me->cur].nread > me->mmaps[me->cur].remap) {
	/* we've read all the packets that fit in the mmaped memory, now
	 * mmap the next area of the trace file */
	if (mmap_next_region(me) < 0)
	    return -1;
    }

    npkts = 0;
    
    capbuf_begin(&me->capbuf, NULL);

    while (npkts < max_pkts) {
	timestamp_t ts;
	pcap_hdr_t *ph;
#ifdef BUILD_FOR_ARM
	pcap_hdr_t tmp_ph;
#endif
	off_t left = me->file_size - me->mmaps[me->cur].nread;
	int drop_this = 0;
	char *base = me->mmaps[me->cur].base + me->mmaps[me->cur].off;

	if (me->mmaps[me->cur].nread > me->mmaps[me->cur].remap)
	    break;

	/* do we have a pcap header? */
	if (left < sizeof(pcap_hdr_t))
	    break;

#ifndef BUILD_FOR_ARM
	ph = (pcap_hdr_t *) base;
#else
	if (base & 0x3) {
	    /* base is not 32bit aligned => read the pcap header into tmp_ph */
	    memcpy(&tmp_ph, base, sizeof(pcap_hdr_t));
	    ph = &tmp_ph;
	} else {
	    ph = (pcap_hdr_t *) base;
	}
#endif

	/* convert the header if needed */
	if (me->swap_byte_order) {
	    ph->ts.tv_sec = swapl(ph->ts.tv_sec);
	    ph->ts.tv_usec = swapl(ph->ts.tv_usec);
	    ph->caplen = swapl(ph->caplen);
	    ph->len = swapl(ph->len);
	}

	/* check if entire record is available */
	if (left < sizeof(pcap_hdr_t) + ph->caplen)
	    break;

	ts = TIME2TS(ph->ts.tv_sec, ph->ts.tv_usec);
	if (npkts > 0 && (ts - first_seen) >= max_ivl) {
	    /* Never returns more than max_ivl of traffic */
	    break;
	} else {
	    first_seen = ts;
	}
	
	/* reserve the space in the buffer for the pkt_t */
	pkt = (pkt_t *) capbuf_reserve_space(&me->capbuf, sizeof(pkt_t));

	/*      
	 * Now we have a packet: start filling a new pkt_t struct
	 * (beware that it could be discarded later on)
	 */
	COMO(ts) = ts;
	COMO(len) = ph->len;
	COMO(type) = me->type;
	
	/* point to beginning of payload */
	base += sizeof(pcap_hdr_t);

	if (me->type != COMOTYPE_RADIO && me->l2type != LINKTYPE_80211) {
	    /*
	     * simplest case: the payload is not processed => export
	     * the pointer to mmaped memory
	     */
	    COMO(caplen) = ph->caplen;
	    COMO(payload) = base;
	} else if (me->type == COMOTYPE_RADIO) {
	    /*
	     * need to process at least the radio header => reserve an area
	     * of the capbuf able to contain the como radio header plus the
	     * maximum of caplen and sizeof(struct _como_wlan_mgmt), process
	     * the radio header, process the 802.11 frame if needed
	     */
	    size_t sz;
	    const size_t mgmt_sz = sizeof(struct _ieee80211_mgmt_hdr) +
				   sizeof(struct _como_wlan_mgmt);
	    char *dest;
	    int info_len, len;
	    struct _como_radio *radio;
	    struct _ieee80211_base *h;
	    
	    assert(me->l2type == LINKTYPE_80211);
	    sz = sizeof(struct _como_radio);
	    sz += MAX((size_t) ph->caplen, mgmt_sz);
	    /* reserve the space */
	    COMO(payload) = capbuf_reserve_space(&me->capbuf, sz);
	    
	    radio = (struct _como_radio *) COMO(payload);
	    /* process the radio info header */
	    info_len = me->to_como_radio(base, radio);
	    assert(info_len > 0);
	    
	    /* so far caplen is sizeof(struct _como_radio) */
	    COMO(caplen) = sizeof(struct _como_radio);
	    
	    /* point to the beginning of 802.11 frame */
	    base += info_len;
	    h = (struct _ieee80211_base *) base;
	    dest = COMO(payload) + COMO(caplen);
	    len = ph->caplen - info_len;
	    if (h->fc_type == IEEE80211TYPE_MGMT) {
		int mgmt_len;
		/* process the mgmt frame */
		mgmt_len = ieee80211_process_mgmt_frame(base, len, dest);
		if (mgmt_len > 0) {
		    COMO(caplen) += mgmt_len;
		} else {
		    drop_this = 1;
		}
	    } else {
		/* copy the mgmt frame */
		memcpy(dest, base, len);
		COMO(caplen) += len;
	    }
	} else {
	    /*
	     * in this last case first check the 802.11 frame type => if of 
	     * mgmt type reserve the space in capbuf and process the frame,
	     * otherwise just export the pointer to mmaped memory
	     */
	    struct _ieee80211_base *h;
	    h = (struct _ieee80211_base *) base;
	    if (h->fc_type == IEEE80211TYPE_MGMT) {
		int mgmt_len;
		size_t mgmt_sz = sizeof(struct _ieee80211_mgmt_hdr) +
				 sizeof(struct _como_wlan_mgmt);
		/* reserve the space */
		COMO(payload) = capbuf_reserve_space(&me->capbuf, mgmt_sz);
		/* process the mgmt frame */
		mgmt_len = ieee80211_process_mgmt_frame(base, ph->caplen,
							COMO(payload));
		if (mgmt_len > 0) {
		    COMO(caplen) = mgmt_len;
		} else {
		    drop_this = 1;
		}
	    } else {
		/* export the pointer to mmaped memory */
		COMO(caplen) = ph->caplen;
		COMO(payload) = base;
	    }
	}

	if (drop_this == 0) {
	    /* 
	     * update layer2 information and offsets of layer 3 and above. 
	     * this sniffer runs on ethernet frames
	     */
	    updateofs(pkt, L2, me->l2type);
	    /* increment the number of processed packets */
	    npkts++;
	    ppbuf_capture(me->sniff.ppbuf, pkt, s);
	} else {
	    (*dropped_pkts)++;
	}
	me->mmaps[me->cur].off += sizeof(pcap_hdr_t) + ph->caplen;
	me->mmaps[me->cur].nread += sizeof(pcap_hdr_t) + ph->caplen;
        me->sniff.curr_pos += sizeof(pcap_hdr_t) + ph->caplen;
    }

    return 0;
}


/* 
 * -- sniffer_stop
 * 
 * Close the file descriptor. 
 */
static void
sniffer_stop(sniffer_t * s)
{
    int i;
    struct pcap_me *me = (struct pcap_me *) s;
    
    for (i = 0; i <= 1; i++) {
        if (me->mmaps[i].base) {
            munmap(me->mmaps[i].base, me->map_size);
        }
    }

    close(me->sniff.fd);
}


static void
sniffer_finish(sniffer_t * s, alc_t * alc)
{
    struct pcap_me *me = (struct pcap_me *) s;

    capbuf_finish(&me->capbuf);
    alc_free(alc, me->sniff.device);
    alc_free(alc, me);
}


SNIFFER(pcap) = {
    name: "pcap",
    init: sniffer_init,
    finish: sniffer_finish,
    setup_metadesc: sniffer_setup_metadesc,
    start: sniffer_start,
    next: sniffer_next,
    stop: sniffer_stop,
    usage: NULL,
};
