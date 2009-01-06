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
 * Definitions of Cache TagStore template policy.
 */

#include "mem/cache/base_cache.hh"
#include "mem/cache/cache.hh"
#include "mem/cache/miss/mshr.hh"
#include "mem/cache/tags/cache_tags.hh"
#include "mem/cache/prefetch/base_prefetcher.hh"


#include "base/trace.hh" // for DPRINTF

using namespace std;

template <class Tags, class Compression>
CacheTags<Tags,Compression>::CacheTags(Tags *_ct, int comp_latency,
				       bool do_fast_writes,
				       bool store_compressed,
				       bool adaptive_compression,
				       bool prefetch_miss)
    : ct(_ct), compLatency(comp_latency), doFastWrites(do_fast_writes),
      storeCompressed(store_compressed),
      adaptiveCompression(adaptive_compression),
      prefetchMiss(prefetch_miss),
      blkSize(ct->getBlockSize())
{
    cache = NULL;
}


template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::regStats(const string &name)
{
    using namespace Stats;
    ct->regStats(name);
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::setCache(BaseCache *_cache, int bus_width,
				      int bus_ratio)
{
    cache = _cache;
    busWidth = bus_width;
    busRatio = bus_ratio;
    ct->setCache(cache, true);
    objName = cache->name();
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::setPrefetcher(BasePrefetcher *_prefetcher)
{
    prefetcher = _prefetcher;
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::handleAccess(MemReqPtr &req, int & lat,
					  MemReqList & writebacks, bool update)
{
    MemCmd cmd = req->cmd;

    // Set the block offset here
    req->offset = ct->extractBlkOffset(req->paddr);
    
    BlkType *blk = NULL;
    if (update) {
	blk = ct->findBlock(req, lat);
    } else {
	blk = ct->findBlock(req->paddr, req->asid);
	lat = 0;
    }
    if (blk != NULL) {
	
	// Hit
	if (blk->isPrefetch()) {

	    //Signal that this was a hit under prefetch (no need for use prefetch (only can get here if true)
	    DPRINTF(HWPrefetch, "%s:Hit a block that was prefetched\n", cache->name());
          cache->goodprefetches++;
	    blk->status &= ~BlkHWPrefetched;
	    if (prefetchMiss) {
		//If we are using the miss stream, signal the prefetcher
		//otherwise the access stream would have already signaled this hit
		prefetcher->handleMiss(req, curTick);
	    }
	}

	if ((cmd.isWrite() && blk->isWritable()) ||
	    (cmd.isRead() && blk->isValid())) {
	    
	    // We are satisfying the request
	    req->flags |= SATISFIED;

	    if (cmd.isWrite()){
		blk->status |= BlkDirty;
		ct->fixCopy(req, writebacks);
	    }

	    if (blk->isCompressed()) {
		// If the data is compressed, need to increase the latency
		lat += (compLatency/4);
	    }

	    if (cache->doData()) {
		bool write_data = false;

		assert(verifyData(blk));

		if (cmd.isWrite()){
		    write_data = true;
		    assert(req->offset < blkSize);
		    assert(req->size <= blkSize);
		    assert(req->offset+req->size <= blkSize);
		    memcpy(blk->data + req->offset, req->data,
			   req->size);
		} else {
		    assert(req->offset < blkSize);
		    assert(req->size <= blkSize);
		    assert(req->offset + req->size <=blkSize);
		    memcpy(req->data, blk->data + req->offset,
			   req->size);
		}

		if (write_data || 
		    (adaptiveCompression && blk->isCompressed()))
		    {
			// If we wrote data, need to update the internal block
			// data.
			updateData(blk, writebacks, 
				   !(adaptiveCompression && 
				     blk->isReferenced()));
		    }
	    }
	} else {
	    // permission violation, treat it as a miss
	    blk = NULL;
	}
    } 
    return blk;
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::handleFill(BlkType *blk, MemReqPtr &req,
					CacheBlk::State new_state,
					MemReqList & writebacks,
					MemReqPtr target)
{
    
#ifndef NDEBUG
    BlkType *tmp_blk = ct->findBlock(req->paddr, req->asid);
    assert(tmp_blk == blk);
#endif
    DPRINTF(Cache, "handleFill\n");
    blk = doReplacement(blk, req, new_state, writebacks);
    
    if (cache->doData()) {
	if (req->cmd.isRead()) {
	    memcpy(blk->data, req->data, blkSize);
	}
    }

    int bus_transactions = (req->size > busWidth) ?
	(req->size - busWidth) / busWidth :
	0;

    blk->whenReady = curTick + (busRatio * bus_transactions) +
	(req->isCompressed() ?
	 compLatency/4 :
	 0);
    
    // Respond to target, if any
    if (target) {
        
	MemCmd cmd = target->cmd;

	target->flags |= SATISFIED;
	    		
	if (cmd == Invalidate) {
	    invalidateBlk(blk);
	    blk = NULL;
	}

	if (cmd == Copy) {
	    writebacks.push_back(target);
	} else {    
	    if (blk && (cmd.isWrite() ? blk->isWritable() : blk->isValid())) {
		assert(cmd.isWrite() || cmd.isRead());
		if (cmd.isWrite()) {
		    blk->status |= BlkDirty;
		    ct->fixCopy(req, writebacks);
		    if (cache->doData()) {
			assert(target->offset + target->size <= blkSize);
			memcpy(blk->data + target->offset,
			       target->data, target->size);
		    }
		} else {
		    if (cache->doData()) {
			assert(target->offset + target->size <= blkSize);
			memcpy(target->data, blk->data + target->offset,
			       target->size);
		    }
		}
	    }
	}
    }

    if (blk && cache->doData()) {
	// Need to write the data into the block
	updateData(blk, writebacks, !adaptiveCompression || true);
    }
    return blk;
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::handleFill(BlkType *blk,
                                        MSHR * mshr,
					CacheBlk::State new_state,
					MemReqList & writebacks,
                                        MemReqPtr fillRequest)
{
#ifndef NDEBUG
    BlkType *tmp_blk = ct->findBlock(mshr->req->paddr, mshr->req->asid);
    assert(tmp_blk == blk);
#endif
    MemReqPtr req = mshr->req;
    blk = doReplacement(blk, req, new_state, writebacks);
    
    if (cache->doData()) {
	if (req->cmd.isRead()) {
	    memcpy(blk->data, req->data, blkSize);
	}
    }
    
    int bus_transactions = (req->size > busWidth) ?
	(req->size - busWidth) / busWidth :
	0;

    blk->whenReady = curTick + (busRatio * bus_transactions) +
	(req->isCompressed() ?
	 compLatency/4 :
	 0);

    // respond to MSHR targets, if any
    Tick response_base = ((!req->isCompressed()) ? curTick : blk->whenReady) +
	ct->getHitLatency();

    // First offset for critical word first calculations
    int initial_offset = 0;
    
    if (mshr->hasTargets()) {
	initial_offset = mshr->getTarget()->offset;
    }
    
    bool firstIsRead = (req->cmd == DirRedirectRead);

    while (mshr->hasTargets()) {
        
	MemReqPtr target = mshr->getTarget();
	MemCmd cmd = target->cmd;
        
	target->flags |= SATISFIED;
	
	// How many bytes pass the first request is this one
	int transfer_offset = target->offset - initial_offset;
	if (transfer_offset < 0) {
	    transfer_offset += blkSize;
	}
	
	// Covert byte offset to cycle offset
	int tgt_latency = (!target->isCompressed()) ?
	    (transfer_offset/busWidth) * busRatio :
	    0;
	
	Tick completion_time = response_base + tgt_latency;
	
        if(cache->isDirectoryAndL1DataCache()){
            if(firstIsRead && cmd.isWrite() && req->owner != cache->getCacheCPUid()){
                // since the first is read and we are not the owner, we need to retransmit
                // re-request is done in missQueue handleMiss()
                break;
            }
        }
        
	if (cmd == Invalidate) {
	    //Mark the blk as invalid now, if it hasn't been already
	    if (blk) {
		invalidateBlk(blk);
		blk = NULL;
	    }

	    //Also get rid of the invalidate
	    mshr->popTarget();

	    DPRINTF(Cache, "Popping off a Invalidate for blk_addr: %x\n",
		    req->paddr & (((ULL(1))<<48)-1));

	    continue;
	}
	
	if (cmd == Copy) {
	    writebacks.push_back(target);
	    break;
	}
	
	if (blk && (cmd.isWrite() ? blk->isWritable() : blk->isValid())) {
	    assert(cmd.isWrite() || cmd.isRead());
	    if (cmd.isWrite()) {
		blk->status |= BlkDirty;
		ct->fixCopy(req, writebacks);
		if (cache->doData()) {
		    assert(target->offset + target->size <= blkSize);
		    memcpy(blk->data + target->offset,
			   target->data, target->size);
		}
	    } else {
		if (cache->doData()) {
		    assert(target->offset + target->size <= blkSize);
		    memcpy(target->data, blk->data + target->offset,
			   target->size);
		}
	    }
	} else {
	    // Invalid access, need to do another request
	    // can occur if block is invalidated, or not correct 
	    // permissions
	    break;
	}
        
        if(cache->isShared){
            for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++){
                target->interferenceBreakdown[i] = fillRequest->interferenceBreakdown[i];
            }
            for(int i=0;i<MEM_REQ_LATENCY_BREAKDOWN_SIZE;i++){
                target->latencyBreakdown[i] = fillRequest->latencyBreakdown[i];
            }
        }
        
        mshr->popTarget();
        cache->respondToMiss(target, completion_time, mshr->hasTargets());
    }
    
    if (blk && cache->doData()) {
	// Need to write the data into the block
	updateData(blk, writebacks, !adaptiveCompression || true);
    }

    return blk;
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::pseudoFill(MSHR * mshr,
					CacheBlk::State new_state,
					MemReqList & writebacks)
{
    MemReqPtr req = mshr->req;
    assert(ct->findBlock(req->paddr, req->asid) == NULL);
    BlkType *blk = doReplacement(NULL, req, new_state, writebacks);

    if (cache->doData()) {
	if (req->cmd.isRead()) {
	    memcpy(blk->data, req->data, blkSize);
	}
    }
    
    fatal("latency breakdown not implemented in pseudoFill");

    blk->whenReady = curTick;
    // respond to MSHR targets, if any
    Tick response_base = curTick + ct->getHitLatency();
    
    while (mshr->hasTargets()) {
	MemReqPtr target = mshr->getTarget();
	MemCmd cmd = target->cmd;
	
	target->flags |= SATISFIED;
	Tick completion_time = response_base;
	
	if (cmd == Invalidate) {
	    invalidateBlk(blk);
	    blk = NULL;
	    continue;
	}
	
	if (cmd == Copy) {
	    writebacks.push_back(target);
	    break;
	}
	
	if (blk && (cmd.isWrite() ? blk->isWritable() : blk->isValid())) {
	    assert(cmd.isWrite() || cmd.isRead());
	    if (cmd.isWrite()) {
		ct->fixCopy(req, writebacks);
		blk->status |= BlkDirty;
		if (cache->doData()) {
		    assert(target->offset + target->size <= blkSize);
		    memcpy(blk->data + target->offset,
			   target->data, target->size);
		}
	    } else {
		if (cache->doData()) {
		    assert(target->offset + target->size <= blkSize);
		    memcpy(target->data, blk->data + target->offset,
			   target->size);
		}
	    }
	} else {
	    // Invalid access, need to do another request
	    // can occur if block is invalidated, or not correct 
	    // permissions
	    break;
	}
        
        mshr->popTarget();
        cache->respondToMiss(target, completion_time, mshr->hasTargets());
    }
    
    if (blk && cache->doData()) {
	// Need to write the data into the block
	updateData(blk, writebacks, !adaptiveCompression || true);
    }

    return blk;
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::handleTargets(MSHR *mshr, MemReqList &writebacks)
{
    MemReqPtr req = mshr->req;

    BlkType *blk = ct->findBlock(req->paddr, req->asid);
    assert(blk != NULL);

    bool write_data = false;
    
    // respond to MSHR targets, if any
    Tick response_base = curTick + ct->getHitLatency();
    
    fatal("latency breakdown not implemented in handleTargets");
    
    while (mshr->hasTargets()) {
	MemReqPtr target = mshr->getTarget();
	MemCmd cmd = target->cmd;
	
	target->flags |= SATISFIED;
	Tick completion_time = response_base;
	
	if (cmd == Invalidate) {
	    invalidateBlk(blk);
	    blk = NULL;
	    continue;
	}
	
	if (cmd == Copy) {
	    writebacks.push_back(target);
	    break;
	}
	
	if (blk && (cmd.isWrite() ? blk->isWritable() : blk->isValid())) {
	    assert(cmd.isWrite() || cmd.isRead());
	    if (cmd.isWrite()) {
		blk->status |= BlkDirty;
		ct->fixCopy(req, writebacks);
		if (cache->doData()) {
		    write_data = true;
		    assert(target->offset + target->size <= blkSize);
		    memcpy(blk->data + target->offset,
			   target->data, target->size);
		}
	    } else {
		if (cache->doData()) {
		    assert(target->offset + target->size <= blkSize);
		    memcpy(target->data, blk->data + target->offset,
			   target->size);
		}
	    }
	} else {
	    // Invalid access, need to do another request
	    // can occur if block is invalidated, or not correct 
	    // permissions
	    break;
	}
        
        mshr->popTarget();
        cache->respondToMiss(target, completion_time, mshr->hasTargets());
    }
    
    if (write_data && blk && cache->doData()) {
	// Need to write the data into the block
	updateData(blk, writebacks, !adaptiveCompression || true);
    }

    return blk;
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::handleSnoop(BlkType *blk, 
					 CacheBlk::State new_state, 
					 MemReqPtr &req)
{
    if (blk) {
	req->flags |= SATISFIED;
	if (cache->doData()) {
	    // Can only supply data
	    assert(req->cmd.isRead());
	    
	    assert(req->offset < blkSize);
	    assert(req->size <= blkSize);
	    assert(req->offset + req->size <=blkSize);
	    memcpy(req->data, blk->data + req->offset, req->size);   
	}
	handleSnoop(blk, new_state);
    }
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::handleSnoop(BlkType *blk,
					 CacheBlk::State new_state)
{
    if (blk && blk->status != new_state) {
	if ((new_state && BlkValid) == 0) {
	    invalidateBlk(blk);
	} else {
	    assert(new_state >= 0 && new_state < 128);
	    blk->status = new_state;
	}
    }
}

template <class Tags, class Compression>
MemReqPtr
CacheTags<Tags,Compression>::writebackBlk(BlkType *blk)
{
    assert(blk && blk->isValid() && blk->isModified());
    int data_size = blkSize;
    if (cache->doData()) {
	data_size = blk->size;
	if (!storeCompressed) {
	    // not already compressed
	    // need to compress to ship it
	    assert(data_size == blkSize);
	    uint8_t *tmp_data = new uint8_t[blkSize];
	    data_size = compress.compress(tmp_data,blk->data,
					  data_size);
	    delete [] tmp_data;
	}
    }
    
    MemReqPtr writeback = 
	buildWritebackReq(ct->regenerateBlkAddr(blk->tag, blk->set), 
			  blk->asid, blk->xc, blkSize, 
			  (cache->doData()) ? blk->data : 0, 
			  data_size);
    
    if(!cache->isShared){
        cache->setSenderID(writeback);
    }
    else{
        assert(blk->origRequestingCpuID != -1);
        
//         writeback->adaptiveMHASenderID = -1;
        writeback->adaptiveMHASenderID = blk->origRequestingCpuID;
    }
    
    blk->status &= ~BlkDirty;
    return writeback;
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::doUnalignedCopy(Addr source, Addr dest, int asid,
					     MemReqList &writebacks)
{    
    int source_offset = ct->extractBlkOffset(source);
    int dest_offset = ct->extractBlkOffset(dest);

    bool source_unaligned = source_offset != 0;
    bool dest_unaligned = dest_offset != 0;

    // Block addresses for unaligned accesses.
    Addr source1 = ct->blkAlign(source);;
    Addr source2 = source1 + blkSize;
    Addr dest1 = ct->blkAlign(dest);
    Addr dest2 = dest1 + blkSize;

    int lat;

    // Reference the blocks.
    // We need to do this to move the blocks into the primary tag store in 
    // the IIC so that the pointers can't be invalidated by moving the tag 
    // later.
    BlkType *source1_blk = ct->findBlock(source1, asid, lat);
    BlkType *source2_blk = ct->findBlock(source2, asid, lat);
    BlkType *dest1_blk = ct->findBlock(dest1, asid, lat);
    BlkType *dest2_blk = ct->findBlock(dest2, asid, lat);

    assert(source1_blk->xc);

    uint8_t *dest_data = NULL;

    // Need to read data out now in case one of the source blocks gets
    // replaced
    if (cache->doData()) {
	if (source_unaligned) {
	    dest_data = new uint8_t[blkSize];
	    memcpy(dest_data, source1_blk->data  + source_offset,
		   blkSize - source_offset);
	    memcpy(dest_data + blkSize - source_offset, source2_blk->data,
		   source_offset);
	} else {
	    dest_data = new uint8_t[blkSize];
	    memcpy(dest_data, source1_blk->data, blkSize);
	}
    }
    if (!dest1_blk) {
	assert(!dest_unaligned);
	// do replacement
	MemReqPtr req = new MemReq();
	req->paddr = dest;
	req->asid = asid;
	req->xc = source1_blk->xc;
	dest1_blk = doReplacement(NULL, req, 
				  BlkValid | BlkWritable,
				  writebacks);
    } else {
	MemReqPtr req = new MemReq();
	req->paddr = dest1;
	req->asid = asid;
	ct->fixCopy(req, writebacks);
    }
    
    if (dest_unaligned) {
	dest2_blk->xc = source1_blk->xc;
	dest2_blk->status |= BlkDirty;
	MemReqPtr req = new MemReq();
	req->asid = asid;
	req->paddr = dest2;
	ct->fixCopy(req, writebacks);
    }
    
    dest1_blk->status |= BlkDirty;
    
    if (cache->doData()) {
	assert(dest_data);
	if (dest_unaligned) {
	    // Copy first portion to dest1
	    memcpy(dest1_blk->data + dest_offset, dest_data,
		   blkSize - dest_offset);
	    // Copy second portion to dest2
	    memcpy(dest2_blk->data, dest_data + blkSize - dest_offset,
		   dest_offset);
	    updateData(dest1_blk, writebacks, !adaptiveCompression);
	    updateData(dest2_blk, writebacks, !adaptiveCompression);
	} else {
	    memcpy(dest1_blk->data, dest_data, blkSize);
	    updateData(dest1_blk, writebacks, !adaptiveCompression);
	}
	delete [] dest_data;
    }
}

template <class Tags, class Compression>
bool
CacheTags<Tags,Compression>::verifyData(BlkType *blk)
{
    bool retval;
    // The data stored in the blk
    uint8_t *blk_data = new uint8_t[blkSize];
    ct->readData(blk, blk_data);
    // Pointer for uncompressed data, assumed uncompressed
    uint8_t *tmp_data = blk_data;
    // The size of the data being stored, assumed uncompressed
    int data_size = blkSize;
    
    // If the block is compressed need to uncompress to access
    if (blk->isCompressed()){
	// Allocate new storage for the data
	tmp_data = new uint8_t[blkSize];
	data_size = compress.uncompress(tmp_data,blk_data,
					blk->size);
	assert(data_size == blkSize);
	// Don't need to keep blk_data around
	delete [] blk_data;
    } else {
	assert(blkSize == blk->size);
    }
    
    retval = memcmp(tmp_data, blk->data, blkSize) == 0;
    delete [] tmp_data;
    return retval;
}

template <class Tags, class Compression>
void
CacheTags<Tags,Compression>::updateData(BlkType *blk, MemReqList &writebacks,
					bool compress_block)
{
    if (storeCompressed && compress_block) {
	uint8_t *comp_data = new uint8_t[blkSize];
	int new_size = compress.compress(comp_data, blk->data,
					 blkSize);
	if (new_size > (blkSize - ct->getSubBlockSize())){
	    // no benefit to storing it compressed
	    blk->status &= ~BlkCompressed;
	    ct->writeData(blk, blk->data, blkSize, 
			  writebacks);
	} else {
	    // Store the data compressed
	    blk->status |= BlkCompressed;
	    ct->writeData(blk, comp_data, new_size, 
			  writebacks);
	}
	delete [] comp_data;
    } else {
	blk->status &= ~BlkCompressed;
	ct->writeData(blk, blk->data, blkSize, writebacks);
    }
}

template <class Tags, class Compression>
typename CacheTags<Tags,Compression>::BlkType*
CacheTags<Tags,Compression>::doReplacement(BlkType *blk, MemReqPtr &req, 
					   CacheBlk::State new_state, 
					   MemReqList &writebacks)
{
    
    if (blk == NULL) {
        
	// need to do a replacement
	BlkList compress_list;
	blk = ct->findReplacement(req, writebacks, compress_list);
        
	while (adaptiveCompression && !compress_list.empty()) {
	    updateData(compress_list.front(), writebacks, true);
	    compress_list.pop_front();
	}
        
	if (blk->isValid()) {
            
            if(cache->isDirectoryAndL1DataCache()){
                // This is a L1 data cache
                
                // All _data_ blocks must be owned
                assert(blk->owner >= 0);
                
                if(blk->dirState == DirOwnedExGR || blk->dirState == DirOwnedNonExGR){
                    int numCopies = 0;
                    for(int i=0;i<cache->cpuCount;i++){
                        if(blk->presentFlags[i] == true) numCopies++;
                    }
                    
                    assert(numCopies > 0);
                    
                    if(numCopies == 1){
                        
                        assert(blk->dirState == DirOwnedExGR);
                        
                        if(blk->isModified()){
                            writebacks.push_back(writebackBlk(blk));
                        }
                        else{
                            
                            MemReqPtr wbBlk = buildDirectoryReq(ct->regenerateBlkAddr(blk->tag,blk->set),
                                              blk->asid,
                                              blk->xc,
                                              blkSize, 
                                              (cache->doData()) ? blk->data : 0,
                                              DirWriteback);
                            writebacks.push_back(wbBlk);
                        }
                        
                    }
                    else{
                        MemReqPtr wbBlk = buildDirectoryReq(ct->regenerateBlkAddr(blk->tag,blk->set),
                                                            blk->asid,
                                                            blk->xc,
                                                            blkSize, 
                                                            (cache->doData()) ? blk->data : 0,
                                                            DirOwnerWriteback);
                        
                        bool* pfCopy = new bool[cache->cpuCount];
                        for(int i=0;i<cache->cpuCount;i++){
                            pfCopy[i] = blk->presentFlags[i];
                        }
                        
                        wbBlk->presentFlags = pfCopy;
                        writebacks.push_back(wbBlk);
                    }
                }
                else{

                    assert(blk->presentFlags == NULL);
                    assert(!blk->isModified());
                    
                    MemReqPtr wbBlk = buildDirectoryReq(ct->regenerateBlkAddr(blk->tag,blk->set),
                                      blk->asid,
                                      blk->xc,
                                      blkSize, 
                                      (cache->doData()) ? blk->data : 0,
                                      DirSharerWriteback);
                    
                    writebacks.push_back(wbBlk);
                }
                
                
                // update cache block state info from request, set other values to initial values
                blk->owner = req->owner;
                blk->dirState = DirNoState;
                if(blk->presentFlags != NULL){
                    delete blk->presentFlags;
                    blk->presentFlags = NULL;
                }
                
                // the block is written back so clear the dirty bit
                blk->status &= ~BlkDirty;
            }
            else{
            
                DPRINTF(Cache, "replacement %d replacing %x with %x: %s\n", 
                        blk->asid,
                        (ct->regenerateBlkAddr(blk->tag,blk->set) 
                        & (((ULL(1))<<48)-1)),req->paddr & (((ULL(1))<<48)-1),
                        (blk->isModified()) ? "writeback" : "clean");
                
                if (blk->isModified()) {
                    writebacks.push_back(writebackBlk(blk));
                }
            }
	}
        else{
            assert(blk->dirState != DirInvalid);
            assert(blk->dirState != DirOwnedExGR);
            assert(blk->dirState != DirOwnedNonExGR);
        }
        
        // set block values to the values of the new occupant
	blk->tag = ct->extractTag(req->paddr, blk);
	blk->asid = req->asid;
	assert(req->xc || !cache->doData());
	blk->xc = req->xc;
        
        if(cache->isShared){
            assert(req->adaptiveMHASenderID != -1);
            blk->origRequestingCpuID = req->adaptiveMHASenderID;
        }
        
    } else {
        
        if(!cache->isDirectoryAndL1DataCache()){
            // A cache might recieve the same cache line twice (read and write at the same time)
            // we don't want this warning every time ;-)
            if (blk->status == new_state) warn("Changing state to same value\n");
        }
    }
    
    blk->status = new_state;
    
    return blk;
}

template <class Tags, class Compression>
std::vector<int>
CacheTags<Tags,Compression>::perCoreOccupancy(){
    return ct->perCoreOccupancy();
}

