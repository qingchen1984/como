/*
 * Copyright (c) 2005 Universitat Politecnica de Catalunya 
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
 */

/*
 * Snort Module
 *
 * This module interprets and runs Snort rules passed as arguments from the
 * config file
 * Output: 
 * - logs in libpcap format, alerts in Snort's fast-alert format
 *   (through the print() callback)
 * - packet trace formed by pkt_t structures
 *   (through the dump() callback)
 *
 */

#include <time.h>
#include <string.h>         /* bcopy */
#include <sys/types.h>
#include <pcap.h>           /* libpcap data types */

#include "como.h"
#include "module.h"
#include "snort.h"


#define FLOWDESC        struct _snort


__inline size_t
offcopy(void *dst, const void *src, size_t n);

#define OFFCOPY(d,s,n)  ((memcpy(d,s,n)),(n))

#define TIMEBUF_SIZE    26

#define IP_ADDR_LEN     15    /* strlen("XXX.XXX.XXX.XXX") */
#define IP_ADDR(x)      (inet_ntoa(*(struct in_addr*)&(N32(x))))

#define MAXPKTSIZE      65535         // max bytes we keep of a packet 
#define RECSIZE         MAXPKTSIZE + 20 // Actual size of the record on disk
FLOWDESC {
    uint8_t buf[MAXPKTSIZE];
    uint    sz; 
};

#if 0 
/* Input and output packet description, needed to use dump() */
static pktdesc_t indesc;
static pktdesc_t outdesc;
#endif

struct snort_hdr {
    int alert;  /* Snort rule that fired the alert 
                   (-1 if the packet was logged by a non-alert rule) */
    pkt_t pkt; 
};

/* Here we save the info that we get from Snort rules */
ruleinfo_t ri[MAX_RULES];

/* Number of Snort rules in the config file */
int nrules = 0;

/* Number of the rule that matches with a packet */
int rule_match;

/* Declaration of the Bison-generated parsing routine */
void snort_parse_rules(char *rules);

static int
init(__unused void *mem, __unused size_t msize, char **args)
{
    int i;
    
    /* Call the parsing routine generated by Bison.
     * It reads a Snort rule's info and fills the ri
     * structure with it.
     */
    for(i = 0; args[i] != NULL; i++) {
        snort_parse_rules(args[i]);
    }
    
#if 0		/* XXX this is not done right yet! */
    /* Fill the input and output packet description.
     * We need and provide entire packet headers 
     * so we fill all the bitmasks with ones.
     */
    memset(&indesc, 0, sizeof(pktdesc_t));
    memset(&(indesc.bm), 0xff, 
	sizeof(struct _como_machdr) + sizeof(struct _como_iphdr) +
        sizeof(struct _como_tcphdr) + sizeof(struct _como_udphdr) +
        sizeof(struct _como_icmphdr));
    memset(&outdesc, 0, sizeof(pktdesc_t));
    memset(&(outdesc.bm), 0xff, 
	sizeof(struct _como_machdr) + sizeof(struct _como_iphdr) +
        sizeof(struct _como_tcphdr) + sizeof(struct _como_udphdr) +
        sizeof(struct _como_icmphdr));
#endif

    return 0;
}
    
/**
 * -- check_proto
 *
 * matches the protocol type in a packet 
 * with the one obtained from a Snort rule
 *
 */
unsigned int 
check_proto(ruleinfo_t *i, pkt_t *pkt)
{
    if (pkt->l3type == ETH_P_IP)
	return (i->proto == IP(proto));
    else
	return 0;
}

/**
 * -- check_xxx_xxx_port
 *
 * match the tcp/udp port in a packet 
 * against a set of ports obtained from a Snort rule
 *
 */    
unsigned int 
check_tcp_src_port(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0;
    r = ( i->src_ports.lowport <= H16(TCPUDP(src_port)) &&
          i->src_ports.highport >= H16(TCPUDP(src_port))    );
    if (i->src_ports.negation) r ^= 1;
    return r;
}

unsigned int 
check_tcp_dst_port(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0;
    r = ( i->dst_ports.lowport <= H16(TCP(dst_port)) &&
          i->dst_ports.highport >= H16(TCP(dst_port))    );
    if (i->dst_ports.negation) r ^= 1;
    return r;
}

unsigned int 
check_udp_src_port(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0;
    r = ( i->src_ports.lowport <= H16(UDP(src_port)) &&
          i->src_ports.highport >= H16(UDP(src_port))    );
    if (i->src_ports.negation) r ^= 1;
    return r;
}

