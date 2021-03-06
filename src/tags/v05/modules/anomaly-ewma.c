/*
 * Copyright (c) 2004 Intel Corporation
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

/*
 * Volume anomaly detection module
 *
 * This module builds histograms for the number of packets and bytes 
 * observed over time. At each interval it computes a forecast (using an 
 * exponential weighted moving average EWMA) for the value of the next 
 * interval. If the actual value is very different from the estimate an 
 * alert is raised and stored to disk. 
 * Queries can then retrieve all alerts informations. 
 * 
 * The module presents three configuration parameters: 
 *
 *   .. interval, the measurement interval in second over which to 
 *      estimate the number of bytes sent.
 *   .. weight, the weight w to apply on the moving average, i.e. 
 *      x_{i+1} = w * x_i + (1 - w) * b, where b is the number of 
 *      bytes observed in the current interval and x_i is the 
 *      previous estimate.  
 *   .. change_thresh, the threshold value for the number of bytes, i.e., 
 *      an alert will be raised if b > change_threshold * x_i
 */

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "como.h"
#include "module.h"

#define FLOWDESC	struct _ports
#define EFLOWDESC	struct _alert_ewma

FLOWDESC {
    timestamp_t ts;
    uint64_t    bytes; 
    uint64_t    pkts; 
};

EFLOWDESC { 
    timestamp_t ts; 			/* timestamp of last update */
    uint64_t    bytes;			/* bytes after EWMA */
    uint64_t    pkts;			/* bytes after EWMA */
    float	diff_bytes;		/* difference in case of alert */
    float	diff_pkts;		/* difference in case of alert */
    int		alert;		 	/* alert */
    int 	past_alert;		/* set if last interval raised alert */
};

#define ALERT_BYTES		0x1
#define ALERT_PKTS		0x2
    

static int meas_ivl = 30;     		/* measurement interval */
static double weight = 0.9;		/* weigth for EWMA */
static double change_thresh = 3.0;	/* volume change threshold */ 

static timestamp_t 
init(__unused void *mem, __unused size_t msize, char *args[])
{
    int i;

    for (i = 0; args && args[i]; i++) {
	if (strstr(args[i], "interval")) {
	    char * val = index(args[i], '=') + 1;
	    meas_ivl = atoi(val);
        } else if (strstr(args[i], "weight")) {
	    char * val = index(args[i], '=') + 1;
	    weight = strtod(val, NULL); 
        } else if (strstr(args[i], "change_thresh")) {
	    char * val = index(args[i], '=') + 1;
	    change_thresh = strtod(val, NULL); 
        } 
    }

    return TIME2TS(meas_ivl, 0);
}

static int
update(pkt_t *pkt, void *rp, int isnew)
{
    FLOWDESC *x = F(rp);

    if (isnew) {
	x->ts = pkt->ts;
	x->bytes = 0; 
	x->pkts = 0;
    }

    x->bytes += pkt->len;
    x->pkts++;
    return 0;
}

static int
export(void *erp, void *rp, int isnew)
{
    EFLOWDESC *ex = EF(erp);
    FLOWDESC *x = F(rp);
    uint64_t b = x->bytes / meas_ivl;
    uint64_t p = x->pkts / meas_ivl;
    time_t ts;

    if (isnew) {
	bzero(ex, sizeof(EFLOWDESC));
	ex->bytes = b;
	ex->pkts = p;
    } 
    
    ex->ts = x->ts;

    /* check if we need to raise a volume alert */
    ex->diff_bytes = (float) b / (float) ex->bytes; 
    ex->diff_pkts = (float) p / (float) ex->pkts; 

    if (ex->diff_bytes > change_thresh)
	ex->alert |= ALERT_BYTES; 
    if (ex->diff_pkts > change_thresh)
	ex->alert |= ALERT_PKTS; 

    ts = (time_t) TS2SEC(ex->ts);
    logmsg(LOGMODULE, "ts: %.24s; alert: %d, past: %d, diff: %.2f, %.2f\n", 
	asctime(gmtime(&ts)), ex->alert, ex->past_alert, 
	ex->diff_bytes, ex->diff_pkts); 

    /* update the moving average for the sum */
    ex->bytes = (uint64_t) ((1.0 - weight) * (float) ex->bytes + 
				weight * (float) b); 
    ex->pkts = (uint64_t) ((1.0 - weight) * (float) ex->pkts + 
				weight * (float) p); 

    /* reset the past alert flag if no alerts are raised */
    ex->past_alert &= ex->alert; 

    return 0;
}
    
