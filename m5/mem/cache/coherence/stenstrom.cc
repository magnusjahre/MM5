
#include "stenstrom.hh"

using namespace std;

template<class TagStore>
void
StenstromProtocol<TagStore>::sendDirectoryMessage(MemReqPtr& req, int lat){

    if(cache->isDirectoryAndL1DataCache()){
        directoryRequests.push_back(req);
        cache->setMasterRequest(Request_DirectoryCoherence, curTick + lat);
        return;
    }

    if(cache->isDirectoryAndL2Cache()){
        cache->respond(req, curTick + lat);
    }
}

template<class TagStore>
void
StenstromProtocol<TagStore>::sendNACK(MemReqPtr& req,
                                      int lat,
                                      int toID,
                                      int fromID){

    req->toProcessorID = toID;
    req->fromProcessorID = fromID;
    req->toInterfaceID = -1;
    req->fromInterfaceID = -1;
    req->dirNACK = true;

    numNACKs++;

    this->writeTraceLine(cacheName,
                   "NACK Sent",
                   -1,
                   DirNoState,
                   (req->paddr & ~((Addr)cache->getBlockSize() - 1)),
                   cache->getBlockSize(),
                   NULL);

    sendDirectoryMessage(req, lat);
}

template<class TagStore>
bool
StenstromProtocol<TagStore>::doDirectoryAccess(MemReqPtr& req){

    int lat = cache->getHitLatency();
    int fromCpuId = req->xc->cpu->params->cpu_id;
    Addr tmpL2BlkAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);

    // Directory info is only stored for data blocks
    if(!req->readOnlyCache){

        if(!parentPtr->isOwned(tmpL2BlkAddr)){

            if(req->dirNACK){

                if(req->cmd == DirSharerWriteback){

                    // the block is not shared anymore, discard message
                    this->writeTraceLine(cacheName,
                                   "Recieved NACK to block that is no longer "
                                   "shared, discarding message",
                                   -1,
                                   DirNoState,
                                   tmpL2BlkAddr,
                                   cache->getBlockSize(),
                                   NULL);
                    return true;
                }
                else if(req->cmd = DirOwnerTransfer){

                    // the old owner wrote back the block
                    // make the requester retransmit the request
                    assert(outstandingOwnerTransAddrs.find(tmpL2BlkAddr)
                            != outstandingOwnerTransAddrs.end());
                    outstandingOwnerTransAddrs.erase(
                            outstandingOwnerTransAddrs.find(tmpL2BlkAddr));

                    req->ownerWroteBack = true;
                    sendNACK(req, cache->getHitLatency(), fromCpuId, -1);
                    return true;
                }
                else{
                    fatal("Unimplemented NACK type in doDirectoryAccess()");
                    return true;
                }
            }

            if(req->cmd == DirRedirectRead){
                parentPtr->setOwner(tmpL2BlkAddr, fromCpuId);
                req->owner = fromCpuId;

                this->writeTraceLine(cacheName,
                               "Redirected Read to not owned block recieved, "
                               "requester is new owner",
                               parentPtr->getOwner(tmpL2BlkAddr),
                               DirNoState,
                               tmpL2BlkAddr,
                               cache->getBlockSize(),
                               NULL);

                req->toProcessorID = fromCpuId;
                req->fromProcessorID = -1;
                req->toInterfaceID = -1;
                req->fromInterfaceID = -1;

                sendDirectoryMessage(req, cache->getHitLatency());

                return true;
            }

            if(req->cmd == DirOwnerTransfer){
                // the previous owner wrote back the block
                // in the middle of the transfer
                // make the requester resend as a read
                assert(outstandingOwnerTransAddrs.find(tmpL2BlkAddr)
                        == outstandingOwnerTransAddrs.end());
                req->ownerWroteBack = true;
                sendNACK(req, cache->getHitLatency(), fromCpuId, -1);
                return true;
            }

            assert(!req->cmd.isDirectoryMessage());

            // The requesting cache is now the owner, do normal response
            parentPtr->setOwner(tmpL2BlkAddr, fromCpuId);
            req->owner = fromCpuId;
            req->fromProcessorID = -1;
        }
        else{

            if(req->dirACK){
                if(req->cmd == DirOwnerTransfer){

                    // owner transfer to this block is allowed again
                    outstandingOwnerTransAddrs.erase(
                            outstandingOwnerTransAddrs.find(tmpL2BlkAddr));

                    this->writeTraceLine(cacheName,
                                   "Owner Transfer ACK recieved",
                                   parentPtr->getOwner(tmpL2BlkAddr),
                                   DirNoState,
                                   tmpL2BlkAddr,
                                   cache->getBlockSize(),
                                   NULL);

                    return true;
                }
                else{
                    fatal("ACK type not implemented");
                }
            }
            else if(req->dirNACK){
                if(req->cmd == DirOwnerTransfer){
                    // the owner had not recieved the block yet
                    // clean up and send a NACK to the requester
                    // NOTE: use fromProcessorID here
                    // fromCPUId identifies the original requester
                    assert(fromCpuId == parentPtr->getOwner(tmpL2BlkAddr));
                    assert(req->fromProcessorID > -1);

                    // reset too original owner and remove address
                    // from blocked list
                    int requesterID = parentPtr->getOwner(tmpL2BlkAddr);
                    parentPtr->setOwner(tmpL2BlkAddr, req->fromProcessorID);
                    outstandingOwnerTransAddrs.erase(
                            outstandingOwnerTransAddrs.find(tmpL2BlkAddr));

                    sendNACK(req, lat, requesterID, -1);
                    return true;
                }
                else if(req->cmd == DirSharerWriteback){

                    req->dirNACK = false;
                    req->toProcessorID = parentPtr->getOwner(tmpL2BlkAddr);
                    req->fromProcessorID = -1;
                    req->toInterfaceID = -1;

                    this->writeTraceLine(cacheName,
                                   "Recieved NACK on sharer writeback, "
                                   "retransmitting",
                                   parentPtr->getOwner(tmpL2BlkAddr),
                                   DirNoState,
                                   tmpL2BlkAddr,
                                   cache->getBlockSize(),
                                   NULL);

                    sendDirectoryMessage(req, lat);

                    return true;
                }
                else{
                    fatal("Unimplemented NACK type (in L2)");
                }
            }
            else if(req->writeMiss){

                if(outstandingOwnerTransAddrs.find(tmpL2BlkAddr)
                   != outstandingOwnerTransAddrs.end()){
                    //destination was req->fromProcessorID
                    sendNACK(req, lat, fromCpuId, -1);
                    return true;
                }
                outstandingOwnerTransAddrs[tmpL2BlkAddr] = fromCpuId;

                // write to allready owned block
                int oldOwner = parentPtr->getOwner(tmpL2BlkAddr);
                parentPtr->setOwner(tmpL2BlkAddr, fromCpuId);

                this->writeTraceLine(cacheName,
                               "Write miss to owned block",
                               oldOwner,
                               DirNoState,
                               tmpL2BlkAddr,
                               cache->getBlockSize(),
                               NULL);

                /* update and send request */
                setUpOwnerTransferInL2(req, oldOwner, fromCpuId);
                sendDirectoryMessage(req, lat);

                return true;

            }
            else if(req->cmd == Writeback || req->cmd == DirWriteback){

                if(outstandingOwnerTransAddrs.find(tmpL2BlkAddr)
                   == outstandingOwnerTransAddrs.end()){
                    // not part of an owner transfer, must be from owner
                    assert(parentPtr->getOwner(tmpL2BlkAddr) == fromCpuId);
                }

                // this block is not in any L1 cache, reset owner status
                parentPtr->removeOwner(tmpL2BlkAddr);

                // carry out normal writeback actions in the write back case
                if(req->cmd == DirWriteback){
                    return true;
                }

            }
            else if(req->cmd == DirOwnerTransfer){

                if(outstandingOwnerTransAddrs.find(tmpL2BlkAddr)
                   != outstandingOwnerTransAddrs.end()){
                    //destination was req->fromProcessorID
                    sendNACK(req, lat, fromCpuId, -1);
                    return true;
                }
                outstandingOwnerTransAddrs[tmpL2BlkAddr] = fromCpuId;

                // change owner status and forward to old owner
                int oldOwner = parentPtr->getOwner(tmpL2BlkAddr);
                int newOwner = req->fromProcessorID;

                parentPtr->setOwner(tmpL2BlkAddr, newOwner);

                this->writeTraceLine(cacheName,
                               "Owner Change Request Granted",
                               parentPtr->getOwner(tmpL2BlkAddr),
                               DirNoState,
                               tmpL2BlkAddr,
                               cache->getBlockSize(),
                               NULL);

                /* update and send request */
                setUpOwnerTransferInL2(req, oldOwner, newOwner);
                sendDirectoryMessage(req, lat);

                return true;
            }
            else if(req->cmd == Read){

                // return the owner state to the requesting cache
                int owner = parentPtr->getOwner(tmpL2BlkAddr);

                assert(fromCpuId != owner);

                req->owner = owner;
            }
            else if(req->cmd == DirSharerWriteback){
                // block replaced from a non-owner cache, inform owner
                assert(req->replacedByID == -1);

                req->toProcessorID = parentPtr->getOwner(tmpL2BlkAddr);
                req->fromProcessorID = -1;
                req->toInterfaceID = -1;
                req->fromInterfaceID = -1;
                req->replacedByID = fromCpuId;

                this->writeTraceLine(cacheName,
                               "Forwarding sharer writeback to owner",
                               req->owner,
                               DirNoState,
                               tmpL2BlkAddr,
                               cache->getBlockSize(),
                               req->presentFlags);

                sendDirectoryMessage(req, lat);
                return true;
            }
            else if(req->cmd == DirRedirectRead){
                // a redirected read got NACKed
                // return the owner state to the requester
                req->owner = parentPtr->getOwner(tmpL2BlkAddr);
                req->toProcessorID = req->fromProcessorID;
                req->fromProcessorID = -1;
                req->toInterfaceID = -1;
                req->fromInterfaceID = -1;

                this->writeTraceLine(cacheName,
                               "Got Redirected Read, "
                               "informing requester of current owner",
                               req->owner,
                               DirNoState,
                               tmpL2BlkAddr,
                               cache->getBlockSize(),
                               NULL);

                sendDirectoryMessage(req, lat);
                return true;
            }
            else{
                fatal("In L2: access to a block that is allready owned, "
                      "unimplemented request type");
            }
        }
    }
    return false;
}