unsigned int 
check_udp_dst_port(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0;
    r = ( i->dst_ports.lowport <= H16(UDP(dst_port)) &&
          i->dst_ports.highport >= H16(UDP(dst_port))    );
    if (i->dst_ports.negation) r ^= 1;
    return r;
}

/**
 * -- check_xxx_ip
 *
 * match the src/dst IP address in a packet 
 * with the one(s) obtained from a Snort rule
 *
 */    
unsigned int
check_src_ip(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0; 
    ipnode_t *aux;
    aux = i->src_ips.ipnode;
    while (aux != NULL && !r) {
        r = (aux->ipaddr == (N32(IP(src_ip)) & aux->netmask));
        aux = aux->next;
    }
    if (i->src_ips.negation) r ^= 1;
    return r;
}

unsigned int
check_dst_ip(ruleinfo_t *i, pkt_t *pkt)
{
    unsigned int r = 0; 
    ipnode_t *aux;
    aux = i->dst_ips.ipnode;
    while (aux != NULL && !r) {
        r = (aux->ipaddr == (N32(IP(dst_ip)) & aux->netmask));
        aux = aux->next;
    }
    if (i->dst_ips.negation) r ^= 1;
    return r;
}

/**
 * -- check_options
 *
 * processes the options found in a Snort rule's body
 * against the info in a packet
 * XXX This function is only provisional yet
 * 
 */
unsigned int
check_options(ruleinfo_t *i, pkt_t *pkt) 
{
    optnode_t *opt;
    size_t pload_len; 
    char *pl;
    
    opt = i->opts;
    
    while (opt != NULL) {
        switch(opt->keyword) {
            case SNTOK_NULL:
                /* There are no options in the rule */
                return 1;

            case SNTOK_CONTENT:
		pload_len = pkt->caplen; 
		if (pload_len == 0) 
		    break; 
                pl = safe_malloc(pload_len); 
                bcopy(TCP(payload), pl, pload_len); 
                if (strstr(pl, opt->content) == NULL)
                    return 0;
                free(pl);
                break;

            default:
                logmsg(LOGWARN, "SNORT: Unknown Snort option\n");
                break;
        }
        opt = opt->next;
    }
    return 1;
}
    
/*
 * -- swap_ip_list
 * -- swap_port_list
 *
 * Auxiliar functions used by check() for bidirectional rules 
 *
 */
    
static void
swap_ip_list(ip_t *src, ip_t *dst)
{
    ip_t aux;
    
    aux = *src;
    *src = *dst;
    *dst = aux;
    
    return;
}
    
static void
swap_port_list(portset_t *src, portset_t *dst)
{
    portset_t aux;

    aux = *src;
    *src = *dst;
    *dst = aux;
    
    return;
}
    
static int
check(pkt_t *pkt)
{
    /* compare the incoming packet with the info
     * obtained from the Snort rules
     */    
    
    int i;
    unsigned int ok;
    fpnode_t *fp;
    
    for (i = 0; i < nrules; i++) {
        /* Check whether the rule matches the packet in the -> direction */
        ok = 1;
        for (fp = ri[i].funcs; fp != NULL && ok; fp = fp->next) {
            ok &= fp->function(&ri[i], pkt);
        }
        if (ok) {
            /* The rule matched the packet in the -> direction
             * If it's a pass rule, we drop the packet */
            if (ri[i].action == SNTOK_PASS) {
                rule_match = -1;
                return 0;
            }
            /* If it's not a pass rule, we accept the packet 
             * and save the rule no. that matched against it */
            else {
                rule_match = i;
                return 1;
            }
        }
        else if (ri[i].bidirectional) {
            /* Swap the source and destination ip's and ports 
             * It's not a problem if we directly modify the ri structure 
             */
            
            swap_ip_list(&(ri[i].src_ips), &(ri[i].dst_ips));
            swap_port_list(&(ri[i].src_ports), &(ri[i].dst_ports));
            
            /* Check whether the rule matches the packet in the <- direction */ 
            ok = 1;
            for (fp = ri[i].funcs; fp != NULL && ok != 0; fp = fp->next) {
                ok &= fp->function(&(ri[i]), pkt);
            }
            if (ok) {
                /* The rule matched the packet in the <- direction
                 * If it's a pass rule, we drop the packet */
                if (ri[i].action == SNTOK_PASS) {
                    rule_match = -1;
                    return 0;
                }
                /* If it's not a pass rule, we accept the packet 
                 * and save the rule no. that matched against it */
                else {
                    rule_match = i;
                    return 1;
                }
            }
        }
    }
    // None of the rules matched the packet
    rule_match = -1;
    return 0;
}