static int
action(void * rp, __unused timestamp_t t, __unused int count)
{
    EFLOWDESC *ex = EF(rp);

    if (rp == NULL)
        return ACT_GO;

    if (ex->past_alert)
	ex->alert = 0; 

    return (ex->alert)? ACT_STORE : ACT_STOP;
}

struct alert_record { 
    uint32_t ts; 
    uint16_t type; 
    uint16_t change; 
};


static ssize_t
store(void *rp, char *buf, size_t len)
{
    EFLOWDESC *ex = EF(rp);
    time_t ts;
    int count;

    if (len < 2*sizeof(struct alert_record))
	return -1; 

    ts = (time_t) TS2SEC(ex->ts);
    count = 0;

    if (ex->alert & ALERT_BYTES) {
	uint16_t ch = (uint16_t) (ex->diff_bytes * 256); 
	PUTH32(buf, TS2SEC(ex->ts));
	PUTH16(buf, ALERT_BYTES);
	PUTH16(buf, ch);
	logmsg(LOGUI, "ts: %.24s; ewma: %llu; type: bytes diff: %.2f\n", 
	       asctime(gmtime(&ts)), ex->bytes, ex->diff_bytes);
	count++;
    } 

    if (ex->alert & ALERT_PKTS) {
	uint16_t ch = (uint16_t) (ex->diff_pkts * 256); 
	PUTH32(buf, TS2SEC(ex->ts));
	PUTH16(buf, ALERT_PKTS);
	PUTH16(buf, ch);
	logmsg(LOGUI, "ts: %.24s; ewma: %llu; type: packets diff: %.2f\n", 
	       asctime(gmtime(&ts)), ex->pkts, ex->diff_pkts);
	count++;
    } 

    ex->past_alert = ex->alert; 
    ex->alert = 0;

    return count*sizeof(struct alert_record);
}

static size_t
load(char * buf, size_t len, timestamp_t * ts)
{
    if (len < sizeof(struct alert_record)) {
        ts = 0;
        return 0;
    }

    *ts = TIME2TS(ntohl(((struct alert_record *)buf)->ts),0); 
    return sizeof(struct alert_record);
}


#define PRETTYHDR	"Date                     Port number   Type\n"
#define PRETTYFMT	"%.24s %s %2u.2u\n"

#define PLAINFMT	"%12ld %2 %2u.%2u\n"

#define HTMLHDR                                                 \
    "<html>\n"							\
    "<head>\n"							\
    "  <style type=\"text/css\">\n"				\
    "   body {margin: 0; padding: 0\n"				\
    "     font-family: \"lucida sans unicode\", verdana, arial;\n" \
    "     font-size: 9pt; scrollbar-face-color: #ddd}\n"        \
    "   table,tr,td{\n"						\
    "     margin: 1; font-family: \"lucida sans unicode\", verdana, arial;\n" \
    "     font-size: 9pt; background-color: #dddddd;}" 		\
    "   a, a.visited { text-decoration: none;}\n"		\
    "   .netview {\n"						\
    "     top: 0px; width: 100%%; vertical-align:top;\n" 	\
    "     margin: 2; padding-left: 5px;\n" 			\
    "     padding-right: 5px; text-align:left;}\n" 		\
    "   .nvtitle {\n"						\
    "     font-weight: bold; padding-bottom: 3px;\n" 		\
    "     font-family: \"lucida sans unicode\", verdana, arial;\n" \
    "     font-size: 9pt; color: #475677;}\n"			\
    "  </style>\n"						\
    "</head>\n"							\
    "<body><div class=nvtitle style=\"border-top: 1px dashed;\">"  \
    "Alerts</div>\n"  				

#define HTMLHDR_ALERTS						\
    "<table class=netview>\n"                                   \
    "  <tr class=nvtitle>\n" 					\
    "    <td><b>Time</b></td>\n" 		                \
    "    <td><b>Type</b></td>\n" 				\
    "    <td><b>Size</b></td>\n" 				\
    "  </tr>\n"                                         

