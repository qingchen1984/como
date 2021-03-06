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
 * $Id$ 
 */

/*
 * Author: Diego Amores Lopez (damores@ac.upc.edu)
 * 
 * Description:
 * ------------
 *  
 * Filter parsing for CoMo - lexic file
 *
 * Here we define the tokens that can appear in a CoMo filter.
 * GNU Flex turns this file into a C program that can get one token at a time
 * from the input and send them to the Bison-generated parser.
 */

%{

/* C Declarations */

%}

/* Tokens for the filter parser:
 * The tokens that can appear in a CoMo filter are not completely defined yet.
 * This section should be modified as they change
 */

DIGIT           [0-9]
ALFANUMERIC     [a-zA-Z0-9]
DIR             src|dst
PORTDIR         sport|dport
IPADDR          ({DIGIT}{1,3}"."){3}{DIGIT}{1,3}
NETMASK         "/"({DIGIT}){1,2}
PORT            {DIGIT}+
LEVEL3          ip
LEVEL4          tcp|udp|icmp

%option noyywrap
%option nounput

/* Actions to be executed when a token is found:
 * 
 * Here we need to specify, for each possible token, what is the information
 * that we want to pass to the syntactic parser.
 *
 * Variables that are available in this section:
 *
 * - yytext is the string read from the input
 * - yyleng contains yytext length
 * - yflval is the value that will be passed to the syntactic parser.
 *   new data types can be added to it in base/filter-syntax.y (%union section)
 */

%%

{DIR}           {
                if (!strcmp(yytext, "src"))
                    yflval.byte = 0;
                else /* dst */
                    yflval.byte = 1;
                return(DIR);
                }
{PORTDIR}       {
                if (!strcmp(yytext, "sport"))
                    yflval.byte = 0;
                else /* dport */
                    yflval.byte = 1;
                return(PORTDIR);
                }
{IPADDR}        { 
                /* Copy the dots and numbers notation string */
                yflval.string = safe_strdup(yytext);
                return(IPADDR); 
                }
{NETMASK}       { 
                /* Copy the netmask number, get rid of the slash */ 
                yflval.dword = atoi(yytext + 1);
                return(NETMASK); 
                }
{PORT}          {
                /* Copy the port number */
                yflval.word = atoi(yytext);
                return(PORT); 
                }
{LEVEL3}        {
                /* For now there is only the IP protocol */
                yflval.word = ETHERTYPE_IP;
                return(LEVEL3);
                }
{LEVEL4}        {
                if (!strcmp(yytext, "tcp"))
                    yflval.word = IPPROTO_TCP;
                else if (!strcmp(yytext, "udp"))
                    yflval.word = IPPROTO_UDP;
                else /* icmp */
                    yflval.word = IPPROTO_ICMP;
                return(LEVEL4);
                }
and             { return(AND); }
or              { return(OR); }
not             { return(NOT); }
\(              { return(OPENBR); }
\)              { return(CLOSEBR); }
":"             { return(COLON); }

all             { return(ALL); }

[ \t\n]+

.

%%
