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
 * Definition of an ITX memory trace writer.
 */
#include <stdio.h>
#include <string>
#include "mem/trace/mem_trace_writer.hh"

#ifndef __ITX_WRITER_HH__
#define __ITX_WRITER_HH__

/**
 * ITX memory trace writer.
 */
class ITXWriter : public MemTraceWriter
{
    /** Trace file. */
    FILE *trace;
    
    bool codeVirtValid;
    Addr codeVirtAddr;
    bool codePhysValid;
    Addr codePhysAddr;

    int traceFormat;

    bool isPipe;

    enum ITXType {
	ITXRead,
	ITXWrite,
	ITXWriteback,
	ITXCode,
	ITXCodeComp
    };

  public:
    /** Construct a ITX memory trace writer. */
    ITXWriter(const std::string &name, const std::string &filename);

    /** Flush and close the trace file. */
    virtual ~ITXWriter();
    
    /**
     * Write a memory request to the trace.
     * @param req The memory request to write.
     */
    virtual void writeReq(const MemReqPtr &req);
};

#endif // __ITX_WRITER_HH__
