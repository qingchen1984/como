/*
 * Copyright (c) 2004, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>	/* F_GETFL */
#include <unistd.h>     /* read, write etc. */
#include <string.h>     /* bzero */
#include <errno.h>      /* errno */

#include "como.h"
#include "sniffers.h"
#include "sniffer-list.h"

/* poll time (in usec) */
#define POLL_WAIT   500000

/* flush and freeze/unfreeze thresholds */
#define MB(m)				((m)*1024*1024)
#define FLUSH_THRESHOLD(mem)		(MB(mem) / 2) 
#define FREEZE_THRESHOLD(mem)		(MB(mem) / 10)
#define UNFREEZE_THRESHOLD(mem)		(MB(mem) - MB(mem) / 3) 

#define PKT_BUF_SIZE (1024 * 1024)

/* sniffer list and callbacks */
extern struct _sniffer *__sniffers[];

/*
 * Support for tailq handling.
 * Used for the expired tables.
 */
typedef struct {
    void * __head;
    void * __tail;
} tailq_t;


#define TQ_HEAD(queue)  ((queue)->__head)

#define TQ_APPEND(queue, entry, link_field)             \
    do {                                                \
        tailq_t *q = (queue);                           \
        typeof(entry) e = entry;                        \
        if (q->__head)                                  \
            ((typeof(entry))q->__tail)->link_field = e; \
        else                                            \
            q->__head = e;                              \
        q->__tail = e;                                  \
        e->link_field = NULL;                           \
    } while (0);


/*
 * -- match_desc
 * 
 * This function checks if the packet stream output by a source (sniffer or
 * module via dump interface) is compatible with the input requirements of
 * a module; it does so by checking the values of their pktdesc_t, in
 * particular matching the bitmask part.
 */
static int
match_desc(pktdesc_t *req, pktdesc_t *bid)
{
    char *a, *b;
    size_t i; 
    
    /* if req or bid are NULL they always match 
     * XXX this assumption could go away once we introduce meta-packet 
     *     fields (e.g., AS number, anonymized addresses, etc.)
     */
    if (!req || !bid)
        return 1;
    
    /* compare time granularity */ 
    if (req->ts < bid->ts)
        return 0;

    if (req->caplen > req->caplen) 
	return 0; 
         
    a = (char *) req; 
    b = (char *) bid; 

    i = sizeof(req->ts) + sizeof(req->caplen); 
    while (i < sizeof(pktdesc_t)) { 
        if (a[i] & ~b[i])
            return 0;
        i++; 
    } 
    
    return 1;
}


/*
 * -- create_filter
 *
 * Create a filter on the fly. Using the template provided
 * in the configuration, for each module we dump the associated
 * filter expression, then invoke the compiler to build a shared object
 * which reside in the working directory.
 * The function returns the full pathname of the shared object.
 *
 */
static char *
create_filter(module_t * mdl, int count, char * template, char *workdir)
{
    char *buf;
    FILE * fp;
    int ret;
    int idx; 

    asprintf(&buf, "%s/my_filter", workdir);
    fp = fopen(buf, "w");
    if (fp == NULL)
        panic("cannot open temp file %s", buf);
    free(buf);

    fprintf(fp, "#define MODULE_COUNT %d\n", count);
    fprintf(fp, "#define USERFILTER \\\n");

    for (idx = 0; idx < count; idx++) 
         fprintf(fp, "\touts[%d][i] = (%s);\\\n", idx, mdl[idx].filter);

    fprintf(fp,"\n");

    fclose(fp);

    /*
     * Invoke the compiler with the appropriate flags.
     */
#define	BUILD_FLAGS "-g -I- -I . -shared -Wl,-x,-S"
    asprintf(&buf,
        "(cd %s; cc %s \\\n"
        "\t\t-o my_filter.so -include my_filter %s )",
	workdir, BUILD_FLAGS, template);
#undef BUILD_FLAGS

    logmsg(LOGCAPTURE, "compiling filter \n\t%s\n", buf);
    ret = system(buf);
    free(buf);
    if (ret != 0) {
	logmsg(LOGWARN, "compile failed with error %d\n", ret);
	return NULL;
    }
    asprintf(&buf, "%s/my_filter.so", workdir);
    return buf;
}