template<class TagStore>
bool
StenstromProtocol<TagStore>::doL1DirectoryAccess(MemReqPtr& req, BlkType* blk){

    int lat = cache->getHitLatency();
    Addr tmpL1BlkAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);

    if(!req->cmd.isDirectoryMessage()){
        assert(req->xc->cpu->params->cpu_id == cache->getCacheCPUid());
    }

    switch(req->cmd){

        case Write:
            if(blk->dirState == DirOwnedExGR
               || blk->dirState == DirOwnedNonExGR){
                // Write is OK
            }
            else{
                // we need to request ownership for this block
                req->oldCmd = req->cmd;
                req->cmd = DirOwnerTransfer;
                req->toProcessorID = -1;
                req->toInterfaceID = -1;
                req->fromProcessorID = cache->getCacheCPUid();

                numOwnerRequests++;

                this->writeTraceLine(cacheName,
                               "Issuing owner transfer request",
                               blk->owner,
                               blk->dirState,
                               tmpL1BlkAddr,
                               cache->getBlockSize(),
                               blk->presentFlags);

                sendDirectoryMessage(req, lat);

                return true;
            }
            break;

        case Read:
            if(blk->dirState == DirOwnedExGR
               || blk->dirState == DirOwnedNonExGR){
                // Read is OK
            }
            else{
                setUpRedirectedRead(req, cache->getCacheCPUid(), blk->owner);

                numRedirectedReads++;

                this->writeTraceLine(cacheName,
                               "Issuing Redirected Read (1)",
                               blk->owner,
                               blk->dirState,
                               tmpL1BlkAddr,
                               cache->getBlockSize(),
                               blk->presentFlags);

                sendDirectoryMessage(req,lat);

                return true;
            }
            break;

        case Soft_Prefetch:
            // discard, since it is a hit in the L1 cache we don't care
            break;

        case DirRedirectRead:

            if(blk->dirState == DirOwnedExGR){

                // make sure it is really one owner
                int presentCount = 0;
                for(int i=0;i<cache->cpuCount;i++){
                    if(blk->presentFlags[i]) presentCount++;
                }
                assert(presentCount == 1);

                // update block state
                int fromCpuID = req->fromProcessorID;
                blk->presentFlags[fromCpuID] = true;

                // this request might be from ourselves
                int newSharerCount = 0;
                for(int i=0;i<cache->cpuCount;i++){
                    if(blk->presentFlags[i]) newSharerCount++;
                }
                if(newSharerCount == 1){
                    // the block is present in our cache, answer the request
                    this->writeTraceLine(cacheName,
                                   "Answering Redirected Read from myself "
                                   "(return to cache) (1)",
                                   blk->owner,
                                   blk->dirState,
                                   tmpL1BlkAddr,
                                   cache->getBlockSize(),
                                   blk->presentFlags);

                    blk->dirState = DirOwnedExGR;
                    return false;
                }


                blk->dirState = DirOwnedNonExGR;

                // update request and send it
                setUpRedirectedReadReply(req,
                                         cache->getCacheCPUid(),
                                         fromCpuID);

                sendDirectoryMessage(req, lat);

            }
            else if(blk->dirState == DirOwnedNonExGR){

                int fromCpuID = req->fromProcessorID;

                // sending redirected reads to ourselves
                // will cause a deadlock, let it finish
                if(fromCpuID == cache->getCacheCPUid()){
                    this->writeTraceLine(cacheName,
                                   "Answering Redirected Read from myself "
                                   "(return to cache) (2)",
                                   blk->owner,
                                   blk->dirState,
                                   tmpL1BlkAddr,
                                   cache->getBlockSize(),
                                   blk->presentFlags);

                    return false;
                }
                assert(fromCpuID != cache->getCacheCPUid());

                blk->presentFlags[fromCpuID] = true;
                setUpRedirectedReadReply(req,
                                         cache->getCacheCPUid(),
                                         fromCpuID);

                sendDirectoryMessage(req, lat);
            }
            else{
                sendNACK(req,
                         lat,
                         req->fromProcessorID,
                         cache->getCacheCPUid());
                return true;
            }

            this->writeTraceLine(cacheName,
                           "Answering Redirected Read Request",
                           blk->owner,
                           blk->dirState,
                           tmpL1BlkAddr,
                           cache->getBlockSize(),
                           blk->presentFlags);

            return true;

            break;

        case DirOwnerTransfer:
        {

            // Owner transfer is finished
            // Update the block state and let the request go through
            assert(req->owner == cache->getCacheCPUid());
            assert(req->presentFlags != NULL);
            // ownership might be requested by more than one request
            // consequently, we might be transfering ownership to ourselves
            // i.e. no blk->owner != req->owner assertion
            assert(blk->presentFlags == NULL);

            blk->presentFlags = req->presentFlags;
            // the flags must not be removed when the request is deleted
            req->presentFlags = NULL;
            blk->owner = req->owner;

            // the previous owner does not necessarily know about this cache
            blk->presentFlags[cache->getCacheCPUid()] = true;

            int sharerCount = 0;
            for(int i=0;i<cache->cpuCount;i++){
                if(blk->presentFlags[i]) sharerCount++;
            }

            assert(sharerCount > 0);
            if(sharerCount == 1) blk->dirState = DirOwnedExGR;
            else blk->dirState = DirOwnedNonExGR;

            this->writeTraceLine(cacheName,
                           "Owner transfer complete",
                           blk->owner,
                           blk->dirState,
                           tmpL1BlkAddr,
                           cache->getBlockSize(),
                           blk->presentFlags);

            // Send ACK to the L2 cache
            MemReqPtr tmpReq = buildReqCopy(req,
                                            cache->cpuCount,
                                            DirOwnerTransfer);
            setUpACK(tmpReq, -1, cache->getCacheCPUid());
            sendDirectoryMessage(tmpReq, cache->getHitLatency());
        }

        break;

        case DirOwnerWriteback:
        {
            // hit on owner writeback
            assert(req->presentFlags == NULL);
            assert(req->paddr ==
                    (req->paddr & ~((Addr)cache->getBlockSize() - 1)));

            // make a copy that will become the ownership request
            MemReqPtr ownershipReq = buildReqCopy(req,
                                                  cache->cpuCount,
                                                  DirOwnerTransfer);

            // set addressing info
            ownershipReq->toProcessorID = -1;
            ownershipReq->fromProcessorID = cache->getCacheCPUid();
            ownershipReq->toInterfaceID = -1;

            // send an ACK to the current owner
            req->toProcessorID = req->fromProcessorID;
            req->fromProcessorID = cache->getCacheCPUid();
            req->toInterfaceID = -1;
            req->fromInterfaceID = -1;
            req->dirACK = true;

            // send the messages
            sendDirectoryMessage(req, lat);
            sendDirectoryMessage(ownershipReq, lat);

            this->writeTraceLine(cacheName,
                           "Accepting ownership, ACK and ownership request sent",
                           blk->owner,
                           blk->dirState,
                           tmpL1BlkAddr,
                           cache->getBlockSize(),
                           blk->presentFlags);

            return true;
        }
        break;

        default:
            cout << req->cmd.toString() << "\n";
            fatal("L1: cache access(), unknown request type");
    }

    return false;
}

