/*-
 * Copyright (c) 2005 - Intel Corporation 
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
 *
 */

/*
 *
This file implements the client side for processes trying to
access files in CoMo. The interface to the storage module has
four methods, and provides an mmap-like interface.

  int csopen(const char * name, int mode, off_t size, int sd)

	name		is the file name
	mode		is the access mode, CS_READER or CS_WRITER
 	size		is the max bytestream size (CS_WRITER only)
	sd		is the (unix-domain) socket used to talk to the
			daemon supplying the service.

	Returns an integer to be used as a file descriptor,
	or -1 on error.

  off_t csgetofs(int fd)

	fd		is the file descriptor

	Returns the offset in the current file.

  void *csmap(int fd, off_t ofs, ssize_t * sz)

	fd		is the file descriptor
	ofs		is the offset that we want to map
	*sz		on input, the desired size;
			on output, the actual size;

	Flushes any unmapped block, and returns a new one.
	Returns a pointer to the mapped region.

  off_t csseek(int fd)

	fd		is the file descriptor

	Moves to the beginning of the next file, unmapping any
	mapped region.

  void csclose(int fd)  

	fd		is the file descriptor

	Flushes any unmapped block, and closes the descriptor.
  
 *
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>


#include "como.h"
#include "storage.h"

/* 
 * Client-side file descriptor 
 * 
 * This data structure contains the minimal information a client
 * needs to know about the open file, i.e., the id, the current offset, 
 * the address and size of the current mmapped block, the start offset
 * of the block and the socket descriptor to communicate with the storage
 * process.
 * 
 */
typedef struct { 
    int sd;			/* unix socket 
				 * XXX not that nice to have it here */ 
    char * name; 		/* file name, dynamically allocated */
    int mode;			/* file access mode */
    int fd; 			/* OS file descriptor */
    int id; 			/* STORAGE file ID */
    off_t off_file; 		/* start offset of current file */
    void * addr; 		/* address of memory mapped block */
    size_t size; 		/* size of memory mapped block */
    off_t offset; 		/* bytestream offset of the mapped block */
    off_t readofs; 		/* currently read offset (used by csreadp) */
    off_t readsz; 		/* currently read size (used by csreadp) */
} csfile_t;


/* file descriptors for open files */
static csfile_t * files[CS_MAXCLIENTS];


/*
 * -- waitforack
 *
 * Wait to receive an acknowledgemnt to a request 
 * sent to the server. 
 *
 */
static void
waitforack(int sd, csmsg_t * m) 
{
    fd_set rd;
    int ret = 0;

    FD_ZERO(&rd);
    FD_SET(sd, &rd);
    /* wait for the message */
    ret = select(sd + 1, &rd, NULL, NULL, NULL);
    if (ret < 0)
	panic("select error (%s)\n", strerror(errno));

    /* read the message */
    ret = read(sd, m, sizeof(csmsg_t));
    if (ret != sizeof(csmsg_t)) 
	panic("read error: got %d, expected %d", ret, sizeof(csmsg_t)); 

    return;
}

/* 
 * -- csopen
 * 
 * csopen() opens a file managed by the storage process. It 
 * uses the unix socket to communicate its intentions. The function 
 * returns an integer used as file descriptor, or -1 on error.
 * Obviously the descriptor is not select()-able.
 */ 
int
csopen(const char * name, int mode, off_t size, int sd) 
{ 
    csfile_t * cf;
    csmsg_t m;
    int fd; 

    assert(mode == CS_READER || mode == CS_WRITER || mode == CS_READER_NOBLOCK);

    /* look for an empty file descriptor */
    for (fd = 0; fd < CS_MAXCLIENTS && files[fd] != NULL; fd++)
	;

    if (fd == CS_MAXCLIENTS) {
	logmsg(LOGWARN, "Too many open files, cannot open %s\n", name); 
	return -1;
    } 

    /* 
     * file descriptor found, prepare and send the request 
     * message to the storage process
     */
    bzero(&m, sizeof(m));
    m.type = S_OPEN;
    m.id = 0;
    m.arg = mode;
    m.size = size;
    strncpy(m.name, name, FILENAME_MAX); 

    if (write(sd, (char *) &m, sizeof(m)) < 0) 
	panic("sending message to storage: %s\n", strerror(errno)); 

    /* wait for the acknowledgment */
    waitforack(sd, &m);

    if (m.type == S_ERROR) {
	logmsg(LOGWARN, "error opening file %s: %s\n", name, strerror(m.arg));
	return -1;
    }

    /* 
     * allocate a new file descriptor and initialize it with 
     * the information in the message. 
     */
    cf = safe_calloc(1, sizeof(csfile_t)); 
    cf->fd = -1; 
    cf->sd = sd;
    cf->name = strdup(name);
    cf->mode = mode;
    cf->id = m.id;
    cf->off_file = m.ofs;  

    /* store current offset. m.size is set to 0 by the server if this client
     * is a reader. otherwise it is equal to the size of the bytestream so 
     * that writes are append only. 
     */
    cf->offset = m.ofs + m.size; 

    files[fd] = cf;
    return fd; 
}
    