static int
update(pkt_t *pkt, void *fh, int new_rec)
{
    FLOWDESC *x;
    struct snort_hdr *shdr;
    
    x = (FLOWDESC *)(fh);
            
    if (new_rec)
        x->sz = 0;
    
    shdr = (struct snort_hdr *) x->buf; 
    shdr->alert = -1;
    if (rule_match >= 0 && ri[rule_match].action == SNTOK_ALERT)
        shdr->alert = rule_match;
    bcopy(&shdr->pkt, pkt->payload, pkt->caplen); 
    x->sz = pkt->caplen + sizeof(shdr->alert); 

    /* 
     * all records are always full, this way we rely on 
     * CAPTURE in handling the queue of packets. 
     */
    return 1; 
}

static ssize_t
store(void *fh, char *buf, size_t len)
{
    FLOWDESC *x = F(fh);
    
    if (len < x->sz)
        return -1;
    memcpy(buf, x->buf, x->sz);
    return x->sz;
}

static size_t
load(char * buf, size_t len, timestamp_t * ts)
{
    struct snort_hdr *shdr;
    
    /* The buffer that we receive must contain at least the
     * snort_hdr structure, even if it has no packet info 
     */
    if (len < sizeof(struct snort_hdr)) {
        ts = 0;
	return 0;
    }
    shdr = (struct snort_hdr *) buf;
    *ts = shdr->pkt.ts; 
    return (shdr->pkt.caplen + sizeof(shdr->alert));
}


/*
 * -- ts_print
 *
 * Converts a timeval structure into a readable string
 * Taken from tcpdump code and modified
 */
static char * 
ts_print(const struct timeval *tvp)
{
    register int s;
    int    localzone;
    time_t Time;
    struct timeval tv;
    struct timezone tz;
    struct tm *lt;    /* place to stick the adjusted clock data */
    static char timebuf[25];

    /* if null was passed, we use current time */
    if(!tvp)
    {
        /* manual page (for linux) says tz is never used, so.. */
        bzero((char *) &tz, sizeof(tz));
        gettimeofday(&tv, &tz);
        tvp = &tv;
    }

    /* Assume we are using UTC */
    localzone = 0;
        
    s = (tvp->tv_sec + localzone) % 86400;
    Time = (tvp->tv_sec + localzone) - s;
    
    lt = gmtime(&Time);
    
    /* We do not include the year in the string */
    (void) snprintf(timebuf, TIMEBUF_SIZE,
                        "%02d/%02d-%02d:%02d:%02d.%06u ", lt->tm_mon + 1,
                        lt->tm_mday, s / 3600, (s % 3600) / 60, s % 60,
                        (u_int) tvp->tv_usec);
    return timebuf;
}



/*
 * -- create_alert_str
 *
 * utility function used to print a Snort alert
 */
static void 
create_alert_str(struct timeval *t, pkt_t *pkt, int rule, char *s)
{
    char srcip[IP_ADDR_LEN];
    char dstip[IP_ADDR_LEN];
    char srcport[5];
    char dstport[5];
    char proto[5];
    char *timestr;
    
    if (pkt->l3type == ETH_P_IP) {
	timestr = ts_print(t);
	snprintf(srcip, IP_ADDR_LEN + 1, "%s", IP_ADDR(IP(src_ip)));
	snprintf(dstip, IP_ADDR_LEN + 1, "%s", IP_ADDR(IP(dst_ip)));
    
	switch (IP(proto)) { 
        case IPPROTO_TCP:
            sprintf(proto, "tcp");
            sprintf(srcport, "%d", H16(TCP(src_port))); 
            sprintf(dstport, "%d", H16(TCP(dst_port))); 
            break;
        case IPPROTO_UDP:
            sprintf(proto, "udp");
            sprintf(srcport, "%d", H16(UDP(src_port))); 
            sprintf(dstport, "%d", H16(UDP(dst_port))); 
            break;
        case IPPROTO_ICMP:
            sprintf(proto, "icmp");
            sprintf(srcport, "N/A");
            sprintf(dstport, "N/A");
            break;
        case IPPROTO_IP:
            sprintf(proto, "ip");
            sprintf(srcport, "N/A");
            sprintf(dstport, "N/A");
            break;
        default:
            sprintf(proto, "other");
            sprintf(srcport, "N/A");
            sprintf(dstport, "N/A");            
            break;
	}
	sprintf(s, "%s [**] rule no. %d [**] [%s] %s:%s -> %s:%s\n",
                timestr, rule, proto, srcip, srcport, dstip, dstport);
    } else {
	sprintf(s, "%s [**] rule no. %d [**] non-ip %x\n",
		timestr, rule, pkt->l3type);
    }
}
    

