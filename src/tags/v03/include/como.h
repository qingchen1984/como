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

#ifndef _COMO_COMO_H
#define _COMO_COMO_H

#include <sys/time.h>   /* struct timeval */

#include "comotypes.h"

/*
 * Data structure containing all the configuration parameters
 * loaded from the default config file(s) and the command line.
 * This data structure will contain the information about all the
 * modules and classifiers as well.
 */
struct _como {
    char * procname;    /* process using this instance */
    char * basedir;     /* base directory for output files */
    char * libdir;      /* base directory for classifiers */
    char * workdir;	/* work directory for templates etc. */
    size_t mem_size;    /* memory size for capture/export (MB) */
    int logflags;       /* log flags (i.e., what to log) */

    stats_t * stats; 	/* statistic counters */

    source_t *sources;	/* list of input data feeds */

    char * template;    /* file for compiling the filter on the fly */
    char * filter;      /* dedicated library for filter */

    module_t * modules; /* array of modules */ 
    int module_max;  	/* max no. of modules */
    int module_count;   /* current no. of modules */

    size_t maxfilesize; /* max file size in one bytestream */

    int maxqueries;
    int query_port;

    int supervisor_fd;	/* util routines etc */

    char *debug;		/* debug mode */
	/*
	 * here we simply store a malloced copy of the string
	 * passed as -x from the command line, then each
	 * process decides what to do with it, with some string
	 * matching function.
	 */
};


/*---  Prototypes -----*/

/*
 * config.c
 *
 */
int parse_cmdline(int argc, char *argv[]);

/*
 * memory.c
 */
void dump_alloc(void);
int mem_merge_maps(memlist_t *dst, memlist_t *m);
void *new_mem(memlist_t *m, uint size, char *msg); 
void mfree_mem(memlist_t *m, void *p, uint size); 
void memory_init(uint size_mb);     
void memory_clear(void);        
uint memory_usage(void);
uint memory_peak(void);
memlist_t *new_memlist(uint entries); 

/*
 * capture.c
 */
void capture_mainloop(int fd);

/*
 * export.c
 */
void export_mainloop(int fd);

/*
 * supervisor.c
 */
void supervisor_mainloop(int fd);
pid_t start_child(char *name, char *procname, void (*mainloop)(int fd), int fd);
int get_sp_fd(char *name);

/*
 * util.c
 */
int create_socket(const char *name, char **arg);
int como_readn(int fd, char *buf, size_t len);
int como_writen(int fd, const char *buf, size_t len);
void logmsg(int flags, const char *fmt, ...);
void rlimit_logmsg(unsigned interval, int flags, const char *fmt, ...);
char * loglevel_name(int flags); 

/*
 * panic() and panicx() are a printf-like function to print a panic message
 * and terminate.
 */
void _epanic(const char * file, const int line, const char *fmt, ...);
#define panic(...) _epanic(__FILE__, __LINE__, __VA_ARGS__)
void _epanicx(const char * file, const int line, const char *fmt, ...);
#define panicx(...) _epanicx(__FILE__, __LINE__, __VA_ARGS__)

/*
 * If possible, do not call malloc(),  calloc() and realloc() directly.
 * Instead use safe_malloc(), safe_calloc() and safe_realloc() which provide
 * wrappers to check the arguments and panic if necessary.
 */
void *_smalloc(const char * file, const int line, size_t sz); 
#define safe_malloc(...) _smalloc(__FILE__, __LINE__, __VA_ARGS__) 
void *_scalloc(const char * file, const int line, int n, size_t sz); 
#define safe_calloc(...) _scalloc(__FILE__, __LINE__, __VA_ARGS__) 
void *_srealloc(const char * file, const int line, void * ptr, size_t sz); 
#define safe_realloc(...) _srealloc(__FILE__, __LINE__, __VA_ARGS__) 

void *load_object(char *base_name, char *symbol);

/* log flags. The high bits are only set for verbose logging */
#define	LOGUI		0x0001	/* normal warning msgs			*/
#define	LOGWARN		0x0002	/* normal warning msgs			*/
#define	LOGMEM		0x0004	/* memory.c debugging			*/
#define	LOGCONFIG	0x0008	/* config.c debugging			*/
#define	LOGCAPTURE	0x0010
#define	LOGEXPORT	0x0020
#define	LOGSTORAGE	0x0040
#define	LOGQUERY	0x0080
#define	LOGSNIFFER	0x0100	/* sniffers debugging 			*/
#define	LOGDEBUG	0x8000
#define	LOGALL		(LOGUI|LOGWARN|LOGMEM|LOGCONFIG|LOGCAPTURE| \
				LOGEXPORT|LOGSTORAGE|LOGQUERY|LOGDEBUG| \
				LOGSNIFFER)

#define	V_LOGUI		(LOGUI << 16) 
#define	V_LOGWARN	(LOGWARN << 16)
#define	V_LOGMEM	(LOGMEM << 16)
#define	V_LOGCONFIG	(LOGCONFIG << 16)
#define	V_LOGCAPTURE	(LOGCAPTURE << 16)
#define	V_LOGEXPORT	(LOGEXPORT << 16)
#define	V_LOGSTORAGE	(LOGSTORAGE << 16)
#define	V_LOGQUERY	(LOGQUERY << 16)
#define	V_LOGSNIFFER	(LOGSNIFFER << 16)
#define	V_LOGDEBUG	(LOGDEBUG << 16)

/*
 * default values 
 */
#define DEFAULT_CFGFILE 	"como.conf"	/* configuration file */
#define DEFAULT_STREAMSIZE 	(1024*1024*1024)/* bytestream size */
#define DEFAULT_FILESIZE 	(128*1024*1024)	/* single file size in stream */
#define DEFAULT_BLOCKSIZE 	4096		/* block size */
#define DEFAULT_MODULE_MAX	128		/* max no. modules */
#define DEFAULT_LOGFLAGS	(LOGUI|LOGWARN) /* log messages */
#define DEFAULT_MEMORY		64		/* memory capture/export */
#define DEFAULT_QUERY_PORT	44444		/* query port */
#define DEFAULT_MAXCAPTUREIVL	TIME2TS(1,0)    /* max capture flush interval */
#define DEFAULT_MINCAPTUREIVL	0 		/* min capture flush interval */
#define DEFAULT_REPLAY_BUFSIZE	(1024*1024)	/* replay packet trace buffer */


#endif /* _COMO_COMO_H */
