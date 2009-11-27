/*
 * Copyright (c) 2003, 2004, 2005
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

/** @file
 * Declaration of a structure to manage MSHRs.
 */

#ifndef __MSHR_QUEUE_HH__
#define __MSHR_QUEUE_HH__

#include <vector>
#include "mem/cache/miss/mshr.hh"
#include "mem/cache/base_cache.hh" // for CACHE_DEBUG

#include "base/statistics.hh"
#include "mem/requesttrace.hh"

class MLPEstimationEvent;

/**
 * A Class for maintaining a list of pending and allocated memory requests.
 */
class MSHRQueue {
  private:
    /**  MSHR storage. */
    MSHR* registers;
    /** Holds pointers to all allocated MSHRs. */
    MSHR::List allocatedList;
    /** Holds pointers to MSHRs that haven't been sent to the bus. */
    MSHR::List pendingList;
    /** Holds non allocated MSHRs. */
    MSHR::List freeList;

    RequestTrace missCountTrace;

    // Parameters
    /**
     * The total number of MSHRs in this queue. This number is set as the
     * number of MSHRs requested plus (numReserve - 1). This allows for
     * the same number of effective MSHRs while still maintaining the reserve.
     */
    int numMSHRs;

    /**
     * The number of MSHRs to hold in reserve. This is needed because copy
     * operations can allocate upto 4 MSHRs at one time.
     */
    const int numReserve;

    int maxMSHRs;

    BaseCache* cache;

    MSHR* maxMSHRAddr;
    MSHR* minMSHRAddr;

    /** The number of allocated MSHRs. */
	int allocated;

	int countdownCounter;
	int ROBSize;

	void missArrived(MemCmd cmd);

	bool isMissQueue;

	MLPEstimationEvent* mlpEstEvent;

	std::vector<double > currentMLPAccumulator;
	Tick mlpAccumulatorTicks;


	Tick outstandingMissAccumulator;
	Tick outstandingMissAccumulatorCount;

  protected:

	Stats::Scalar<> opacu_overlapped_misses;
	Stats::Scalar<> opacu_serial_misses;
	Stats::Formula opacu_serial_percentage;

	Stats::Scalar<> mshrcnt_overlapped_misses;
	Stats::Scalar<> mshrcnt_serial_misses;
	Stats::Formula mshrcnt_serial_percentage;

	Stats::Scalar<> roblookup_overlapped_misses;
	Stats::Scalar<> roblookup_serial_misses;
	Stats::Formula roblookup_serial_percentage;

	Stats::Scalar<> mlp_cost_accumulator;
	Stats::Scalar<> mlp_cost_total_misses;
	Stats::Formula avg_mlp_cost_per_miss;

	Stats::Distribution<> mlp_cost_distribution;
	Stats::Distribution<> latency_distribution;
	Stats::Distribution<> allocated_mshrs_distribution;

	Stats::Vector<> mlp_estimation_accumulator;
	Stats::Scalar<> mlp_active_cycles;
	Stats::Formula avg_mlp_estimation;

  public:
    /** The number of MSHRs that have been forwarded to the bus. */
    int inServiceMSHRs;
    /** The number of targets waiting for response. */
    int allocatedTargets;

    /**
     * Create a queue with a given number of MSHRs.
     * @param num_mshrs The number of MSHRs in this queue.
     * @param reserve The minimum number of MSHRs needed to satisfy any access.
     */
    MSHRQueue(int num_mshrs, bool _isMissQueue, int reserve = 1);

    /** Destructor */
    ~MSHRQueue();

    void regStats(const char* subname);

    /**
     * Find the first MSHR that matches the provide address and asid.
     * @param addr The address to find.
     * @param asid The address space id.
     * @return Pointer to the matching MSHR, null if not found.
     */
    MSHR* findMatch(Addr addr, int asid) const;

    // Magnus HACK for coherence behaviour
    MSHR* findMatch(Addr addr, MemCmd cmd) const;


    /**
     * Find and return all the matching MSHRs in the provided vector.
     * @param addr The address to find.
     * @param asid The address space ID.
     * @param matches The vector to return pointers to the matching MSHRs.
     * @return True if any matches are found, false otherwise.
     * @todo Typedef the vector??
     */
    bool findMatches(Addr addr, int asid, std::vector<MSHR*>& matches) const;

    /**
     * Find any pending requests that overlap the given request.
     * @param req The request to find.
     * @return A pointer to the earliest matching MSHR.
     */
    MSHR* findPending(MemReqPtr &req) const;

    /**
     * Allocates a new MSHR for the request and size. This places the request
     * as the first target in the MSHR.
     * @param req The request to handle.
     * @param size The number in bytes to fetch from memory.
     * @return The a pointer to the MSHR allocated.
     *
     * @pre There are free MSHRs.
     */
    MSHR* allocate(MemReqPtr &req, int size = 0);

    /**
     * Allocate a read request for the given address, and places the given
     * target on the target list.
     * @param addr The address to fetch.
     * @param asid The address space for the fetch.
     * @param size The number of bytes to request.
     * @param target The first target for the request.
     * @return Pointer to the new MSHR.
     */
    MSHR* allocateFetch(Addr addr, int asid, int size, MemReqPtr &target);