#define HTMLFMT							\
    "<tr><td><a href=%s target=_top>%s</a></td><td>%s</td>"  \
    "<td>%2u.%2u</td></tr>\n"

#define HTMLFOOTER_ALERTS                                       \
    "</table>\n"                                                

#define HTMLFOOTER                                              \
    "</div></body></html>\n"                                          

static char *
print(char *buf, size_t *len, char * const args[])
{
    static char s[2048];
    static char * fmt; 
    static char urlstr[2048] = "#";
    static int alerts = 0;
    struct alert_record *x; 
    char typestr[20];
    time_t ts;
    int n; 
    uint32_t ch_int, ch_dec; 

    if (buf == NULL && args != NULL) { 
        char * url = NULL;
        char * urlargs[20];
        int no_urlargs = 0;

	/* by default, pretty print */
	*len = sprintf(s, PRETTYHDR);  
	fmt = PRETTYFMT; 

	/* first call of print, process the arguments and return */
	for (n = 0; args[n]; n++) {
	    if (!strcmp(args[n], "format=plain")) {
		*len = 0; 
		fmt = PLAINFMT;
	    } else if (!strcmp(args[n], "format=html")) {
                *len = sprintf(s, HTMLHDR); 
                fmt = HTMLFMT;
            } else if (!strncmp(args[n], "url=", 4)) {
                url = args[n] + 4;
            } else if (!strncmp(args[n], "urlargs=", 8)) {
                urlargs[no_urlargs] = args[n] + 8;
                no_urlargs++;
            }
        }

        if (url != NULL) {
            int w, k;

            w = sprintf(urlstr, "%s?", url);
            for (k = 0; k < no_urlargs; k++)
                w += sprintf(urlstr + w, "%s&", urlargs[k]);
	    w += sprintf(urlstr + w, "stime=%%u&etime=%%u"); 
        }

	return s; 
    } 

    if (buf == NULL && args == NULL) {  
	*len = alerts? 0 : 
	    sprintf(s, "No anomalies to report during this time interval\n"); 
	if (fmt == HTMLFMT) { 
	    if (alerts) 
		*len += sprintf(s + *len, HTMLFOOTER_ALERTS); 
	    *len += sprintf(s + *len, HTMLFOOTER);
	} 
	return s; 
    } 
	
    *len = 0; 

    if (alerts == 0) { 
	if (fmt == HTMLFMT) 
	    *len = sprintf(s, HTMLHDR_ALERTS); 
	alerts = 1;
    } 

    x = (struct alert_record *) buf; 
    ts = (time_t) ntohl(x->ts);
    sprintf(typestr, "%s", 
	((ntohs(x->type) & ALERT_BYTES)? "bytes" : "packets")); 
    ch_int = ntohs(x->change) >> 8; 
    ch_dec = ntohs(x->change) & 0xff;

    /* print according to the requested format */
    if (fmt == PRETTYFMT) {
	*len += sprintf(s + *len, fmt, asctime(gmtime(&ts)), typestr, 
			ch_int, ch_dec);
    } else if (fmt == HTMLFMT) {
	char timestr[20]; 
        char tmp[2048] = "#";

        if (urlstr[0] != '#')
            sprintf(tmp, urlstr, ts - 3600, ts + 3600);

	strftime(timestr, sizeof(timestr), "%b %d %T", gmtime(&ts)); 
	*len += sprintf(s + *len, fmt, tmp, timestr, typestr, 
			ch_int, ch_dec); 
    } else {
	*len += sprintf(s + *len, fmt, (long int)ts, typestr, ch_int, ch_dec); 
    } 
	
    return s;
}

callbacks_t callbacks = {
    ca_recordsize: sizeof(FLOWDESC),
    ex_recordsize: sizeof(EFLOWDESC),
    st_recordsize: 2*sizeof(struct alert_record), 
    indesc: NULL, 
    outdesc: NULL,
    init: init,
    check: NULL,
    hash: NULL,
    match: NULL,
    update: update,
    ematch: NULL,
    export: export,
    compare: NULL,
    action: action,
    store: store,
    load: load,
    print: print,
    replay: NULL
};
