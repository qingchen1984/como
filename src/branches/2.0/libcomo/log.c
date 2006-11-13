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
 * $Id:log.c 1009 2006-11-13 13:39:55 +0000 (Mon, 13 Nov 2006) m_canini $
 *
 * This is the logging facility. 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "como.h"


typedef struct log_handler {
    char *	domain;
    log_fn	user_fn;
    void *	user_data;
} log_handler_t;


static void
default_handler(const char * domain, log_level_t level,
		const char * message, struct timeval tv,
		UNUSED void * user_data)
{
    FILE *o;
    char *pre;

    if (level & LOG_LEVEL_ERROR) {
	o = stderr;
	pre = "FATAL ERROR: ";
    } else if (level & LOG_LEVEL_WARNING) {
	o = stderr;
	pre = "WARNING: ";
    } else {
	o = stdout;
	pre = "";
    }

    if (domain) {
	fprintf(o, "[%5ld.%06ld %s] %s%s",
		tv.tv_sec % 86400, (long int) tv.tv_usec,
		domain, pre, message);
    } else {
	fprintf(o, "[%5ld.%06ld] %s%s",
		tv.tv_sec % 86400, (long int) tv.tv_usec,
		pre, message);
    }
}

static log_handler_t s_initial_handler = {
    domain: NULL,
    user_fn: default_handler,
    user_data: NULL
};

/* static state */
static struct log_static_state {
    log_level_t		level;
    log_handler_t *	handlers;
    int			handlers_count;
    char *		buf;
    size_t		buf_len;
} s_log = {
    level: LOG_LEVEL_ERROR | LOG_LEVEL_WARNING |
	   LOG_LEVEL_NOTICE | LOG_LEVEL_MESSAGE | LOG_LEVEL_DEBUG,
    buf: NULL,
    buf_len: 0,
    handlers: &s_initial_handler,
    handlers_count: 1
};


void
log_set_level(log_level_t level)
{
    s_log.level = level;
}


log_level_t
log_get_level()
{
    return s_log.level;
}


static inline log_handler_t *
log_handler_lookup(const char * domain)
{
    log_handler_t *h;
    int i;
    
    if (s_log.handlers == NULL)
	return NULL;

    for (h = s_log.handlers, i = 0; i < s_log.handlers_count; h++, i++) {
	if (domain == NULL && h->domain == NULL)
	    return h;
	if (h->domain != NULL && strcmp(h->domain, domain) == 0)
	    return h;
    }
    
    return s_log.handlers;
}


void
log_set_handler(const char * domain, log_fn user_fn, void * user_data)
{
    log_handler_t *h;

    if (user_fn == NULL)
	return;

    h = log_handler_lookup(domain);
    if (h == NULL) {
	s_log.handlers_count++;
	if (s_log.handlers != &s_initial_handler) {
	    s_log.handlers = como_realloc(s_log.handlers, s_log.handlers_count *
					sizeof(log_handler_t));
	} else {
	    s_log.handlers = como_malloc(s_log.handlers_count *
					sizeof(log_handler_t));
	    s_log.handlers[0] = s_initial_handler;
	}
	h = &s_log.handlers[s_log.handlers_count - 1];
    }
    h->domain = como_strdup(domain);
    h->user_fn = user_fn;
    h->user_data = user_data; 
}


void
log_out(const char *domain, log_level_t level,
	const char *format, ...)
{
    va_list ap;
    size_t len;
    struct timeval tv;
    log_handler_t *h;

    if (!(s_log.level & level))
        return;
    
    gettimeofday(&tv, NULL);
    
    va_start(ap, format);
    
    /* format the message */
    len = 1 + vsnprintf(s_log.buf, s_log.buf_len, format, ap);
    
    /* manage the message buffer */
    if (len > s_log.buf_len) {
    	if (s_log.buf_len * 2 > len) {
	    s_log.buf_len *= 2;
    	} else {
	    s_log.buf_len = len;
	    if (s_log.buf_len % 4) {
		s_log.buf_len += 4 - s_log.buf_len % 4;
	    }
    	}
    	s_log.buf = como_realloc(s_log.buf, s_log.buf_len);
	/* CHECKME: need va_end, va_start? */
	len = 1 + vsnprintf(s_log.buf, s_log.buf_len, format, ap);
    }
    
    h = log_handler_lookup(domain);
    h->user_fn(domain, level, s_log.buf, tv, h->user_data);
    
    va_end(ap);
    
    if (level & LOG_LEVEL_ERROR)
	abort();
}


void
log_outv(const char *domain, log_level_t level, const char *format, va_list ap)
{
    size_t len;
    struct timeval tv;
    log_handler_t *h;

    if (!(s_log.level & level))
        return;
    
    gettimeofday(&tv, NULL);
    
    /* format the message */
    len = 1 + vsnprintf(s_log.buf, s_log.buf_len, format, ap);
    
    /* manage the message buffer */
    if (len > s_log.buf_len) {
    	if (s_log.buf_len * 2 > len) {
	    s_log.buf_len *= 2;
    	} else {
	    s_log.buf_len = len;
	    if (s_log.buf_len % 4) {
		s_log.buf_len += 4 - s_log.buf_len % 4;
	    }
    	}
    	s_log.buf = como_realloc(s_log.buf, s_log.buf_len);
	len = 1 + vsnprintf(s_log.buf, s_log.buf_len, format, ap);
    }
    
    h = log_handler_lookup(domain);
    h->user_fn(domain, level, s_log.buf, tv, h->user_data);

    if (level & LOG_LEVEL_ERROR)
	abort();
}

/** 
 * -- log_level_name
 * 
 * Returns a string with the log message level. 
 * 
 */
char *
log_level_name(log_level_t level)
{
    static char s[64];
    char *name = s;

    if (level & LOG_LEVEL_ERROR)
	name += sprintf(name, "ERROR ");

    if (level & LOG_LEVEL_WARNING)
	name += sprintf(name, "WARNING ");

    if (level & LOG_LEVEL_NOTICE)
	name += sprintf(name, "NOTICE ");

    if (level & LOG_LEVEL_MESSAGE)
	name += sprintf(name, "MESSAGE ");

    if (level & LOG_LEVEL_DEBUG)
	name += sprintf(name, "DEBUG ");

    *(name - 1) = '\0';
    
    return s;
}
