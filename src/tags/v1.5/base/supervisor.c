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
#include <unistd.h>     /* mkstemp */
#include <sys/wait.h>	/* wait3() */
#include <string.h>     /* bzero */
#include <errno.h>      /* errno */
#include <err.h>	/* errx */
#include <assert.h>

#include "como.h"
#include "comopriv.h"
#include "storage.h"
#include "query.h"	// XXX query();
#include "ipc.h"


/* global state */
extern struct _como map;


/*
 * -- ipc_echo_handler()
 *
 * processes IPC_ECHO messages by printing their content to screen 
 * and to file if required.  
 * 
 * XXX the logfile should go thru STORAGE to allow for circular 
 *     files and better disk scheduling.
 * 
 */
static void 
su_ipc_echo(procname_t sender, void * buf, 
	    __attribute__((__unused__)) size_t len) 
{
    logmsg_t *lmsg = (logmsg_t *) buf;
    displaymsg(stdout, sender, lmsg);
    if (map.logfile != NULL) 
	displaymsg(map.logfile, sender, lmsg);
}


/*
 * -- ipc_sync_handler()
 * 
 * on receipt of an IPC_SYNC, send an IPC_MODULE_ADD for all 
 * active modules in the map. this is used to synchronize the 
 * SUPERVISOR with another process that has just started on 
 * which modules should be running. 
 * 
 */
static void
su_ipc_sync(procname_t sender, __attribute__((__unused__)) void * b, 
	    __attribute__((__unused__)) size_t l)
{
    int i; 
    
    /* 
     * initialize all modules and send the start signal to 
     * the other processes 
     */
    for (i = 0; i <= map.module_last; i++) { 
        char * pack;
        int sz;

	if (map.modules[i].status != MDL_ACTIVE) 
	    continue; 

	if (getprocclass(sender) != QUERY && 
	    map.modules[i].running == RUNNING_ON_DEMAND)
	    continue; 

        /* prepare the module for transmission */
        pack = pack_module(&map.modules[i], &sz);

        /* inform the other processes */
        ipc_send(sender, IPC_MODULE_ADD, pack, sz);

        free(pack);
    }

    /* now we can send the start message */
    ipc_send(sender, IPC_MODULE_START, &map.stats, sizeof(void *));
}


/*  
 * -- su_ipc_record 
 * 
 * prints a record to screen. this is used only when running
 * in inline mode 
 *
 */
static void
su_ipc_record(__attribute__((__unused__)) procname_t sender, void * buf,
        __attribute__((__unused__)) size_t l)
{
    assert(map.runmode == RUNMODE_INLINE);  
    fprintf(stdout, "%s", (char *) buf); 
}


/*  
 * -- su_ipc_done 
 * 
 * EXPORT should send this message to report that there are 
 * no more records to be processed. This messages happens only
 * in inine mode. As a result we exit (sending a SIGPIPE to all 
 * children as well).
 *
 */
static void
su_ipc_done(__attribute__((__unused__)) procname_t sender,
	__attribute__((__unused__)) void * b,
        __attribute__((__unused__)) size_t l)
{
    ipc_send(CAPTURE, IPC_EXIT, NULL, 0); 
    ipc_send(EXPORT, IPC_EXIT, NULL, 0); 
    ipc_send(STORAGE, IPC_EXIT, NULL, 0); 
    exit(EXIT_SUCCESS);
}


/*
 * -- cleanup
 * 
 * cleanup() called at termination time to
 * remove the byproducts of the compilation.
 * The function is registered with atexit(), which does not provide  
 * a way to unregister the function itself. So we have to check which
 * process died (through map.procname) and determine what to do accordingly.
 */
static void
cleanup()
{
    char *cmd;
 
    if (map.whoami != SUPERVISOR) {
        logmsg(V_LOGWARN, "%s terminating normally\n", 
		getprocfullname(map.whoami));
        return;
    }
    map.logflags |= LOGUI; /* make sure the message is visible */
    logmsg(LOGUI, "\n\n\n--- about to exit... remove work directory %s\n",
        map.workdir);
    asprintf(&cmd, "rm -rf %s\n", map.workdir);
    system(cmd);
    logmsg(LOGUI, "--- done, thank you for using como\n");
}
  

/*
 * -- apply_map_changes
 *
 * compare two map data structures (struct _como) to verify
 * if anything relevant has changed. any change that we can
 * accomodate will be applied immediately.
 *
 * XXX right now we only allow to add or remove modules.
 *     it should be possible to modify more configuration
 *     parameters on-the-fly.
 * 
 * XXX this function doesn't handle multiple virtual nodes. 
 *
 */
