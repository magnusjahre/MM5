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
 * @file
 * Miss Status and Handling Register (MSHR) declaration.
 */

#ifndef __MSHR_HH__
#define __MSHR_HH__

#include "mem/mem_req.hh"
#include "mem/cache/base_cache.hh"
#include <list>
#include <deque>

class MSHR;

/**
 * Miss Status and handling Register. This class keeps all the information
 * needed to handle a cache miss including a list of target requests.
 */
class MSHR {
public:
    /** Defines the Data structure of the MSHR targetlist. */
    typedef std::deque<MemReqPtr> TargetList;
    /** A list of MSHRs. */
    typedef std::list<MSHR *> List;
    /** MSHR list iterator. */
    typedef List::iterator Iterator;
    /** MSHR list const_iterator. */
    typedef List::const_iterator ConstIterator;

    /** Address of the miss. */
    Addr addr;
    /** Adress space id of the miss. */
    short asid;
    /** True if the request has been sent to the bus. */
    bool inService;
    /** Thread number of the miss. */
    int threadNum;
    /** The request that is forwarded to the next level of the hierarchy. */
    MemReqPtr req;
    /** The number of currently allocated targets. */
    short ntargets;
    /** The original requesting command. */
    MemCmd originalCmd;
    /** Order number of assigned by the miss queue. */
    uint64_t order;

    MemCmd directoryOriginalCmd;

    /**
     * Pointer to this MSHR on the ready list.
     * @sa MissQueue, MSHRQueue::readyList
     */
    Iterator readyIter;
    /**
     * Pointer to this MSHR on the allocated list.
     * @sa MissQueue, MSHRQueue::allocatedList
     */
    Iterator allocIter;

    double mlpCost;
    std::vector<double> mlpCostDistribution;

private:
    /** List of all requests that match the address */
    TargetList targets;

    BaseCache* cache;

public:
    /**
     * Allocate a miss to this MSHR.
     * @param cmd The requesting command.
     * @param addr The address of the miss.
     * @param asid The address space id of the miss.
     * @param size The number of bytes to request.
     * @param req  The original miss.
     */
    void allocate(MemCmd cmd, Addr addr, int asid, int size,
		  MemReqPtr &req);

    /**
     * Allocate this MSHR as a buffer for the given request.
     * @param target The memory request to buffer.
     */
    void allocateAsBuffer(MemReqPtr &target);

    /**
     * Mark this MSHR as free.
     */
    void deallocate();

    /**
     * Add a request to the list of targets.
     * @param target The target.
     */
    void allocateTarget(MemReqPtr &target);

    /** A simple constructor. */
    MSHR();
    /** A simple destructor. */
    ~MSHR();

    /**
     * Returns the current number of allocated targets.
     * @return The current number of allocated targets.
     */
    int getNumTargets()
    {
        return(ntargets);
    }

    /**
     * Returns a pointer to the target list.
     * @return a pointer to the target list.
     */
    TargetList* getTargetList()
    {
	return &targets;
    }

    /**
     * Returns a reference to the first target.
     * @return A pointer to the first target.
     */
    MemReqPtr getTarget()
    {
	return targets.front();
    }

    /**
     * Pop first target.
     */
    void popTarget()
    {
        --ntargets;
	targets.pop_front();
    }

    /**
     * Returns true if there are targets left.
     * @return true if there are targets
     */
    bool hasTargets()
    {
	return !targets.empty();
    }

    /**
     * Prints the contents of this MSHR to stderr.
     */
    void dump();

    void setCache(BaseCache* _cache);
};

#endif //__MSHR_HH__