template<class TagStore>
bool
StenstromProtocol<TagStore>::handleDirectoryResponse(MemReqPtr& req,
                                                     TagStore *tags){

    if(req->dirNACK){

        if(req->cmd == DirOwnerTransfer){

            req->dirNACK = false;
            req->flags &= ~SATISFIED;

            BlkType* tmpBlk = tags->findBlock(req);

            if(req->ownerWroteBack){
                assert(req->writeMiss);
                req->cmd = Read;

                this->writeTraceLine(cacheName,
                               "Owner Transfer NACK recieved, owner wrote back,"
                               " retransmitting as read",
                               req->owner,
                               DirNoState,
                               req->paddr,
                               cache->getBlockSize(),
                               NULL);
            }
            else if(tmpBlk != NULL &&
                    (tmpBlk->dirState == DirOwnedExGR
                    || tmpBlk->dirState == DirOwnedNonExGR)){

                this->writeTraceLine(cacheName,
                               "Owner Transfer NACK recieved, "
                               "we have become the owner",
                               tmpBlk->owner,
                               tmpBlk->dirState,
                               req->paddr,
                               cache->getBlockSize(),
                               NULL);

                // we have become the owner, return request to processor
                if(req->mshr == NULL){
                    assert(req->completionEvent != NULL);
                    cache->respond(req, curTick + cache->getHitLatency());
                    return true;
                }
                else{
                    assert(req->mshr != NULL);
                    cache->missQueueHandleResponse(req,
                            curTick + cache->getHitLatency());
                    return true;
                }
            }
            else{
                this->writeTraceLine(cacheName,
                               "Owner Transfer NACK recieved, "
                               "retransmitting to L2 cache",
                               req->owner,
                               DirNoState,
                               req->paddr,
                               cache->getBlockSize(),
                               NULL);
            }

            req->toProcessorID = -1;
            req->fromProcessorID = cache->getCacheCPUid();
            req->fromInterfaceID = -1;
            req->toInterfaceID = -1;

            sendDirectoryMessage(req, cache->getHitLatency());

            return true;
        }
        else if(req->cmd == DirRedirectRead){

            req->dirNACK = false;
            req->flags &= ~SATISFIED;

            this->writeTraceLine(cacheName,
                           "Redirected Read NACK recieved, "
                           "retransmitting to L2 cache",
                           req->owner,
                           DirNoState,
                           req->paddr,
                           cache->getBlockSize(),
                           NULL);

            req->toProcessorID = -1;
            req->fromProcessorID = cache->getCacheCPUid();
            req->fromInterfaceID = -1;
            req->toInterfaceID = -1;

            sendDirectoryMessage(req, cache->getHitLatency());

            return true;
        }
        else if(req->cmd == DirOwnerWriteback){

            assert(req->presentFlags == NULL);

            Addr tmpBlkAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);
            assert(outstandingWritebackWSAddrs.find(tmpBlkAddr)
                    != outstandingWritebackWSAddrs.end());

            outstandingWritebackWSAddrs[tmpBlkAddr][req->fromProcessorID]
                    = false;
            bool* tmpPresentFlags = outstandingWritebackWSAddrs[tmpBlkAddr];

            int presentCount = 0;
            for(int i=0;i<cache->cpuCount;i++){
                if(tmpPresentFlags[i]) presentCount++;
            }

            req->dirNACK = false;

            if(presentCount == 0){
                // no other sharers left, send to L2
                req->toProcessorID = -1;
                req->fromProcessorID = cache->getCacheCPUid();
                req->toInterfaceID = -1;
                req->fromInterfaceID = -1;

                req->cmd = DirWriteback;

                sendDirectoryMessage(req, cache->getHitLatency());

                // writeback has been handled, remove it
                outstandingWritebackWSAddrs.erase(
                        outstandingWritebackWSAddrs.find(tmpBlkAddr));

                this->writeTraceLine(cacheName,
                               "No sharers left, doing normal writeback",
                               req->owner,
                               DirNoState,
                               req->paddr,
                               cache->getBlockSize(),
                               tmpPresentFlags);

                return true;
            }

            else{
                int nextSharer = -1;
                for(int i=0;i<cache->cpuCount;i++){
                    if(tmpPresentFlags[i]){
                        nextSharer = i;
                        break;
                    }
                }
                assert(nextSharer != -1);

                req->toProcessorID = nextSharer;
                req->fromProcessorID = cache->getCacheCPUid();
                req->toInterfaceID = -1;
                req->fromInterfaceID = -1;

                this->writeTraceLine(cacheName,
                               "Attempting to transfer ownership to "
                               "different sharer",
                               req->owner,
                               DirNoState,
                               req->paddr,
                               cache->getBlockSize(),
                               tmpPresentFlags);

                sendDirectoryMessage(req, cache->getHitLatency());

                return true;
            }
        }
        else if(req->cmd == Read){
            assert(req->fromProcessorID == -1);

            if(req->writeMiss){
                assert(req->writeMiss);

                // change the request to an owner transfer and resend
                req->cmd = DirOwnerTransfer;
                req->dirNACK = false;
                req->fromProcessorID = cache->getCacheCPUid();
                req->toProcessorID = -1;
                req->toInterfaceID = -1;

                this->writeTraceLine(cacheName,
                               "Write miss, recieved NACK, retransmitting",
                               -1,
                               DirNoState,
                               req->paddr,
                               cache->getBlockSize(),
                               NULL);


                //TODO: have a different retransmit delay?
                sendDirectoryMessage(req, cache->getHitLatency());
                return true;
            }
            else{
                fatal("NACK on read miss not implemented");
            }
        }
        else{
            fatal("Recieved NACK, request type not implemented");
        }
    }
    else if(req->cmd == DirOwnerWriteback){

        assert(!req->isDirectoryNACK());

        if(req->isDirectoryACK()){
            assert(req->presentFlags == NULL);

            this->writeTraceLine(cacheName,
                           "Owner with sharers, recieved ACK",
                           req->owner,
                           DirNoState,
                           req->paddr,
                           cache->getBlockSize(),
                           NULL);

            return true;
        }
        else{
            // recieved a request to take over ownership of this block
            assert(req->owner != cache->getCacheCPUid());
            cache->access(req);
            return true;
        }

    }
    else if(req->cmd == DirSharerWriteback){

        // a sharer has written back a block owned by this cache
        BlkType* tmpBlk = tags->findBlock(req);
        if(tmpBlk == NULL){
            // we are in the process of writing back the block
            Addr tmpBlkAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);
            if(outstandingWritebackWSAddrs.find(tmpBlkAddr)
               == outstandingWritebackWSAddrs.end()){
                // we have written back the block
                // or the block has been given a new owner
                // send NACK to L2 and let it handle it
                sendNACK(req,
                         cache->getHitLatency(),
                         -1,
                         cache->getCacheCPUid());
                return true;
            }

            assert(outstandingWritebackWSAddrs.find(tmpBlkAddr)
                    != outstandingWritebackWSAddrs.end());
            outstandingWritebackWSAddrs[tmpBlkAddr][req->replacedByID] = false;

            this->writeTraceLine(cacheName,
                           "Sharer writeback recieved to block that is "
                           "being written back",
                           -1,
                           DirNoState,
                           req->paddr,
                           cache->getBlockSize(),
                           outstandingWritebackWSAddrs[tmpBlkAddr]);

            return true;
        }

        if(tmpBlk->dirState == DirInvalid){
            // we are in the middle of an owner transfer and
            // haven't recieved the data yet
            // make the L2 resend the request
            sendNACK(req, cache->getHitLatency(), -1, cache->getCacheCPUid());
            return true;
        }

        assert(tmpBlk != NULL);
        assert(tmpBlk->dirState == DirOwnedNonExGR
                || tmpBlk->dirState == DirOwnedExGR);
        assert(tmpBlk->presentFlags != NULL);

        assert(req->replacedByID >= 0);
        if(req->replacedByID == cache->cacheCpuID){
            // if a sharer and the owner writes back a block at the same time
            // this can happen, discard this update
            this->writeTraceLine(cacheName,
                           "Recieved sharer writeback from ourselves",
                           tmpBlk->owner,
                           tmpBlk->dirState,
                           req->paddr,
                           cache->getBlockSize(),
                           tmpBlk->presentFlags);
            return true;
        }

        tmpBlk->presentFlags[req->replacedByID] = false;

        int sharers = 0;
        for(int i=0;i<cache->cpuCount;i++){
            if(tmpBlk->presentFlags[i]) sharers++;
        }

        assert(sharers > 0);
        if(sharers == 1) tmpBlk->dirState = DirOwnedExGR;
        else tmpBlk->dirState = DirOwnedNonExGR;

        this->writeTraceLine(cacheName,
                       "Sharer writeback recieved and handled",
                       tmpBlk->owner,
                       tmpBlk->dirState,
                       req->paddr,
                       cache->getBlockSize(),
                       tmpBlk->presentFlags);

        return true;
    }
    else if(req->cmd == DirNewOwnerMulticast){

        BlkType* tmpBlk = tags->findBlock(req);

        if(tmpBlk == NULL){
            //we have written back this block and don't care who owns it
            this->writeTraceLine(cacheName,
                           "Owner info discarded, block written back",
                           -1,
                           DirNoState,
                           req->paddr,
                           cache->getBlockSize(),
                           NULL);
            return true;
        }

        assert(tmpBlk != NULL);
        assert(tmpBlk->dirState == DirInvalid);
        assert(tmpBlk->presentFlags == NULL);
        assert(req->owner >= 0);

        tmpBlk->owner = req->owner;

        this->writeTraceLine(cacheName,
                       "New owner info recieved and stored",
                       tmpBlk->owner,
                       tmpBlk->dirState,
                       req->paddr,
                       cache->getBlockSize(),
                       tmpBlk->presentFlags);

        return true;
    }
    else if(req->owner != -1
            && req->owner != cache->getCacheCPUid()
            && req->fromProcessorID == -1
            && req->cmd != DirOwnerTransfer){
        // This is an L1 cache, but is not the owner

        // redirected request to owner, must be a read
        assert(!req->writeMiss);

        setUpRedirectedRead(req, cache->getCacheCPUid(), req->owner);

        numRedirectedReads++;

        this->writeTraceLine(cacheName,
                       "Issuing Redirected Read (2)",
                       req->owner,
                       DirNoState,
                       req->paddr,
                       cache->getBlockSize(),
                       NULL);

        sendDirectoryMessage(req, cache->getHitLatency());

        return true;
    }
    else if(req->fromProcessorID != -1
            && req->owner == cache->getCacheCPUid()){
        // Case 1: request from a different L1 cache and we are the owner
        // Case 2: we have recieved the owner state at the end of
        //         a owner transfer request
        if(req->writeMiss){
            // this is a cache fill, let it through
        }
        else{
            assert(req->cmd == DirRedirectRead || req->cmd == DirOwnerTransfer);

            if(req->cmd == DirOwnerTransfer){
                // the block must be in the cache for this forwarding to work
                BlkType* tmpBlk = tags->findBlock(req);
                if(tmpBlk == NULL){
                    // we have replaced this block, let it back in
                    assert(req->mshr == NULL);
                    return false;
                }
            }

            cache->access(req);
            return true;
        }
    }
    else if(req->cmd == DirRedirectRead
            && req->owner == cache->getCacheCPUid()
            && req->fromProcessorID == -1){

        if(req->mshr != NULL){
            // a mshr is allocated
            // let it go through and handle the fill normally
            return false;
        }
        else{
            // no MSHR is allocated because the block is allready in our cache

            // this code assumes that we have become the owner while the
            // Redirected Read  has been transported through the system
            BlkType* tmpBlk = tags->findBlock(req);
            assert(tmpBlk != NULL);
            if(tmpBlk->dirState == DirInvalid){
                // we have not become the owner yet, send nack to ourselves
                sendNACK(req,
                         cache->getHitLatency(),
                         cache->getCacheCPUid(),
                         cache->getCacheCPUid());
                return true;
            }
            assert(tmpBlk->dirState == DirOwnedExGR
                    || tmpBlk->dirState == DirOwnedNonExGR);
            assert(tmpBlk->owner == cache->getCacheCPUid());

            this->writeTraceLine(cacheName,
                           "This cache is the owner, send response to CPU",
                           req->owner,
                           DirNoState,
                           req->paddr,
                           cache->getBlockSize(),
                           NULL);

            assert(req->completionEvent != NULL);
            cache->respond(req, curTick + cache->getHitLatency());

            return true;
        }
    }

    return false;
}