/* 
 * -- create_table 
 * 
 * allocates and initializes a hash table
 */
static ctable_t *
create_table(module_t * mdl, timestamp_t ts) 
{
    ctable_t * ct; 
    size_t len;

    len = sizeof(ctable_t) + mdl->ca_hashsize * sizeof(void *);
    ct = new_mem(NULL, len, "new_flow_table");
    if (ct == NULL)
	panicx("CAPTURE ran out of memory (allocating table)");

    ct->size = mdl->ca_hashsize;
    ct->first_full = ct->size;      /* all records are empty */
    ct->records = 0;
    ct->live_buckets = 0;
    ct->module = mdl;
    ct->mem = NULL;                 /* use the system's map */
    ct->ts = ts; 

    /*
     * save the timestamp indicating with flush interval this 
     * table belongs to. this information will be useful for 
     * EXPORT when it processes the flushed tables. 
     */
    ct->ivl = ts - (ts % mdl->max_flush_ivl);
    return ct; 
}


/*
 * -- flush_table
 *
 * Called by capture_pkt() process when a timeslot is complete.
 * it flushes the flow table (if it exists and it is non-empty)
 * and then it allocates a new one.
 *
 */
static void
flush_table(module_t * mdl, tailq_t * expired)
{
    ctable_t *ct;

    logmsg(V_LOGCAPTURE, "flush_table start\n");

    /* check if the table is there and if it is non-empty */
    ct = mdl->ca_hashtable;
    if (ct && ct->records) {
        if (ct->records > ct->size)
            logmsg(LOGWARN,
		"flush_table table '%s' overfull (%d vs %d) -- %d live\n",
		mdl->name, ct->records, ct->size, ct->live_buckets);

        logmsg(V_LOGCAPTURE,
            "flush_tables %p(%s) buckets %d records %d live %d\n", ct,
            mdl->name, ct->size, ct->records, ct->live_buckets);

        /* add to linked list and remove from here. */
        TQ_APPEND(expired, ct, next_expired);
        mdl->ca_hashtable = NULL;
    }

    logmsg(V_LOGCAPTURE, "module %s flush_table done.\n", mdl->name);
}


#if 0 // still working on this...

/* 
 * -- freeze_module
 * 
 * This function is called if capture runs out of memory (this could
 * also be due to export being too slow in processing the expired flow
 * tables). It browses thru the module list and select one module to 
 * be frozen. The decision is based on static module priority (the weight 
 * parameter in the configuration file). If two modules have the same 
 * weight, the module that is using the most memory is frozen.
 * The frozen module loses all current data stored in CAPTURE and is set 
 * to run in passive mode until the memory usage drops below a certain 
 * threshold. The function returns the index of the module that has 
 * been frozen. 
 *     
 * XXX EXPORT is not informed of this. It will come for free if we 
 *     decide to put the _como_map in shared memory. Otherwise, we will
 *     have to add some explicit message. 
 */