/* 
 * -- _csinform
 * 
 * send a message to storage to inform that the writer has moved
 * to another offset. note that writes are append-only so we can 
 * only move forward. this message does not need acknowledgement and
 * allows storage to wakeup clients waiting for new data. 
 *
 */
static void
_csinform(csfile_t * cf, off_t ofs) 
{ 
    csmsg_t out; 

    out.id = cf->id;
    out.arg = 0; 
    out.type = S_INFORM;
    out.ofs = ofs; 

    if (write(cf->sd, &out, sizeof(out)) < 0) 
	logmsg(LOGWARN, "message to storage: %s\n", strerror(errno)); 
}


/* 
 * -- _csmap
 * 
 * this function provides an interface similar to mmap. 
 * it maps in memory a region of the file. before doing so, informs 
 * the server of this intentions and makes sure that it can do it. 
 *  
 * the acknowledgement from the server carries two pieces of information: 
 *   . offset, that is set to the start offset of the file that 
 *     contains the block.
 *   . size, that indicates the size of the region that can be memory mapped. 
 *     (different only if the region overlaps multiple files). 
 * 
 * this function returns a pointer to the memory mapped region and the size
 * of the region. 
 *
 * _csmap is the back-end for csmap, csreadp, csseek. 
 * 
 */
static void * 
_csmap(int fd, off_t ofs, ssize_t * sz, int method) 
{
    csfile_t * cf;
    csmsg_t out, in;
    int flags;
    int diff;

    cf = files[fd];

    /* Package the request and send it to the server */
    out.id = cf->id;
    if (method == S_REGION) {
	out.arg = 0; 
	out.type = method;
	out.size = *sz;
	out.ofs = ofs; /* the map offset */
    } else {	/* all the seek variants */
	out.type = S_SEEK;
	out.arg = CS_SEEK_FILE_NEXT;
	out.size = 0; 
	out.ofs = 0; 
    }

    /* send the request out */
    if (write(cf->sd, &out, sizeof(out)) < 0) {
	logmsg(LOGWARN, "message to storage: %s\n", strerror(errno)); 
	*sz = -1;
	return NULL;
    }

    /* block waiting for the acknowledgment */
    waitforack(cf->sd, &in);

    switch (in.type) { 
    case S_ERROR: 
	logmsg(LOGWARN, "csmap error in %s: %s\n", cf->name, strerror(in.arg));
	errno = in.arg;
	*sz = -1;
	return NULL;

    case S_ACK: 
	/* 
	 * The server acknowledged our request,
	 * unmap the current block (both for csmap and csseek)
	 */
	if (cf->addr)
	    munmap(cf->addr, cf->size);

	if (method == S_REGION) {
	    /* 
	     * A zero sized block indicates end-of-bytestream. If so, close
	     * the current file and return an EOF as well.
	     */
	    if (in.size == 0) { 
		close(cf->fd);
		*sz = 0;
		return NULL;
	    }
	} else {	/* seek variants */
	    /* the current file is not valid anymore */
	    close(cf->fd);
	    cf->off_file = in.ofs;
	    cf->fd = -1;
	    *sz = (size_t)in.ofs;	/* return value */
	    return NULL;		/* we are done */
	}
	break;

    default: 
	panic("unknown msg type %d from storage\n", in.type);
	break;
    }

    if ((ssize_t)in.size == -1)
	panic("unexpected return from storage process");

    /*
     * now check if the requested block is in the same file (i.e., 
     * the server replied with an offset that is the first offset of 
     * the currently open file. if not, close the current file and 
     * open a new one. 
     */
    if (cf->fd < 0 || in.ofs != cf->off_file) {
	char * nm;

	if (cf->fd >= 0)
	    close(cf->fd);

#ifdef linux
	flags = (cf->mode != CS_WRITER)? O_RDWR : O_RDWR|O_APPEND; 
#else
	flags = (cf->mode != CS_WRITER)? O_RDONLY : O_WRONLY|O_APPEND; 
#endif
	asprintf(&nm, "%s/%016llx", cf->name, in.ofs); 
	cf->fd = open(nm, flags, 0666);
	if (cf->fd < 0)
	    panic("opening file %s acked! (%s)\n", nm, strerror(errno));
	cf->off_file = in.ofs;
	free(nm);
    } 
	   
    /*
     * mmap the new block 
     */
    flags = (cf->mode != CS_WRITER)? PROT_READ : PROT_WRITE; 
    cf->offset = ofs; 
    cf->size = *sz = in.size;

    /* 
     * align the mmap to the memory pagesize 
     */
    diff = (cf->offset - cf->off_file) % getpagesize();
    cf->offset -= diff; 
    cf->size += diff; 

    cf->addr = mmap(0, cf->size, flags, MAP_NOSYNC|MAP_SHARED, 
	cf->fd, cf->offset - cf->off_file);
    if (cf->addr == MAP_FAILED || cf->addr == NULL)
        panic("mmap got NULL (%s)\n", strerror(errno)); 

    assert(cf->addr + diff != NULL);

    return (cf->addr + diff);
}