template<class TagStore>
bool
StenstromProtocol<TagStore>::handleDirectoryFill(MemReqPtr& req,
                                                 BlkType* blk,
                                                 MemReqList& writebacks,
                                                 TagStore* tags){

    // This is an L1 data cache
    if(req->cmd == DirOwnerTransfer){

        if(req->fromProcessorID == -1){

            int newOwner = -1;
            bool* oldFlags = NULL;

            if(outstandingWritebackWSAddrs.find(
               req->paddr & ~((Addr)cache->getBlockSize() - 1))
               != outstandingWritebackWSAddrs.end()){

                //remove this entry
                Addr tmpAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);
                oldFlags = outstandingWritebackWSAddrs[tmpAddr];
                outstandingWritebackWSAddrs.erase(
                        outstandingWritebackWSAddrs.find(tmpAddr));

                // no need to check for other sharers
                // (end of ownership replacement with sharers)
                newOwner = req->owner;
                req->toProcessorID = newOwner;
                req->fromProcessorID = cache->getCacheCPUid();
                req->presentFlags = oldFlags;
                req->owner = newOwner;

                this->writeTraceLine(cacheName,
                               "Transfering Owner State "
                               "(end of owner with sharers replacement)",
                               newOwner,
                               DirInvalid,
                               req->paddr,
                               cache->getBlockSize(),
                               oldFlags);

            }
            else{

                // Owner transfer recieved from L2, we must be the owner
                if(blk == NULL){
                    // The block hasn't been delivered to us yet
                    // or we have written it back, send NACK
                    sendNACK(req,
                             cache->getHitLatency(),
                             -1,
                             cache->getCacheCPUid());
                    return true;
                }

                assert(blk->dirState == DirOwnedExGR
                        || blk->dirState == DirOwnedNonExGR);
                assert(blk->presentFlags != NULL);

                newOwner = req->owner;
                blk->presentFlags[newOwner] = true;
                oldFlags = blk->presentFlags;

                // update local block
                blk->presentFlags = NULL;
                blk->dirState = DirInvalid;
                blk->status &= ~BlkDirty;
                blk->owner = newOwner;

                this->writeTraceLine(cacheName,
                               "Transfering Owner State",
                               blk->owner,
                               blk->dirState,
                               req->paddr,
                               cache->getBlockSize(),
                               oldFlags);

                // send owner state to new owner
                req->toProcessorID = newOwner;
                req->fromProcessorID = cache->getCacheCPUid();
                req->presentFlags = oldFlags;
                req->owner = newOwner;
            }

            assert(oldFlags != NULL);
            assert(newOwner != -1);
            for(int i=0;i<cache->cpuCount;i++){
                if(oldFlags[i]
                   && i != cache->getCacheCPUid()
                   && i != newOwner){
                    // send request to all other sharers
                    MemReqPtr tmpReq = buildReqCopy(req,
                                                    cache->cpuCount,
                                                    DirNewOwnerMulticast);
                    assert(tmpReq->cmd == DirNewOwnerMulticast);
                    tmpReq->toProcessorID = i;
                    tmpReq->presentFlags = NULL;

                    this->writeTraceLine(cacheName,
                                   "Informing sharer of new owner",
                                   (blk != NULL) ? blk->owner : -1,
                                   (blk != NULL) ? blk->dirState : DirNoState,
                                   req->paddr,
                                   cache->getBlockSize(),
                                   oldFlags);

                    sendDirectoryMessage(tmpReq, cache->getHitLatency());
                }
            }

            // send request to new owner
            sendDirectoryMessage(req, cache->getHitLatency());

            return true;
        }
        else if(req->writeMiss){

            // We have recieved ownership of a block because of a L1 write miss
            assert(cache->getCacheCPUid() == req->owner);

            int sharerCount = 0;
            for(int i=0;i<cache->cpuCount;i++){
                if(req->presentFlags[i]) sharerCount++;
            }

            // we must get the block, because the the previous call is bypassed
            CacheBlk::State old_state = (blk) ? blk->status : 0;
            blk = tags->handleFill(blk,
                                   req->mshr,
                                   cache->getNewCoherenceState(req, old_state),
                                   writebacks);
            blk->owner = cache->getCacheCPUid();
            blk->presentFlags = req->presentFlags;

            if(sharerCount > 1) blk->dirState = DirOwnedNonExGR;
            else blk->dirState = DirOwnedExGR;

            // remove the reference to these flags, so they are
            // not deleted together with the request
            req->presentFlags = NULL;

            this->writeTraceLine(cacheName,
                           "Recieved owner state (write miss)",
                           blk->owner,
                           blk->dirState,
                           req->paddr,
                           cache->getBlockSize(),
                           blk->presentFlags);

            // Send ACK to the L2 cache
            MemReqPtr tmpReq = buildReqCopy(req,
                                            cache->cpuCount,
                                            DirOwnerTransfer);
            setUpACK(tmpReq, -1, cache->getCacheCPUid());
            sendDirectoryMessage(tmpReq, cache->getHitLatency());

        }
        else{
            // the block was replaced in the middle of an owner transfer
            // put it back in and update the stats
            assert(req->mshr == NULL);
            assert(blk == NULL);

            blk = tags->handleFill(blk,
                                   req,
                                   BlkValid | BlkWritable,
                                   writebacks);

            blk->owner = cache->getCacheCPUid();
            blk->presentFlags = req->presentFlags;
            blk->presentFlags[cache->getCacheCPUid()] = true;
            blk->dirState = DirOwnedNonExGR;

            // remove the reference to these flags, so they are
            // not deleted together with the request
            req->presentFlags = NULL;

            this->writeTraceLine(cacheName,
                           "Owner transfer complete, needed block was replaced",
                           blk->owner,
                           blk->dirState,
                           req->paddr,
                           cache->getBlockSize(),
                           blk->presentFlags);

            // check that the cache fill worked
            assert(tags->findBlock(req->paddr, req->asid) != NULL);

            // Send ACK to the L2 cache
            MemReqPtr tmpReq = buildReqCopy(req,
                                            cache->cpuCount,
                                            DirOwnerTransfer);
            setUpACK(tmpReq, -1, cache->getCacheCPUid());
            sendDirectoryMessage(tmpReq, cache->getHitLatency());

            return true;
        }

    }
    else if(req->cmd == DirRedirectRead){
        //response from a redirected read recieved

        if(blk->owner == cache->getCacheCPUid()
           || req->owner == cache->getCacheCPUid()){
            // CASE 1: we have become the owner while
            //         the redirected read was in transit
            // CASE 2: the previous owner wrote the line back while the RR was
            //         in transit and we are the new owner

            // the block might be brought into the cache so it might not have a state yet
	    if(blk->dirState == DirNoState){

	        assert(req->presentFlags == NULL);
	        blk->presentFlags = new bool[cache->cpuCount];
		for(int i=0;i<cache->cpuCount;i++){
                    blk->presentFlags[i] = false;
                }
		blk->presentFlags[cache->getCacheCPUid()] = true;
		blk->dirState = DirOwnedExGR;
                blk->owner = cache->getCacheCPUid(); // needed in case 2
	    }
	    assert(blk->dirState == DirOwnedExGR
                    || blk->dirState == DirOwnedNonExGR);
            assert(blk->presentFlags != NULL);
            assert(req->presentFlags == NULL);

            this->writeTraceLine(cacheName,
                           "Redirected Read Response Recieved,"
                           " we have become the owner",
                           blk->owner,
                           blk->dirState,
                           req->paddr,
                           cache->getBlockSize(),
                           blk->presentFlags);
        }
        else{
            assert(blk->owner != cache->getCacheCPUid());
            assert(blk->presentFlags == NULL);

            blk->dirState = DirInvalid;
            blk->owner = req->owner;
            this->writeTraceLine(cacheName,
                           "Redirected Read Response Recieved",
                           blk->owner,
                           blk->dirState,
                           req->paddr,
                           cache->getBlockSize(),
                           blk->presentFlags);
        }

        if(req->mshr == NULL){

            assert(req->completionEvent != NULL);
            cache->respond(req, curTick);
            return true;
        }
    }
    else if(req->cmd == Read
            && (blk->dirState == DirOwnedExGR
            || blk->dirState == DirOwnedNonExGR)){
        // command is a read or write and we are the owner
        // let it go through
        assert(req->mshr != NULL);
        return false;
    }
    else if(req->fromProcessorID == -1
            && req->owner == cache->getCacheCPUid()){

        assert(blk->presentFlags == NULL);
        assert(blk->dirState != DirOwnedExGR);
        assert(blk->dirState != DirOwnedNonExGR);

        blk->owner = req->owner;
        blk->dirState = DirOwnedExGR;

        if(blk->presentFlags == NULL){
            blk->presentFlags = new bool[cache->cpuCount];
        }

        for(int i=0;i<cache->cpuCount;i++){
            blk->presentFlags[i] = false;
        }
        blk->presentFlags[cache->getCacheCPUid()] = true;
    }
    else{
        fatal("response type not implemented (handleResponse())");
    }

    return false;
}