#define ALERT_LEN       	4096
#define SNORT_ALERT		0
#define SNORT_LOG		1
#define TCPDUMP_MAGIC  		0xa1b2c3d4

static char *
print(char *buf, size_t *len, char * const args[])
{
    static int task = SNORT_ALERT; 
    static char s[ALERT_LEN];
    struct snort_hdr *shdr;
    struct timeval t;
    
    if (buf == NULL && args != NULL) { 
	int n;

	/* first call, parse the arguments */ 
	for (n = 0; args[n]; n++) {
	    if (!strcmp(args[n], "output=log")) 
		task = SNORT_LOG; 
	    else if (!strcmp(args[n], "output=alert")) 
		task = SNORT_ALERT; 
	} 

#if 0 	// XXX this is to put the pcap header in front of the output
	//     stream. right now we send just a stream made of CoMo pkts. 
	//     this needs fixing... 
	if (task == SNORT_LOG) { 
	    /* 
	     * send the pcap header out
	     * 
	     * XXX TODO: snaplen and linktype should vary depending on 
	     * the sniffer we use (pass them as init parameters?) 
	     */
	    struct pcap_pkthdr fhdr; 

	    fhdr.magic = TCPDUMP_MAGIC;
	    fhdr.version_major = PCAP_VERSION_MAJOR;
	    fhdr.version_minor = PCAP_VERSION_MINOR;
	    fhdr.thiszone = 0;    // Time Zone Offset - gmt to local correction
	    fhdr.snaplen = 65535; /* Maximum number of captured bytes per packet
				   * We are capturing entire packets, so we put
				   * here the max IPv4 MTU */
	    fhdr.sigfigs = 0;     // Time Stamp Accuracy
	    fhdr.linktype = 1;    // Ethernet
	
	    *len = sizeof(fhdr);
	    return (char *)&fhdr;
	}
#endif

	*len = 0; 
	return s; 
    }
    
    if (buf == NULL && args == NULL) { 	/* done with the records */
	*len = 0; 	
	return s; 
    }

    /* get the snort packet header */
    shdr = (struct snort_hdr *)buf;
    t.tv_sec = TS2SEC(shdr->pkt.ts); 
    t.tv_usec = TS2USEC(shdr->pkt.ts); 
    
    if (task == SNORT_ALERT) { 
        /* if the packet did not match against an alert rule,
         * do not print it (return an empty string) */
        if (shdr->alert < 0) {
            *len = 0;
        } else {
	    pkt_t pkt; 

            /* create the alert string and return it */        
  	    bcopy(&shdr->pkt, &pkt, sizeof(pkt_t)); 
            create_alert_str(&t, &pkt, shdr->alert, s);
            *len = strlen(s);
        }
	return s;
    } 

    /* just send the entire packet */
    *len = shdr->pkt.caplen; 
    return buf + sizeof(int);
}


#if 0  	/* XXX replay still doesn't work... */
static int
replay(char *buf, char *out, size_t * len)
{
    struct snort_hdr *shdr;
    int off;
    pkt_t p;
    
    if (max <= 0)
        return -1; /* run-time error */
    
    /* build a pkt_t structure from the buffer */
    shdr = (struct snort_hdr *)buf;
    
    memset(&p, 0, sizeof(pkt_t));
    p.ts = TIME2TS(shdr->pkthdr.ts.tv_sec, shdr->pkthdr.ts.tv_usec);    
    p.caplen = 0; /* caplen is set to 0 to avoid that sniffer-dump.c
                     continues reading after the pkt_t structure */
    p.len = shdr->pkthdr.len;
    
    off = sizeof(*shdr);    
    off += OFFCOPY(&p.mach, buf + off, MAC_H_SIZE);
    off += OFFCOPY(&p.ih,   buf + off,  IP_H_SIZE);
    off += OFFCOPY(&p.p,    buf + off, APP_H_SIZE);
    
    out[0] = p;

    return 1;
}
#endif

callbacks_t callbacks = {
    ca_recordsize: sizeof(FLOWDESC),
    ex_recordsize: 0, 
    indesc: NULL, 
    outdesc: NULL, 
    init: init,
    check: check,
    hash: NULL,
    match: NULL,
    update: update,
    ematch: NULL,
    export: NULL,
    compare: NULL,
    action: NULL,
    store: store,
    load: load,
    print: print,
    replay: NULL 
};