static void
apply_map_changes(struct _como * x)
{
    int i, j;

    /*
     * browse thru the list of modules to find if all
     * modules in the running config are also present
     * in the new one. if not, remove them.
     */
    for (i = 0; i <= map.module_last; i++) {
	int found = 0; 

	if (map.modules[i].status == MDL_UNUSED) 
	    continue; 

        for (j = 0; j <= x->module_last; j++) {
            if (match_module(&x->modules[j], &map.modules[i])) { 
		/* don't need to look at this again */
		x->modules[j].status = MDL_UNUSED; 
		found = 1;
                break;
	    } 
        }
    
        if (!found) {
            remove_module(&map, &map.modules[i]);
            
            if (map.modules[i].running != RUNNING_ON_DEMAND) {
		/* inform the other processes */
		ipc_send(CAPTURE, IPC_MODULE_DEL, (char *) &i, sizeof(int)); 
		ipc_send(EXPORT, IPC_MODULE_DEL, (char *) &i, sizeof(int)); 
		ipc_send(STORAGE, IPC_MODULE_DEL, (char *) &i, sizeof(int)); 
            }

	    if (map.modules[i].status == MDL_ACTIVE && 
		map.modules[i].running != RUNNING_ON_DEMAND) { 
		map.stats->modules_active--; 
	    }
	    
	    if (map.modules[i].indesc || map.modules[i].outdesc) {
		/* free metadesc information. to do this 
		 * we need to freeze CAPTURE for a while 
		 */ 
		/* CHECKME: shouldn't this code be executed in any case? */
		ipc_send_blocking(CAPTURE, IPC_FREEZE, NULL, 0);
	        metadesc_list_free(map.modules[i].indesc);
		metadesc_list_free(map.modules[i].outdesc);
		ipc_send(CAPTURE, IPC_ACK, NULL, 0); 
	    } 

	}
    }

    /*
     * now add any modules in the new map that do
     * not exist in the old map
     */
    for (j = 0; j <= x->module_last; j++) {
        module_t * mdl;
	int sz; 

        if (x->modules[j].status == MDL_UNUSED)
            continue;

	/* add this module to the main map */
        mdl = copy_module(&map, &x->modules[j], x->modules[j].node, -1, NULL);
	if (activate_module(mdl, map.libdir)) {
	    remove_module(&map, mdl);
	    continue;
	} 

	/* if this module is not running on demand, initialize it. 
	 * however, before doing so freeze CAPTURE to avoid conflicts 
         * in the shared memory
	 */
	if (mdl->running != RUNNING_ON_DEMAND) {
	    char * pack;

	    ipc_send_blocking(CAPTURE, IPC_FREEZE, NULL, 0);
	    if (init_module(mdl)) { 
		/* let CAPTURE resume */
		ipc_send(CAPTURE, IPC_ACK, NULL, 0); 
		remove_module(&map, mdl);
		continue;
	    } 
	
	    /* prepare the module for transmission */
	    pack = pack_module(mdl, &sz);

	    /* inform the other processes */
	    ipc_send(CAPTURE, IPC_MODULE_ADD, pack, sz); 
	    ipc_send(EXPORT, IPC_MODULE_ADD, pack, sz); 
	    ipc_send(STORAGE, IPC_MODULE_ADD, pack, sz); 
	    map.stats->modules_active++; 

	    free(pack);
	}

    }
}


/*
 * -- reconfigure
 * 
 * this is called when a SIGHUP is received to cause
 * como process again the config files and command line
 * parameters.
 *
 */
static void
reconfigure(__attribute__((__unused__)) int si_code)
{
    struct _como tmp_map;

    init_map(&tmp_map);
    tmp_map.cli_args = map.cli_args;
    configure(&tmp_map, map.ac, map.av);
    apply_map_changes(&tmp_map);
}

/*
 * -- defchld
 * 
 * handle dead children
 * 
 */
static void
defchld(__attribute__((__unused__)) int si_code)
{
    handle_children();
}


/*
 * -- supervisor_mainloop
 * 
 * Basically mux incoming messages and show them to the console.
 * Also take care of processes dying. XXX update this comment
 */