template<class TagStore>
bool
StenstromProtocol<TagStore>::doDirectoryWriteback(MemReqPtr& req){

    if(req->cmd == DirWriteback){
        // Directory writeback of non-modified block
        // latency has allready been counted
        sendDirectoryMessage(req, 0);
        return true;
    }
    else if(req->cmd == DirOwnerWriteback){

        assert(req->presentFlags != NULL);

        // set our present flag to false
        req->presentFlags[cache->getCacheCPUid()] = false;

        int foundCount = 0;
        int newOwner = -1;
        for(int i=0;i<cache->cpuCount;i++){
            if(i != cache->getCacheCPUid() && req->presentFlags[i]){
                newOwner = i;
                foundCount++;
                break;
            }
        }
        assert(foundCount == 1);
        assert(newOwner >= 0);

        //update the request stats
        req->toProcessorID = newOwner;
        req->fromProcessorID = cache->getCacheCPUid();
        req->toInterfaceID = -1;
        req->owner = cache->getCacheCPUid();

        bool* tmpFlags = req->presentFlags;
        req->presentFlags = NULL;

        Addr tmpBlkAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);
        if(outstandingWritebackWSAddrs.find(tmpBlkAddr)
           == outstandingWritebackWSAddrs.end()){
            // there is no outstanding writeback to this address
            outstandingWritebackWSAddrs[tmpBlkAddr] = tmpFlags;
        }
        else{
            fatal("We are writing back the same block twice,"
                  " this is not nice...");
        }

        numOwnerWritebacks++;

        this->writeTraceLine(cacheName,
                       "Replacing owned block with sharers",
                       cache->getCacheCPUid(),
                       DirInvalid,
                       req->paddr,
                       cache->getBlockSize(),
                       tmpFlags);

        //forward the request to the new owner
        sendDirectoryMessage(req, 0);

        return true;
    }
    else if(req->cmd == DirSharerWriteback){

        // send it to the L2 cache, it will inform the owner
        req->toProcessorID = -1;
        req->fromProcessorID = cache->getCacheCPUid();
        req->toInterfaceID = -1;
        req->fromInterfaceID = -1;

        numSharerWritebacks++;

        this->writeTraceLine(cacheName,
                       "Replacing not owned block",
                       -1,
                       DirInvalid,
                       req->paddr,
                       cache->getBlockSize(),
                       req->presentFlags);

        //forward the request to the L2 cache
        sendDirectoryMessage(req, 0);

        return true;
    }
    return false;
}

