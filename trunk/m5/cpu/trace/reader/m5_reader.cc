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
 * Declaration of a memory trace reader for a M5 memory trace.
 */

#include "cpu/trace/reader/m5_reader.hh"
#include "mem/trace/m5_format.hh"
#include "mem/mem_cmd.hh"
#include "sim/builder.hh"

using namespace std;

M5Reader::M5Reader(const string &name, const string &filename)
    : MemTraceReader(name)
{
    traceFile.open(filename.c_str(), ios::binary);
}

Tick
M5Reader::getNextReq(MemReqPtr &req)
{
    M5Format ref;
    
    MemReqPtr tmp_req;
    // Need to read EOF char before eof() will return true.
    traceFile.read((char*) &ref, sizeof(ref));
    if (!traceFile.eof()) {
	//traceFile.read((char*) &ref, sizeof(ref));
#ifndef NDEBUG
	int gcount = traceFile.gcount();
	assert(gcount != 0 || traceFile.eof());
	assert(gcount == sizeof(ref));
	assert(ref.cmd < 12);
#endif
	tmp_req = new MemReq();
	tmp_req->paddr = ref.paddr;
	tmp_req->asid = ref.asid;
	// Assume asid == thread_num
	tmp_req->thread_num = ref.asid;
	tmp_req->cmd = (MemCmdEnum)ref.cmd;
	tmp_req->size = ref.size;
	tmp_req->dest = ref.dest;
    } else {
	ref.cycle = 0;
    }
    req = tmp_req;
    return ref.cycle;
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(M5Reader)
  
    Param<string> filename;

END_DECLARE_SIM_OBJECT_PARAMS(M5Reader)


BEGIN_INIT_SIM_OBJECT_PARAMS(M5Reader)

    INIT_PARAM(filename, "trace file")
    
END_INIT_SIM_OBJECT_PARAMS(M5Reader)


CREATE_SIM_OBJECT(M5Reader)
{
    return new M5Reader(getInstanceName(), filename);
}

REGISTER_SIM_OBJECT("M5Reader", M5Reader)