static int 
freeze_module(void)
{
    module_t * freeze;
    ctable_t * ct; 
    int idx; 

    freeze = &map.modules[0];
    for (idx = 1; idx < map.module_count; idx++) { 
	module_t * mdl = &map.modules[idx];

	if (mdl->status != MDL_ACTIVE) 
	    continue; 

 	if (mdl->weight < freeze->weight) {
	    freeze = mdl; 
	} else if (mdl->weight == freeze->weight) {
	    if (mdl->memusage > freeze->memusage) 
		freeze = mdl;
	} 
    }

    /* 
     * freeze this module and free all memory that it is 
     * currently used. 
     * 
     * XXX NOTE: this way we drop data that has already been processed. 
     *     Unfortunately, we don't have a choice given that we cannot 
     *     pass this table to EXPORT and have it process it because we 
     *     are trying to allocate memory to a module that is currently 
     *     serving packets. Anyway, the only alternative would be to 
     *     freeze modules before the memory is exhausted. The end result
     *     of these approaches would be more or less the same. 
     */     
    logmsg(LOGWARN, "freezing module %s to save memory\n", freeze->name); 
    freeze->status = MDL_FROZEN; 

    /* free all records in the capture table, one by one */
    for (ct = mdl->ca_hashtable; ct->first_full < ct->size; ct->first_full++) {
	rec_t * fh; 

        fh = ct->bucket[ct->first_full];
        while (fh != NULL) {
            rec_t *end = fh->next;      /* Mark next record to scan */

            /* Walk back to the oldest record */
            while (fh->prev)
                fh = fh->prev;

            /*
             * now save the entries for this flow one by one
             */
            while (fh != end) {
                rec_t *p;

                /* keep the next record because fh will be freed */
                p = fh->next;

                mfree_mem(ct->mem, fh, 0);  /* done, free this record */
                fh = p;                 /* move to the next one */
            }
            /* done with the entry, move to next */
            ct->bucket[ct->first_full] = fh;
	}
    }

    /* free the table */
    mfree_mem(NULL, ct, 0);

    /* merge to the rest of the memory */
    mem_merge_maps(NULL, mem);

    return freeze->idx; 
}

#endif

/* global state */
extern struct _como map;

/*
 * -- capture_pkt
 *
 * This function is called for every batch of packets that need to be
 * processed by a classifier.
 * For each packet in the batch it runs the check()/hash()/match()/update()
 * methods of the classifier cl_index. The function also checks if the
 * current flow table needs to be flushed.
 *
 */
static timestamp_t
capture_pkt(module_t * mdl, void *pkt_buf, int no_pkts, int * which, 
	tailq_t * expired)
{
    pkt_t *pkt = (pkt_t *) pkt_buf;
    ctable_t * ct; 
    timestamp_t max_ts; 
    int i;

    if (mdl->ca_hashtable == NULL) 
	mdl->ca_hashtable = create_table(mdl, pkt->ts); 

    ct = mdl->ca_hashtable;

    max_ts = 0; 
    for (i = 0; i < no_pkts; i++, pkt = STDPKT_NEXT(pkt)) {
        rec_t *prev, *cand;
        uint32_t hash;
        uint bucket;

	if (pkt->ts >= max_ts) {
	    max_ts = pkt->ts; 
	} else {
	    logmsg(LOGCAPTURE,
		"pkt no. %d timestamps not increasing (%u.%06u --> %u.%06u)\n", 
		i, TS2SEC(max_ts), TS2USEC(max_ts), 
		TS2SEC(pkt->ts), TS2USEC(pkt->ts));
	}

        /* flush the current flow table, if needed */
	ct->ts = pkt->ts; 
        if (ct->ts > ct->ivl + mdl->max_flush_ivl) {  
            flush_table(mdl, expired);
            ct = mdl->ca_hashtable = create_table(mdl, pkt->ts); 
	}


        if (which[i] == 0)
            continue;   /* no interest in this packet */

        /* unset the filter for this packet */
        which[i] = 0;

        /*
         * check if there are any errors in the packet that
         * make it unacceptable for the classifier.
         * (if check() is not provided, we take the packet anyway)
         */
        if (mdl->callbacks.check != NULL && !mdl->callbacks.check(pkt))
            continue;

        /*
         * find the entry where the information related to
         * this packet reside
         * (if hash() is not provided, it defaults to 0)
         */
        hash = mdl->callbacks.hash != NULL ? mdl->callbacks.hash(pkt) : 0;
        bucket = hash % ct->size;

        /*
         * keep track of the first entry in the table that is used.
         * this is useful for the EXPORT process that will have to
         * scan the entire hash table later.
         */
        if (bucket < ct->first_full)
            ct->first_full = bucket;

        prev = NULL;
        cand = ct->bucket[bucket];
        while (cand) {
            /* if match() is not provided, any record matches */
            if (mdl->callbacks.match == NULL || mdl->callbacks.match(pkt, cand))
                break;
            prev = cand;
            cand = cand->next;
        }

        if (cand != NULL) {
           /*
            * found!
            * two things to do first:
            *   i) move this record to the front of the bucket to
            *      speed up future (and likely) accesses.
            *  ii) check if this record was flagged as full and
            *      in that case create a new one;
            */

            /* move to the front, if needed */
            if (ct->bucket[bucket] != cand) {
                prev->next = cand->next;
                cand->next = ct->bucket[bucket];
                ct->bucket[bucket] = cand;
            }

            /* check if this record was flagged as full */
            if (cand->full) {
                rec_t * x;

                /* allocate a new record */
                x = new_mem(ct->mem, mdl->callbacks.ca_recordsize, "new");
		if (x == NULL) 
		    panicx("CAPTURE ran out of memory (allocating record)");
                x->hash = cand->hash;
                x->next = cand->next;

                /* link the current full one to the list of full records */
                x->prev = cand;
                cand->next = x;

                /* we moved cand to the front, now is x */
                ct->bucket[bucket] = x;

                /* done. new empty record ready. */
                /* NOTE: we do not increment ct->records here because we count
	 	 *       this just as a variable size record. */

                /* now update the new record */
                x->full = mdl->callbacks.update(pkt, x, 1);
                continue;
            }

            /* not full and not new, update the record */
            cand->full = mdl->callbacks.update(pkt, cand, 0);
            continue;
        }

        /*
         * not found!
         * create a new record, update table stats
         * and link it to the bucket.
         */
        cand = new_mem(ct->mem,
		       mdl->callbacks.ca_recordsize + sizeof(rec_t), 
		       "new");
	if (cand == NULL) 
	    panicx("CAPTURE ran out of memory (allocating record)");
        cand->hash = hash;
        cand->next = ct->bucket[bucket];

        ct->records++;
        ct->bucket[bucket] = cand;
	if (cand->next == NULL) 
	    ct->live_buckets++;

        /* now update the new record */
        cand->full = mdl->callbacks.update(pkt, cand, 1);
    }

    return max_ts; 
}