template<class TagStore>
MemAccessResult
StenstromProtocol<TagStore>::handleL1DirectoryMiss(MemReqPtr& req){

    if(req->cmd == Soft_Prefetch){
        return MA_CACHE_MISS;
    }
    else if(req->cmd == DirRedirectRead){
        // Miss on a redirected read, we have written back the block, send NACK
        sendNACK(req,
                 cache->getHitLatency(),
                 req->fromProcessorID,
                 cache->getCacheCPUid());
        return MA_CACHE_MISS;
    }
    else if(req->cmd == DirOwnerWriteback){
        // Miss on a owner transfer request,
        // we have written back the block, send NACK
        sendNACK(req,
                 cache->getHitLatency(),
                 req->fromProcessorID,
                 cache->getCacheCPUid());
        return MA_CACHE_MISS;
    }
    else if(req->cmd == Read || req->cmd == Write){
        Addr tmpAddr = req->paddr & ~((Addr)cache->getBlockSize() - 1);
        if(outstandingWritebackWSAddrs.find(tmpAddr)
           != outstandingWritebackWSAddrs.end()){
            // this cache still has updated state for this cache
            // respond to request
            cache->respond(req, curTick + cache->getHitLatency());
            return MA_HIT;
        }
    }

    return BA_NO_RESULT;
}


