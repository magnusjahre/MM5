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

/**
 * @file
 * Declaration of a simple buffer for a blocking cache.
 */

#ifndef __BLOCKING_BUFFER_HH__
#define __BLOCKING_BUFFER_HH__

#include <vector>

#include "mem/cache/miss/mshr.hh"
#include "base/statistics.hh"

class BaseCache;
class BasePrefetcher;

/**
 * Miss and writeback storage for a blocking cache.
 */
class BlockingBuffer
{
protected:
    /** Miss storage. */
    MSHR miss;
    /** WB storage. */
    MSHR wb;

    //Params

    /** Allocate on write misses. */
    const bool writeAllocate;

    /** Pointer to the parent cache. */
    BaseCache* cache;

    BasePrefetcher* prefetcher;

    /** Block size of the parent cache. */
    int blkSize;

    // Statistics
    /**
     * @addtogroup CacheStatistics
     * @{
     */
    /** Number of blocks written back per thread. */
    Stats::Vector<> writebacks;

    /**
     * @}
     */

public:
    /**
     * Builds and initializes this buffer.
     * @param write_allocate If true, treat write misses the same as reads.
     */
    BlockingBuffer(bool write_allocate)
	: writeAllocate(write_allocate)
    {
    }

    /**
     * Register statistics for this object.
     * @param name The name of the parent cache.
     */
    void regStats(const std::string &name);

    /**
     * Called by the parent cache to set the back pointer.
     * @param _cache A pointer to the parent cache.
     */
    void setCache(BaseCache *_cache);

    void setPrefetcher(BasePrefetcher *_prefetcher);

    /**
     * Handle a cache miss properly. Requests the bus and marks the cache as
     * blocked.
     * @param req The request that missed in the cache.
     * @param blk_size The block size of the cache.
     * @param time The time the miss is detected.
     */
    void handleMiss(MemReqPtr &req, int blk_size, Tick time);

    /**
     * Fetch the block for the given address and buffer the given target.
     * @param addr The address to fetch.
     * @param asid The address space of the address.
     * @param blk_size The block size of the cache.
     * @param time The time the miss is detected.
     * @param target The target for the fetch.
     */
    MSHR* fetchBlock(Addr addr, int asid, int blk_size, Tick time,
		     MemReqPtr &target)
    {
	fatal("Unimplemented");
    }

    /**
     * Selects a outstanding request to service.
     * @return The request to service, NULL if none found.
     */
    MemReqPtr getMemReq();

    /**
     * Set the command to the given bus command.
     * @param req The request to update.
     * @param cmd The bus command to use.
     */
    void setBusCmd(MemReqPtr &req, MemCmd cmd);

    /**
     * Restore the original command in case of a bus transmission error.
     * @param req The request to reset.
     */
    void restoreOrigCmd(MemReqPtr &req);

    /**
     * Marks a request as in service (sent on the bus). This can have side
     * effect since storage for no response commands is deallocated once they
     * are successfully sent.
     * @param req The request that was sent on the bus.
     */
    void markInService(MemReqPtr &req);

    /**
     * Frees the resources of the request and unblock the cache.
     * @param req The request that has been satisfied.
     * @param time The time when the request is satisfied.
     */
    void handleResponse(MemReqPtr &req, Tick time);

    /**
     * Removes all outstanding requests for a given thread number. If a request
     * has been sent to the bus, this function removes all of its targets.
     * @param thread_number The thread number of the requests to squash.
     */
    void squash(int thread_number);

    /**
     * Return the current number of outstanding misses.
     * @return the number of outstanding misses.
     */
    int getMisses()
    {
	return miss.getNumTargets();
    }

    /**
     * Searches for the supplied address in the miss "queue".
     * @param addr The address to look for.
     * @param asid The address space id.
     * @return A pointer to miss if it matches.
     */
    MSHR* findMSHR(Addr addr, int asid)
    {
	if (miss.addr == addr && miss.req)
	    return &miss;
	return NULL;
    }

    /**
     * Searches for the supplied address in the write buffer.
     * @param addr The address to look for.
     * @param asid The address space id.
     * @param writes List of pointers to the matching writes.
     * @return True if there is a matching write.
     */
    bool findWrites(Addr addr, int asid, std::vector<MSHR*>& writes)
    {
	if (wb.addr == addr && wb.req) {
	    writes.push_back(&wb);
	    return true;
	}
	return false;
    }



    /**
     * Perform a writeback of dirty data to the given address.
     * @param addr The address to write to.
     * @param asid The address space id.
     * @param xc The execution context of the address space.
     * @param size The number of bytes to write.
     * @param data The data to write, can be NULL.
     * @param compressed True if the data is compressed.
     */
    void doWriteback(Addr addr, int asid, ExecContext *xc,
		     int size, uint8_t *data, bool compressed);

    /**
     * Perform a writeback request.
     * @param req The writeback request.
     */
    void doWriteback(MemReqPtr &req);

    /**
     * Returns true if there are outstanding requests.
     * @return True if there are outstanding requests.
     */
    bool havePending()
    {
	return !miss.inService || !wb.inService;
    }

    /**
     * Add a target to the given MSHR. This assumes it is in the miss queue.
     * @param mshr The mshr to add a target to.
     * @param req The target to add.
     */
    void addTarget(MSHR *mshr, MemReqPtr &req)
    {
	fatal("Shouldn't call this on a blocking buffer.");
    }

    /**
     * Dummy implmentation.
     */
    MSHR* allocateTargetList(Addr addr, int asid)
    {
	fatal("Unimplemented");
    }

    void incrementNumMSHRs(bool onMSHRs){
        fatal("Makes no sense");
    }

    void decrementNumMSHRs(bool onMSHRs){
        fatal("Makes no sense");
    }

    void incrementNumMSHRsByOne(bool onMSHRs){
        fatal("Makes no sense");
    }

    void decrementNumMSHRsByOne(bool onMSHRs){
        fatal("Makes no sense");
    }

    int getCurrentMSHRCount(bool onMSHRs){
        fatal("Makes no sense");
        return 0;
    }

    std::map<int,int> assignBlockingBlame(bool blockedForMiss, bool blockedForTargets, double threshold){
        fatal("Makes no sense");
        return map<int,int>();
    }

    void coreCommittedInstruction(){
    	fatal("Makes no sense");
    }

    std::vector<double> getMLPEstimate(){
    	fatal("Makes no sense");
    }

    int getNumMSHRs(){
    	return 1;
    }
};

#endif // __BLOCKING_BUFFER_HH__
