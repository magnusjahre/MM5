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
 * Definition of a Intel ITX memory trace format reader.
 */

#ifndef __ITX_READER_HH__
#define __ITX_READER_HH__

#include <stdio.h>
#include <string>

#include "cpu/trace/reader/mem_trace_reader.hh"
#include "mem/mem_req.hh"


/**
 * A memory trace reader for the Intel ITX memory trace format.
 */
class ITXReader : public MemTraceReader
{
    /** Trace file. */
    FILE *trace;

    bool codeVirtValid;
    Addr codeVirtAddr;
    bool codePhysValid;
    Addr codePhysAddr;

    int traceFormat;

    enum ITXType {
	ITXRead,
	ITXWrite,
	ITXWriteback,
	ITXCode,
	ITXCodeComp
    };
    
  public:
    /**
     * Construct an ITXReader.
     */
    ITXReader(const std::string &name, const std::string &filename);
    
    /**
     * Read the next request from the trace. Returns the request in the
     * provided MemReqPtr and the cycle of the request in the return value.
     * @param req Return the next request from the trace.
     * @return ITX traces don't store timing information, return 0
     */
    virtual Tick getNextReq(MemReqPtr &req);
};

#endif //__ITX_READER_HH__