/* Private helper methods */

template<class TagStore>
void
StenstromProtocol<TagStore>::setUpRedirectedRead(MemReqPtr& req,
                                                 int fromProcessorID,
                                                 int toProcessorID){
    req->oldCmd = req->cmd;
    req->cmd = DirRedirectRead;
    req->fromProcessorID = fromProcessorID;
    req->toProcessorID = toProcessorID;
    //must be updated if the req did not come from L2 just now
    req->owner = toProcessorID;
    req->toInterfaceID = -1;
}

template<class TagStore>
void
StenstromProtocol<TagStore>::setUpRedirectedReadReply(MemReqPtr& req,
                                                      int fromProcessorID,
                                                      int toProcessorID){
    req->toInterfaceID = -1;
    req->fromProcessorID = fromProcessorID;
    req->toProcessorID = toProcessorID;
    //only owners reply to redirected reads
    req->owner = fromProcessorID;
}

template<class TagStore>
void
StenstromProtocol<TagStore>::setUpOwnerTransferInL2(MemReqPtr& req,
                                                    int oldOwner,
                                                    int newOwner){
    req->cmd = DirOwnerTransfer;
    req->toProcessorID = oldOwner;
    req->owner =  newOwner; //blk->owner;
    req->fromProcessorID = -1;
    req->toInterfaceID = -1;
}

