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
 * Declaration of a M5 memory trace writer.
 */

#include "mem/trace/m5_writer.hh"
#include "mem/trace/m5_format.hh"
#include "sim/builder.hh"

using namespace std;

M5Writer::M5Writer(const std::string &name, const std::string &filename)
    : MemTraceWriter(name)
{
    traceFile.open(filename.c_str(), ios::binary);
}

M5Writer::~M5Writer()
{
    traceFile.flush();
    traceFile.close();
}

void
M5Writer::writeReq(const MemReqPtr &req)
{
    M5Format ref;
    ref.cycle = curTick;
    ref.paddr = req->paddr;
    ref.asid = req->asid;
    ref.cmd = req->cmd.toIndex();
    ref.size = req->size;
    if (req->cmd == Copy) {
	ref.dest = req->dest;
    } else {
	ref.dest = 0;
    }
    traceFile.write((char*) &ref, sizeof(ref));
    /** @todo Remove this onve we get destructors working. */
    traceFile.flush();
#ifdef DEBUG
    if (req->thread_num != req->asid) {
	warn("Request thread number and asid don't match!");
    }
#endif
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(M5Writer)
  
    Param<string> filename;

END_DECLARE_SIM_OBJECT_PARAMS(M5Writer)


BEGIN_INIT_SIM_OBJECT_PARAMS(M5Writer)

    INIT_PARAM(filename, "trace file")
    
END_INIT_SIM_OBJECT_PARAMS(M5Writer)


CREATE_SIM_OBJECT(M5Writer)
{
    return new M5Writer(getInstanceName(), filename);
}

REGISTER_SIM_OBJECT("M5Writer", M5Writer)
