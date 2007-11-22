/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
 * Declaration of a simple main memory.
 */

#ifndef __SIMPLE_MEM_BANK_HH__
#define __SIMPLE_MEM_BANK_HH__

#include "mem/timing/base_memory.hh"
#include "base/statistics.hh"

/**
 * A simple main memory that can handle compression.
 */
template <class Compression>
class SimpleMemBank : public BaseMemory
{
  protected:
    /** The compression algorithm. */
    Compression compress;

  public:

    // statistics
    /**
     * @addtogroup MemoryStatistics Memory Statistics
     * @{
     */
    /** The number of bytes requested per thread. */
    Stats::Vector<> bytesRequested;
    /** The number of bytes sent per thread. */
    Stats::Vector<> bytesSent;
    /** The number of compressed accesses per thread. */
    Stats::Vector<> compressedAccesses;

    /**
     * Constructs and initializes this memory.
     * @param name The name of this memory.
     * @param hier Pointer to the hierarchy wide parameters.
     * @param params The BaseMemory parameters.
     */
    SimpleMemBank(const std::string &name, HierParams *hier, 
		  BaseMemory::Params params);

    /**
     * Register statistics
     */
    virtual void regStats();

    /**
     * Perform the request on this memory.
     * @param req The request to perform.
     * @return MA_HIT
     */
    MemAccessResult access(MemReqPtr &req);    
    
    /**
     * Probe the memory for the request.
     * @param req The request to probe.
     * @param update True if memory should be updated.
     * @return The estimated completion time.
     *
     * @todo Change this when we need data.
     */
    Tick probe(MemReqPtr &req, bool update);
    
};

#endif
