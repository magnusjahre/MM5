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
 * Definitions of the MemReq builder functions.
 */

#include "mem/mem_req.hh"
#include "cpu/exec_context.hh"

MemReqPtr
buildWritebackReq(Addr addr, int asid, ExecContext *xc, int size, 
			  uint8_t *data, int compressed_size)
{
    MemReqPtr req = new MemReq();
    req->paddr = addr;
    req->asid = asid;
    req->size = size;
    req->data = new uint8_t[size];
    if (data) {
	memcpy(req->data, data, size);
    }
    /**
     * @todo Need to find a way to charge the writeback to the "correct"
     * thread.
     */
    req->xc = xc;
    if (xc) {
	req->thread_num = xc->thread_num;
    } else {
	// Assume thread_num is equal to asid.
	req->thread_num = asid;
    }

    req->cmd = Writeback;
    if (compressed_size < size) {
	req->flags |= COMPRESSED;
	req->size = compressed_size;
	req->actualSize = size;
    }
    return req;
}

MemReqPtr
buildDirectoryReq(Addr addr, int asid, ExecContext *xc, int size, 
                  uint8_t *data, MemCmdEnum directoryCommand)
{
    MemReqPtr req = new MemReq();
    req->paddr = addr;
    req->asid = asid;
    req->size = size;
    req->data = new uint8_t[size];
    if (data) {
        memcpy(req->data, data, size);
    }
    
    req->xc = xc;

    req->cmd = directoryCommand;

    return req;
}

MemReqPtr
buildReqCopy(const MemReqPtr & r, int cpuCount, MemCmdEnum newCommand)
{
    MemReqPtr req = new MemReq();
    
    // set values
    req->vaddr = r->vaddr;
    req->paddr = r->paddr;
    req->oldAddr = r->oldAddr;
    req->dest = r->dest;
    req->mshr = r->mshr;
    req->nic_req = r->nic_req;
    req->busId = r->busId;
    req->fromProcessorID = r->fromProcessorID;
    req->toProcessorID = r->toProcessorID;
    req->toInterfaceID = r->toInterfaceID;
    req->fromInterfaceID = r->fromInterfaceID;
    req->firstSendTime = r->firstSendTime;
    req->readOnlyCache = r->readOnlyCache;
    req->owner = r->owner;
    req->presentFlags = r->presentFlags;
    req->dirACK = r->dirACK;
    req->dirNACK = r->dirNACK;
    req->asid = r->asid;
    req->xc = r->xc;
    req->size = r->size;
    req->flags = r->flags;
    req->completionEvent = r->completionEvent;
    req->thread_num = r->thread_num;
    req->time = r->time;
    req->pc = r->pc;
    req->offset = r->offset;
    req->writeMiss = r->writeMiss;
    req->replacedByID = r->replacedByID;
    req->ownerWroteBack = r->ownerWroteBack;
    req->oldCmd = r->oldCmd;
    req->expectCompletionEvent = r->expectCompletionEvent;
    
    req->adaptiveMHASenderID = r->adaptiveMHASenderID;
    
    req->data = new uint8_t[r->size];
    if (r->data != NULL) {
        memcpy(req->data, r->data, r->size);
    }
    
    if(r->presentFlags != NULL){
        req->presentFlags = new bool[cpuCount];
        for(int i=0;i<cpuCount;i++){
            req->presentFlags[i] = r->presentFlags[i];
        }
    }

    req->cmd = newCommand;

    return req;
}

void
copyRequest(MemReqPtr & to, const MemReqPtr & from, int cpuCount)
{
    // set values
    to->vaddr = from->vaddr;
    to->paddr = from->paddr;
    to->oldAddr = from->oldAddr;
    to->dest = from->dest;
    to->mshr = from->mshr;
    to->nic_req = from->nic_req;
    to->busId = from->busId;
    to->fromProcessorID = from->fromProcessorID;
    to->toProcessorID = from->toProcessorID;
    to->toInterfaceID = from->toInterfaceID;
    to->fromInterfaceID = from->fromInterfaceID;
    to->firstSendTime = from->firstSendTime;
    to->readOnlyCache = from->readOnlyCache;
    to->owner = from->owner;
    to->presentFlags = from->presentFlags;
    to->dirACK = from->dirACK;
    to->dirNACK = from->dirNACK;
    to->asid = from->asid;
    to->xc = from->xc;
    to->size = from->size;
    to->flags = from->flags;
    to->completionEvent = from->completionEvent;
    to->thread_num = from->thread_num;
    to->time = from->time;
    to->pc = from->pc;
    to->offset = from->offset;
    to->cmd = from->cmd;
    to->writeMiss = from->writeMiss;
    to->replacedByID = from->replacedByID;
    to->ownerWroteBack = from->ownerWroteBack;
    to->oldCmd = from->oldCmd;
    to->expectCompletionEvent = from->expectCompletionEvent;
    
    to->adaptiveMHASenderID = from->adaptiveMHASenderID;
    
    to->data = new uint8_t[from->size];
    if (from->data != NULL) {
        memcpy(to->data, from->data, from->size);
    }
    
    if(from->presentFlags != NULL){
        to->presentFlags = new bool[cpuCount];
        for(int i=0;i<cpuCount;i++){
            to->presentFlags[i] = from->presentFlags[i];
        }
    }
}

