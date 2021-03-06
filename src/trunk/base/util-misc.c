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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>     
#include <netdb.h>                     /* gethostbyname */
#include <assert.h>
#include <ctype.h>

#include "como.h"


/* 
 * -- getprotoname
 * 
 * Our custom getprotobyname()-like function.
 *
 * Given that we are at it we also fix the problem with /etc/protocols 
 * in Linux that gives uncommon names to common protocols (e.g., protocol 
 * number 50 is better known as ESP than IPV6-CRYPT). 
 * 
 * we know the name of the first 133 protocols.
 */

char * 
alias[256] = { 
    "ip","icmp","igmp","ggp","ipencap","st2","tcp","cbt","egp",
    "igp","bbn-rcc","nvp","pup","argus","emcon","xnet","chaos",
    "udp","mux","dcn","hmp","prm","xns-idp","trunk-1","trunk-2",
    "leaf-1","leaf-2","rdp","irtp","iso-tp4","netblt","mfe-nsp",
    "merit-inp","sep","3pc","idpr","xtp","ddp","idpr-cmtp","tp++",
    "il","ipv6","sdrp","ipv6-route","ipv6-frag","idrp","rsvp","gre",
    "mhrp","bna","esp","ah","i-nlsp","swipe","narp","mobile","tlsp",
    "skip","ipv6-icmp","ipv6-nonxt","ipv6-opts","cftp","sat-expak",
    "kryptolan","rvd","ippc","sat-mon","visa","ipcv","cpnx","cphb",
    "wsn","pvp","br-sat-mon","sun-nd","wb-mon","wb-expak","iso-ip",
    "vmtp","secure-vmtp","vines","ttp","nsfnet-igp","dgp","tcf",
    "eigrp","ospf","sprite-rpc","larp","mtp","ax.25","ipip","micp",
    "scc-sp","etherip","encap","99","gmtp","ifmp","pnni","pim","aris",
    "scps","qnx","a/n","ipcomp","snp","compaq-peer","ipx-in-ip",
    "carp","pgm","l2tp","ddx","iatp","st","srp","uti","smp","sm",
    "ptp","isis","fire","crtp","crudp","sscopmce","iplt","sps",
    "pipe","sctp","fc","134","135","136","137","138","139","140",
    "141","142","143","144","145","146","147","148","149","150",
    "151","152","153","154","155","156","157","158","159","160",
    "161","162","163","164","165","166","167","168","169","170",
    "171","172","173","174","175","176","177","178","179","180",
    "181","182","183","184","185","186","187","188","189","190",
    "191","192","193","194","195","196","197","198","199","200",
    "201","202","203","204","205","206","207","208","209","210",
    "211","212","213","214","215","216","217","218","219","220",
    "221","222","223","224","225","226","227","228","229","230",
    "231","232","233","234","235","236","237","238","239","pfsync",
    "241","242","243","244","245","246","247","248","249","250",
    "251","252","253","254","255"};

char *
getprotoname(int proto) 
{
    return alias[proto]; 
}


/*
 * strchug and strchomp adapted from eglib
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *   Aaron Bockover (abockover@novell.com)
 *
 * (C) 2006 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

char *
strchug(char *str)
{
    int len;
    char *tmp;

    if (str == NULL)
	return NULL;

    tmp = str;
    while (*tmp && isspace (*tmp)) tmp++;
    if (str != tmp) {
	len = strlen (str) - (tmp - str - 1);
	memmove (str, tmp, len);
    }
    return str;
}

char *
strchomp(char *str)
{
    char *tmp;

    if (str == NULL)
	return NULL;

    tmp = str + strlen (str) - 1;
    while (*tmp && isspace (*tmp)) tmp--;
    *(tmp + 1) = '\0';
    return str;
}
