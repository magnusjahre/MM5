/*
 * Copyright (c) 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/**
 * @file
 * Declaration of a ITX memory trace writer.
 */
#include <sstream>
#include "mem/trace/itx_writer.hh"
#include "sim/builder.hh"

using namespace std;

ITXWriter::ITXWriter(const string &name, const string &filename)
    : MemTraceWriter(name)
{
    if (strcmp((filename.c_str() + filename.length() -3), ".gz") == 0) {
	// Compressed file, need to use a pipe to gzip.
	stringstream buf;
	buf << "gzip -f > " << filename << endl;
	trace = popen(buf.str().c_str(), "w");
	isPipe = true;
    } else {
	trace = fopen(filename.c_str(), "wb");
	isPipe = false;
    }
    if (!trace) {
	fatal("Can't open file %s", filename);
    }
    traceFormat = 0;
    codeVirtValid = false;
    codePhysValid = false;
    int c;
    for (int i = 0; i < 4; ++i) {
	c = putc((traceFormat >> 8 * i) & 0xff, trace);
	if (c == EOF) {
	    fatal("Unexpected end of trace file. ");
	}
    }
}

ITXWriter::~ITXWriter()
{
    //flush and close correctly
    if (isPipe) {
	pclose(trace);
    } else {
	fclose(trace);
    }
}

void
ITXWriter::writeReq(const MemReqPtr &req)
{
    // write req
    ITXType ref_type;
    if (req->cmd.isRead()) {
	if (req->isInstRead()) {
	    ref_type = ITXCode;
	} else {
	    ref_type = ITXRead;
	}
    } else if (req->cmd == Write) {
	ref_type = ITXWrite;
    } else if (req->cmd == Writeback) {
	ref_type = ITXWriteback;
    } else if (req->cmd == Squash) {
	return;
    } else {
	fatal("Unsupported memory command.");
    }

    // Assume the physical address is valid.
    int c = 0x80;
    c |= (ref_type << 4) & 0x70;
    c |= (req->size - 1) & 0x0f;
    putc(c, trace);
    
    // output vaddr
    //(down convert to 32 bits)
    uint32_t vaddr;
    if ((req->paddr & 0x0fff) != (req->vaddr & 0x0fff)) {
	vaddr = (uint32_t) req->paddr;
    } else {
	vaddr = (uint32_t) req->vaddr;
    }
    for (int i = 0; i < 4; ++i) {
	putc((vaddr >> 8 * i) & 0xff, trace);
    }
    
    // output paddr
    Addr paddr = req->paddr;
    if (((req->paddr >> 36) & 0xffffffff)) {
	warn("lost bits on physical address");
    }
    c = ((paddr >> 8) & 0xf0) | ((paddr >> 32) & 0x0f);
    putc(c, trace);
    for (int i = 2; i < 4; ++i) {
	putc((paddr >> 8*i) & 0xff, trace);
    }
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ITXWriter)
  
    Param<string> filename;

END_DECLARE_SIM_OBJECT_PARAMS(ITXWriter)


BEGIN_INIT_SIM_OBJECT_PARAMS(ITXWriter)

    INIT_PARAM(filename, "trace file")
    
END_INIT_SIM_OBJECT_PARAMS(ITXWriter)


CREATE_SIM_OBJECT(ITXWriter)
{
    return new ITXWriter(getInstanceName(), filename);
}

REGISTER_SIM_OBJECT("ITXWriter", ITXWriter)