/* 
 * -- csmap
 * 
 * this function request to mmap a new region in the bytestream. 
 * it first checks if the input parameters are correct. then it 
 * checks if the region requested is already mmapped. if so, it 
 * informs of the request the storage server only if this is a 
 * writer. 
 * 
 * it then uses _csmap to perform the action. 
 */
void * 
csmap(int fd, off_t ofs, ssize_t * sz) 
{
    csfile_t * cf;
    ssize_t newsz; 
    void * addr; 

    assert(fd >= 0 && fd < CS_MAXCLIENTS && files[fd] != NULL); 
    cf = files[fd];

    /* 
     * check if we already have mmapped the requested region. 
     */
    if (ofs > cf->offset && ofs + *sz < cf->offset + cf->size) { 
	/* 
	 * if in write mode, inform the storage process that we 
	 * are moving forward. 
	 */
	if (cf->mode == CS_WRITER) 
	    _csinform(cf, ofs); 

        logmsg(V_LOGSTORAGE, 
	    "ofs %lld sz %d silently approved (%lld:%d)\n", 
            ofs, *sz, cf->offset, cf->size); 

        return (cf->addr + (ofs - cf->offset));
    } 

    /* 
     * inflate the block size if too small. this will help answering
     * future requests. however do not tell anything to the caller. 
     * we change *sz only if the storage process cannot handle the 
     * requested size; 
     */
    newsz = (*sz < CS_OPTIMALSIZE)? CS_OPTIMALSIZE : *sz; 
    addr = _csmap(fd, ofs, &newsz, S_REGION);
    if (newsz < *sz) 
	*sz = newsz; 
    return addr; 
}

/* 
 * -- csseek
 * 
 * jump to next file in the bytestream. it uses _csmap to do it. 
 * 
 */
off_t
csseek(int fd)
{
    ssize_t retval;

    assert(fd >= 0 && fd < CS_MAXCLIENTS && files[fd] != NULL); 
    _csmap(fd, 0, &retval, S_SEEK);

    /* reset read values */
    files[fd]->readofs = files[fd]->offset; 
    files[fd]->readsz = 0;   

    return (off_t)retval;
}


/* 
 * -- csreadp
 *
 * this is a simpler interface to csmap that does not require to 
 * know the offset. it returns a pointer to the mmapped region and 
 * its size
 */
size_t
csreadp(int fd, void ** buf, size_t sz)
{
    csfile_t * cf; 
    ssize_t realsz = sz; 

    assert(fd >= 0 && fd < CS_MAXCLIENTS && files[fd] != NULL); 

    *buf = csmap(fd, cf->readofs + cf->readsz, &realsz); 

    /* update the read offset and size */
    cf->readofs += cf->readsz; 
    cf->readsz = realsz; 
 
    return cf->readsz; 
}


/*
 * -- csclose
 *
 * Closes the file and sends the message to the server. 
 */
void
csclose(int fd)
{
    csmsg_t m;

    assert(fd >= 0 && fd < CS_MAXCLIENTS && files[fd] != NULL); 
    memset(&m, 0, sizeof(m));

    /* unmap the current block and close the file, if any */
    if (files[fd]->addr != NULL)
	munmap(files[fd]->addr, files[fd]->size);
    if (files[fd]->fd >= 0)
	close(files[fd]->fd); 

    /* send the release message to the hfd */
    m.type = S_CLOSE;
    m.id = files[fd]->id; 
    if (write(files[fd]->sd, (char *) &m, sizeof(m)) < 0) 
	panic("sending message to storage: %s\n", strerror(errno)); 

    free(files[fd]);
    files[fd] = NULL;
    return;
}


/* 
 * -- csgetofs
 * 
 * returns the current offset of the file. 
 */
off_t
csgetofs(int fd)
{
    assert(fd >= 0 && fd < CS_MAXCLIENTS && files[fd] != NULL); 
    return files[fd]->offset;
} 
/* end of file */
