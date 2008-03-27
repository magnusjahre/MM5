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

    /* DDR2 states */

enum DDR2State {
        DDR2Idle,   
        DDR2Active,
        DDR2Written,
        DDR2Read
};

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

    /** The number of reads */
    Stats::Scalar<> number_of_reads;
    /** The number of writes */
    Stats::Scalar<> number_of_writes;
    /** The number of reads that hit open page */
    Stats::Scalar<> number_of_reads_hit;
    /** The number of writes that hit open page */
    Stats::Scalar<> number_of_writes_hit;

    /** Read hit rate */
    Stats::Formula read_hit_rate;
    /** Write hit rate */
    Stats::Formula write_hit_rate;
    
    /** Total latency */
    Stats::Scalar<> total_latency;
    /** Average latency */
    Stats::Formula average_latency;

    /* Slow read hits */
    Stats::Scalar<> number_of_slow_read_hits;
    /* Slow write hits */
    Stats::Scalar<> number_of_slow_write_hits;

    /* Non-overlapping activates */
    Stats::Scalar<> number_of_non_overlap_activate;

    /* DDR2 params */
    int num_banks;
    int RAS_latency;
    int CAS_latency;
    int precharge_latency;
    int min_activate_to_precharge_latency;

    int active_bank_count;

    /* DDR2 constants */
    int write_recovery_time;
    int internal_write_to_read;
    int internal_row_to_row;

    int pagesize;
    int internal_read_to_precharge;
    int data_time;
    int read_to_write_turnaround;
    int bus_to_cpu_factor;

    std::vector<DDR2State> Bankstate;
    std::vector<Tick> activateTime;
    std::vector<Tick> readyTime;
    std::vector<Tick> lastCmdFinish;
    std::vector<Tick> closeTime;
    std::vector<Addr> openpage;

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

    Tick calculateLatency(MemReqPtr &req);


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

    bool isActive(MemReqPtr &req);
    bool bankIsClosed(MemReqPtr &req);
    bool isReady(MemReqPtr &req);
    
    Tick getDataTransTime(){
        return data_time; //converted to CPU cycles in the constructor
    }
    
};

#endif