void
supervisor_mainloop(int accept_fd)
{
    fd_set valid_fds;
    int * external_fd;	/* for http queries */
    int max_fd;
    int i; 
    
    /* catch some signals */
    signal(SIGINT, exit);               /* catch SIGINT to clean up */
    signal(SIGTERM, exit);              /* catch SIGTERM to clean up */
    signal(SIGCHLD, defchld);		/* catch SIGCHLD (defunct children) */
    if (map.runmode == RUNMODE_NORMAL)
	signal(SIGHUP, reconfigure);    /* catch SIGHUP to update config */

    /* register a handler for exit */
    atexit(cleanup);

    /* register handlers for IPC */
    ipc_clear();
    ipc_register(IPC_ECHO, su_ipc_echo); 
    ipc_register(IPC_SYNC, su_ipc_sync); 
    ipc_register(IPC_RECORD, su_ipc_record); 
    ipc_register(IPC_DONE, su_ipc_done); 

    if (map.runmode == RUNMODE_INLINE) {
        inline_mainloop(accept_fd); 
	return; 
    } 

    max_fd = 0;
    FD_ZERO(&valid_fds);

    /* accept connections from other processes */
    max_fd = add_fd(accept_fd, &valid_fds, max_fd);

    /* 
     * create sockets and accept connections from the outside world. 
     * they are queries that can be destined to any of the virtual nodes. 
     */
    external_fd = safe_calloc(map.node_count, sizeof(int));
    for (i = 0; i < map.node_count; i++) { 
	char *buf;

	asprintf(&buf, "S:http://%s:%d/", map.node[i].query_address,
		 map.node[i].query_port);
	external_fd[i] = create_socket(buf, NULL);
	if (external_fd[i] < 0)
	    panic("creating the socket %s", buf);
	max_fd = add_fd(external_fd[i], &valid_fds, max_fd);
	free(buf);
    }

    /* initialize all modules */ 
    for (i = 0; i <= map.module_last; i++) {
	module_t * mdl = &map.modules[i]; 

	if (mdl->status != MDL_LOADING) 
	    continue;

	if (activate_module(mdl, map.libdir)) {
	    logmsg(LOGWARN, "cannot start module %s\n", mdl->name); 
	    remove_module(&map, mdl);
	    continue; 
  	} 

	if (mdl->running != RUNNING_ON_DEMAND) {
	    if (init_module(mdl)) { 
		logmsg(LOGWARN, "cannot initialize module %s\n", mdl->name); 
		remove_module(&map, mdl);
		continue; 
	    } 

	    map.stats->modules_active++;
	}
    } 

    /* initialize resource management */
    resource_mgmt_init();

    for (;;) { 
#ifdef RESOURCE_MANAGEMENT
	/* 
	 * to do resource management we need the select timeout to 
	 * be shorter so that supervisor can be more reactive to 
	 * sudden changes in the resource usage. 
	 * 
	 * XXX shouldn't the other processes be more reactive and 
	 *     supervisor just wait for an "heads up" from them? 
	 */
	struct timeval to = {0, 50000};
#else
	struct timeval to = {1, 0};
#endif
        int secs, dd, hh, mm, ss;
	struct timeval now;
	int n_ready;
	int ipcr;
	
	fd_set r = valid_fds;

	/* 
         * user interface. just one line... 
         */
	if (!map.silent) { 
	    gettimeofday(&now, NULL);
	    secs = 1 + now.tv_sec - map.stats->start.tv_sec; 
	    dd = secs / 86400; 
	    hh = (secs % 86400) / 3600; 
	    mm = (secs % 3600) / 60;
	    ss = secs % 60; 

	    if (map.runmode == RUNMODE_NORMAL) {
		fprintf(stderr, 
		    "\r- up %dd%02dh%02dm%02ds; mem %u/%u/%u/%uMB (%d); "
		    "pkts %llu drops %d; mdl %d/%d\r", 
		    dd, hh, mm, ss,
			(unsigned int)map.stats->mem_usage_cur/(1024*1024), 
			(unsigned int)map.stats->mem_waste_cur/(1024*1024), 
			(unsigned int)map.stats->mem_usage_peak/(1024*1024), 
			(unsigned int)map.mem_size, map.stats->table_queue,
		    map.stats->pkts, map.stats->drops,
		    map.stats->modules_active, map.module_used);
	    }
	} 

	n_ready = select(max_fd, &r, NULL, NULL, &to);

	if (!map.silent && map.runmode == RUNMODE_NORMAL) {
		fprintf(stderr, "%78s\r", ""); /* clean the line */
        } 

	for (i = 0; n_ready > 0 && i < max_fd; i++) {
	    int id;
	    
	    if (!FD_ISSET(i, &r))
		continue;

	    if (i == accept_fd) {
		int x;
		x = accept(i, NULL, NULL);
		if (x < 0) {
		    logmsg(LOGWARN, "accept fd[%d] got %d (%s)\n",
			    i, x, strerror(errno));
		} else {
		    max_fd = add_fd(x, &valid_fds, max_fd);
		}
		goto next_one;
	    }

	    for (id = 0; id < map.node_count; id++) {
		struct sockaddr_in addr;
		procname_t who; 
		socklen_t len;
		int cd;
	    
		if (i != external_fd[id]) 
		    continue; 

		len = sizeof(addr);
		cd = accept(external_fd[id], (struct sockaddr *)&addr, &len); 
		if (cd < 0) {
		    /* check if accept was unblocked by a signal */
		    if (errno == EINTR)
			continue;

		    logmsg(LOGWARN, "accepting connection: %s\n",
			strerror(errno));
		}

		logmsg(LOGQUERY,
		       "query from %s on fd %d\n", 
		       inet_ntoa(addr.sin_addr), cd); 

		/* 
		 * start a query process. 
	   	 */
		who = buildtag(map.whoami, QUERY, cd);
		start_child(who, COMO_PRIVATE_MEM, query, cd, id); 
		close(cd);
		goto next_one;
	    }

	    /* this is internal. use ipc handler */
	    ipcr = ipc_handle(i);
	    switch (ipcr) {
	    case IPC_ERR:
		/* an error. close the socket */
		logmsg(LOGWARN, "error on IPC handle from %d\n", i);
	    case IPC_EOF:
		close(i);
		max_fd = del_fd(i, &valid_fds, max_fd);
		break;
	    }
  next_one:
	    n_ready--;
	}
	schedule(); /* resource management */
    }
}