/*
 * -- capture_mainloop
 *
 * This is the CAPTURE mainloop. It first prepares the filter
 * (that could be compiled on the fly) and then opens the sniffer
 * device. Then the real mainloop starts and it sits on a select
 * waiting for messages from EXPORT, the SUPERVISOR or for packets
 * from the sniffer.
 *
 */
void
capture_mainloop(int export_fd)
{
    filter_fn * filter;		/* filter function */
    memlist_t * flush_map;	/* freed blocks in the flow tables */
    tailq_t expired;		/* expired flow tables */
    uint8_t pkt_buf[PKT_BUF_SIZE];   /* batch of packets from sniffer */
    int sniffers_left;		/* how many sniffers are left ? */
    int sent2export;		/* message sent to export */
    source_t *src;
    timestamp_t last_ts; 	/* timestamp of the most recent packet seen */

    /* we only get a socketpair to the export process */

    /*
     * load the filter in. if no object has been provided, then
     * the filter must be compiled on the fly. we will write the
     * filter on a temporary file, call the compiler and finally
     * link in the new shared object.
     */
    if (!map.filter)
        map.filter = create_filter(map.modules, map.module_count,
		map.template, map.workdir);

    filter = load_object(map.filter, "filter");

    /* 
     * browse the list of sniffers and start them. also, make
     * sure the modules support the sniffers we use. 
     */
    sniffers_left = 0;
    for (src = map.sources; src; src = src->next) {
	int idx; 

	/*
         * start the sniffer
	 */
	if (src->cb->sniffer_start(src) < 0) { 
	    logmsg(LOGWARN, "failure to open capture device %s (%s)\n",
		src->cb->name, src->device);
	    continue; 
	} 
	sniffers_left++;
        
	/*
	 * now we browse the list of modules to make sure that 
    	 * they understand the packets coming from the sniffers. 
	 * to do so, we compare the indesc defined in the module callbacks
	 * data structure with the output descriptor define in the source. 
	 * if there is a mismatch, the module is shut down. 
	 */
	for (idx = 0; idx < map.module_count; idx++) {
	    module_t * mdl = &map.modules[idx];

            if (!match_desc(mdl->callbacks.indesc, src->output)) { 
		logmsg(LOGWARN, "module %s does not get %s packets\n", 
		    mdl->name, src->cb->name);
		mdl->status = MDL_INCOMPATIBLE; 
		map.stats->modules_active--;
	    } 
        }
    }

    /*
     * allocate memory used to pass flow tables
     * between CAPTURE and EXPORT
     */
    flush_map = new_memlist(16);

    logmsg(LOGUI, "--- Capture configuration: %d MB, %d sniffers ---\n",
		map.mem_size, sniffers_left);
    for (src = map.sources; src; src = src->next) {
        if (src->fd < 0)
            continue;
        logmsg(LOGUI, "\tsniffer [%s] %s\n", src->cb->name, src->device);
    }

    /* init expired flow tables queue */
    TQ_HEAD(&expired) = NULL;

    /*
     * This is the actual main loop where we monitor the various
     * sniffers and the sockets to communicate with other processes.
     * If a sniffer's data stream is complete or fails, we close it.
     * The loop terminates when all sniffers are closed and there is
     * no pending communication with export.
     */
    sent2export = 0;	
    for (;;) {
	fd_set r;
	int i, maxfd = 0;
	struct timeval t = {POLL_WAIT / 1000000, POLL_WAIT % 1000000};
	struct timeval *pto = NULL;

        errno = 0;

	if (sniffers_left == 0 && !sent2export && TQ_HEAD(&expired) == NULL)
	    break;	/* nothing more to do */

	FD_ZERO(&r);
	
	if (sent2export) {	/* talked to export. listen for reply */
	    FD_SET(export_fd, &r);
	    maxfd = export_fd;
	}

	/* 
	 * add all descriptors for sniffers that support select().
	 * we will deal with the others later on. 
	 */
	for (src = map.sources; src; src = src->next) {
	    /* 
	     * if this sniffer uses polling, set the timeout for 
	     * the select() instead of adding the file descriptor to FD_SET 
	     */  
	    if (src->fd < 0 || (src->cb->flags & SNIFF_POLL)) {
		pto = &t; 
		continue;	/* do not select() on this one */
	    }

	    /* 
	     * if memory usage is high and this sniffer works from a 
	     * file, stop processing packets. this will give EXPORT
	     * some time to process the tables and free memory.  
	     */
	    if ((src->cb->flags & SNIFF_FILE) && 
		map.stats->mem_usage_cur > FLUSH_THRESHOLD(map.mem_size)) {
		continue;	/* do not select() on this one */
	    } 

	    FD_SET(src->fd, &r);
	    if (src->fd > maxfd)
		maxfd = src->fd;
	}

	/* wait for messages, sniffers or up to the polling interval */
	i = select(maxfd+1, &r, NULL, NULL, pto);
	if (i < 0)
	    panic("select"); 

        if (FD_ISSET(export_fd, &r)) {	/* export replies to our message */
	    int ret;
	    msg_t reply;

	    errno = 0;	/* reset */
	    ret = read(export_fd, &reply, sizeof(reply));
	    if (ret != 0 && errno != EAGAIN) {
		if (ret != sizeof(reply))
		    panic("error reading export_fd got %d", ret);

		if (reply.m != flush_map)
		    panicx("bad flush_map from export_fd");

		/* ok, export freed memory into the map for us */
		mem_merge_maps(NULL, flush_map);
		sent2export = 0;   /* mark no pending jobs */
	    }
        }

        /* need to write and have no pending jobs from export */
        if (!sent2export && (TQ_HEAD(&expired)) != NULL) {
	    msg_t x;	/* message to EXPORT */
	    int ret; 

            x.m  = flush_map;
            x.ft = TQ_HEAD(&expired);
            ret = write(export_fd, &x, sizeof(x)); 
            if (ret != sizeof(x))
                panic("error writing export_fd got %d", ret);  

            TQ_HEAD(&expired) = NULL;   /* we are done with this. */
            sent2export = 1;            /* wait response from export */
        }

	if (sniffers_left == 0)
	    continue;

        /*
	 * check sniffers for packet reception (both the ones that use 
	 * select() and the ones that don't)
         */
	for (src = map.sources; src; src = src->next) {
            int * which;
            int idx;
	    int count = 0;

	    if (src->fd < 0)
		continue;	/* invalid device */

	    if ( !(src->cb->flags & SNIFF_POLL) && !FD_ISSET(src->fd, &r))
		continue;	/* nothing to read here. */

	    count = src->cb->sniffer_next(src, pkt_buf, sizeof(pkt_buf));
	    if (count == 0)
		continue;

            if (count < 0) {
                src->cb->sniffer_stop(src);
		src->fd = -1;
		sniffers_left--;
                continue;
            }

	    logmsg(V_LOGCAPTURE, "received %d packets from sniffer\n", count);
	    map.stats->pkts += count; 

            /*
             * Select which classifiers need to see which packets
	     * The filter() function (see comments in file base/template)
	     * returns a bidimensional array of integer which[cls][pkt]
	     * where the first index indicates the classifier, the second
	     * indicates the packet in the batch.
	     * The element of the array is set if the packet is of interest
	     * for the given classifier, and it is 0 otherwise.
             */
            which = filter(pkt_buf, count, map.module_count);

            /*
             * Now browse through the classifiers and
             * perform the capture actions needed.
             *
             * XXX we do it this way just because anyway we
             *     have got a single-threaded process. 
	     *     will have to change it in the future... 
             *
             */
            for (idx = 0; idx < map.module_count; idx++) {
	  	if (map.modules[idx].status == MDL_ACTIVE) { 
		    last_ts = capture_pkt(&map.modules[idx], 
			pkt_buf, count, which, &expired);
		} 
                which += count; /* next module, new list of packets */
            }
        }

	/* 
	 * Now we check the memory usage. We use three thresholds.
	 * 
	 * FLUSH_THRESHOLD indicates that we need to flush some more 
	 * data so that export can process them and hopefully return them 
	 * quickly. 
	 * 
	 * FREEZE_THRESHOLD is used to freeze one of the modules (the way we
	 * decide to do this depends on several parameters -- check 
	 * freeze_module() for details). 
	 * 
	 * UNFREEZE_THRESHOLD is used to restart modules that have been 
	 * frozen once enough memory is available.  
	 * 
	 */
	map.stats->mem_usage_cur = memory_usage(); 
	map.stats->mem_usage_peak = memory_peak(); 
	if (map.stats->mem_usage_cur > FLUSH_THRESHOLD(map.mem_size)) {
	    int idx; 

	    /*
	     * flush all tables respecting the min flush time if set. 
	     */

	    logmsg(LOGCAPTURE, "memory usage %d above threshold %d\n", 
		map.stats->mem_usage_cur, FLUSH_THRESHOLD(map.mem_size));
	    logmsg(0, "    looking for tables to flush\n"); 
	    for (idx = 0; idx < map.module_count; idx++) {
		module_t * mdl = &map.modules[idx];
		ctable_t *ct = mdl->ca_hashtable;

		if (ct && ct->records && 
			last_ts > ct->ts + mdl->min_flush_ivl) {
		    logmsg(0, "    flushing table for %s\n", mdl->name); 
		    flush_table(mdl, &expired); 
		}
	    } 
	}

#if 0		// not implemented yet!
	if (map.stats->mem_usage_cur < UNFREEZE_THRESHOLD(map.mem_size))
	    unfreeze_module(); 
	if (map.stats->mem_usage_cur > FREEZE_THRESHOLD(map.mem_size))
	    freeze_module(); 
#endif
    }

    logmsg(LOGWARN, "Capture: no sniffers left, terminating.\n");
    return;
}
/* end of file */