    /**
     * Allocate a target list for the given address.
     * @param addr The address to fetch.
     * @param asid The address space for the fetch.
     * @param size The number of bytes to request.
     * @return Pointer to the new MSHR.
     */
    MSHR* allocateTargetList(Addr addr, int asid, int size);

    /**
     * Removes the given MSHR from the queue. This places the MSHR on the
     * free list.
     * @param mshr
     */
    void deallocate(MSHR* mshr);

    /**
     * Allocates a target to the given MSHR. Used to keep track of the number
     * of outstanding targets.
     * @param mshr The MSHR to allocate the target to.
     * @param req The target request.
     */
    void allocateTarget(MSHR* mshr, MemReqPtr &req)
    {
	mshr->allocateTarget(req);
	allocatedTargets += 1;
    }

    /**
     * Remove a MSHR from the queue. Returns an iterator into the allocatedList
     * for faster squash implementation.
     * @param mshr The MSHR to remove.
     * @return An iterator to the next entry in the allocatedList.
     */
    MSHR::Iterator deallocateOne(MSHR* mshr);

    /**
     * Moves the MSHR to the front of the pending list if it is not in service.
     * @param mshr The mshr to move.
     */
    void moveToFront(MSHR *mshr);

    /**
     * Mark the given MSHR as in service. This removes the MSHR from the
     * pendingList. Deallocates the MSHR if it does not expect a response.
     * @param mshr The MSHR to mark in service.
     */
    void markInService(MSHR* mshr);

    /**
     * Mark an in service mshr as pending, used to resend a request.
     * @param mshr The MSHR to resend.
     * @param cmd The command to resend.
     */
    void markPending(MSHR* mshr, MemCmd cmd);

    /**
     * Squash outstanding requests with the given thread number. If a request
     * is in service, just squashes the targets.
     * @param thread_number The thread to squash.
     */
    void squash(int thread_number);

    /**
     * Returns true if the pending list is not empty.
     * @return True if there are outstanding requests.
     */
    bool havePending() const
    {
	return !pendingList.empty();
    }

    /**
     * Returns true if there are no free MSHRs.
     * @return True if this queue is full.
     */
    bool isFull() const
    {
	return (allocated > numMSHRs - numReserve);
    }

    /**
     * Returns the request at the head of the pendingList.
     * @return The next request to service.
     */
    MemReqPtr getReq() const;

    /**
     * Returns the number of outstanding targets.
     * @return the number of allocated targets.
     */
    int getAllocatedTargets() const
    {
	return allocatedTargets;
    }

    // Adaptive MSHR methods
    void incrementNumMSHRs(){
        // assumes MSHR count to be a power of two
        int currentMSHRCount = (numMSHRs - numReserve) + 1;
        int newCount = currentMSHRCount << 1;
        numMSHRs = newCount + numReserve -1;
    }

    void decrementNumMSHRs(){
        // assumes MSHR count to be a power of two
        int currentMSHRCount = (numMSHRs - numReserve) + 1;
        int newCount = currentMSHRCount >> 1;
        numMSHRs = newCount + numReserve -1;
    }

    void incrementNumMSHRsByOne(){
        numMSHRs++;
    }

    void decrementNumMSHRsByOne(){
        numMSHRs--;
    }

    void setNumMSHRs(int newMSHRSize);

    int getCurrentMSHRCount(){
        return (numMSHRs - numReserve) + 1;
    }

    std::map<int,int> assignBlockingBlame(int maxTargets, bool blockedMSHRs, double threshold);

    void printMSHRQueue(){
        std::cout << "Allocated list:\n";
        MSHR::ConstIterator i = allocatedList.begin();
        MSHR::ConstIterator end = allocatedList.end();
        for (; i != end; ++i) {
            MSHR *mshr = *i;
            mshr->dump();
        }

        std::cout << "Pending list:\n";
        i = pendingList.begin();
        end = pendingList.end();
        for (; i != end; ++i) {
            MSHR *mshr = *i;
            mshr->dump();
        }

        std::cout << "Free list:\n";
        i = freeList.begin();
        end = freeList.end();
        for (; i != end; ++i) {
            MSHR *mshr = *i;
            mshr->dump();
        }
        std::cout << "\n";

    }

    void printShortStatus(){
        std::cout << "Allocated: " << allocatedList.size() << ", Pending: " << pendingList.size() << ", Free: " << freeList.size() << "\n";
    }

    void setCache(BaseCache* _cache);

    void cpuCommittedInstruction();

    void handleMLPEstimationEvent();

    bool isDemandRequest(MemCmd cmd);

    std::vector<double> getMLPEstimate();

};

class MLPEstimationEvent : public Event{

private:
	MSHRQueue* mshrQueue;

public:
	MLPEstimationEvent(MSHRQueue* _mshrQueue)
		: Event(&mainEventQueue, Event::Stat_Event_Pri), mshrQueue(_mshrQueue) {

	}

	void process() {
		mshrQueue->handleMLPEstimationEvent();
		this->schedule(curTick + 1);
	}

	virtual const char *description() {
		return "MLP Estimation Event";
	}

};

#endif //__MSHR_QUEUE_HH__