template<class TagStore>
void
StenstromProtocol<TagStore>::setUpACK(MemReqPtr& req, int toID, int fromID){
    req->toProcessorID = toID;
    req->fromProcessorID = fromID;
    req->toInterfaceID = -1;
    req->fromInterfaceID = -1;
    req->presentFlags = NULL;
    req->owner = -1;
    req->dirACK = true;
}

/* The rest of this file consists of template definitions */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Include config files
// Must be included first to determine which caches we want
#include "mem/config/cache.hh"
#include "mem/config/compression.hh"


// Tag Templates
#if defined(USE_CACHE_LRU)
#include "mem/cache/tags/lru.hh"
#endif

#if defined(USE_CACHE_FALRU)
#include "mem/cache/tags/fa_lru.hh"
#endif

#if defined(USE_CACHE_IIC)
#include "mem/cache/tags/iic.hh"
#endif

#if defined(USE_CACHE_SPLIT)
#include "mem/cache/tags/split.hh"
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
#include "mem/cache/tags/split_lifo.hh"
#endif

// Compression Templates
#include "base/compression/null_compression.hh"
#if defined(USE_LZSS_COMPRESSION)
#include "base/compression/lzss_compression.hh"
#endif

#if defined(USE_CACHE_FALRU)
    template class StenstromProtocol<CacheTags<FALRU,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class StenstromProtocol<CacheTags<FALRU,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_IIC)
    template class StenstromProtocol<CacheTags<IIC,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class StenstromProtocol<CacheTags<IIC,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_LRU)
    template class StenstromProtocol<CacheTags<LRU,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class StenstromProtocol<CacheTags<LRU,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT)
    template class StenstromProtocol<CacheTags<Split,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class StenstromProtocol<CacheTags<Split,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
    template class StenstromProtocol<CacheTags<SplitLIFO,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class StenstromProtocol<CacheTags<SplitLIFO,LZSSCompression> >;
#endif
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS
