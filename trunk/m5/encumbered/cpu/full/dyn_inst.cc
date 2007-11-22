/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include <string>

#include "base/cprintf.hh"
#include "base/trace.hh"
#include "cpu/exetrace.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/fetch.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "mem/mem_req.hh"

using namespace std;

#define NOHASH
#ifndef NOHASH

#include "base/hashmap.hh"

unsigned int MyHashFunc(const DynInst *addr)
{
  unsigned a = (unsigned)addr;
  unsigned hash = (((a >> 14) ^ ((a >> 2) & 0xffff))) & 0x7FFFFFFF;

  return hash;
}

typedef m5::hash_map<const DynInst *, const DynInst *, MyHashFunc> my_hash_t;
my_hash_t thishash;
#endif


int DynInst::instcount = 0;

//int break_inst = -1;

void
DynInst::squash()
{
    // already been squashed once... nothing to do
    if (squashed)
	return;

    if (DTRACE(Pipeline)) {
	string s;
	dump(s);
	DPRINTF(Pipeline, "Squash %s\n", s);
    }

    if (recover_inst) {
	xc->spec_mode--;
	recover_inst = false;
    }

    if (fault != No_Fault) {
	assert(cpu->fetch_fault_count[thread_number] > 0);
	cpu->fetch_fault_count[thread_number]--;
    }

    squashed = true;
}


void
DynInst::setCPSeq(InstSeqNum seq) {
    correctPathSeq = seq;
    if (trace_data) {
	trace_data->setCPSeq(seq);
    }
}


DynInst::DynInst(StaticInstPtr<TheISA> &_staticInst)
    : staticInst(_staticInst), trace_data(NULL)
{
    eff_addr = MemReq::inval_addr;

    spec_mem_write = false;

    recover_inst = false;
    spec_mode = false;
    btb_missed = false;
    squashed = false;
    serializing_inst = false;

#ifndef NOHASH
    thishash.insert(this, this);

    if (++instcount > 2048) {
	my_hash_t::iterator iter = thishash.begin();
	my_hash_t::iterator end = thishash.end();
	while (iter != end) {
	    cprintf("hash_t, addr = %#x\n"
		    "        valid = %#08x\n"
		    "        spec_mode = %d\n",
		    (void*)*iter, (*iter).second->valid,
		    (*iter).second->spec_mode);
	    ++iter;
	}
	//thishash.DumpData();
	panic("Too many insts.\n\tcycle = %n\n\tdeletecount = %d\n"
	      "\tinstcount = %d\n", curTick, deletecount, instcount);
    }
#endif
}

DynInst::~DynInst()
{
    if (spec_mem_write) {
	// Remove effects of this instruction from speculative memory
	xc->spec_mem->erase(eff_addr);
    }

#ifndef NOHASH
    my_hash_t::iterator i = thishash.find(this);
    thishash.Remove(i);
    --instcount;
#endif
}

FunctionalMemory *DynInst::getMemory(void)
{
    return (spec_mode ? xc->spec_mem : xc->mem);
}

IntReg *DynInst::getIntegerRegs(void)
{
    return (spec_mode ? xc->specIntRegFile : xc->regs.intRegFile);
}


void
DynInst::prefetch(Addr addr, unsigned flags)
{
    // This is the "functional" implementation of prefetch.  Not much
    // happens here since prefetches don't affect the architectural
    // state.

    // Generate a MemReq so we can translate the effective address.
    MemReqPtr req = new MemReq(addr, xc, 1, flags);
    req->asid = asid;

    // Prefetches never cause faults.
    fault = No_Fault;

    // note this is a local, not DynInst::fault
    Fault trans_fault = xc->translateDataReadReq(req);

    if (trans_fault == No_Fault && !(req->flags & UNCACHEABLE)) {
	// It's a valid address to cacheable space.  Record key MemReq
	// parameters so we can generate another one just like it for
	// the timing access without calling translate() again (which
	// might mess up the TLB).
	eff_addr = req->vaddr;
	phys_eff_addr = req->paddr;
	mem_req_flags = req->flags;
    } else {
	// Bogus address (invalid or uncacheable space).  Mark it by
	// setting the eff_addr to InvalidAddr.
	eff_addr = phys_eff_addr = MemReq::inval_addr;
    }

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#if !FULL_SYSTEM
    req->paddr = req->vaddr;
#endif
}

void
DynInst::writeHint(Addr addr, int size, unsigned flags)
{
    // Need to create a MemReq here so we can do a translation.  This
    // will casue a TLB miss trap if necessary... not sure whether
    // that's the best thing to do or not.  We don't really need the
    // MemReq otherwise, since wh64 has no functional effect.
    MemReqPtr req = new MemReq(addr, xc, size, flags);
    req->asid = asid;

    fault = xc->translateDataWriteReq(req);

    if (fault == No_Fault && !(req->flags & UNCACHEABLE)) {
	// Record key MemReq parameters so we can generate another one
	// just like it for the timing access without calling translate()
	// again (which might mess up the TLB).
	eff_addr = req->vaddr;
	phys_eff_addr = req->paddr;
	mem_req_flags = req->flags;
    } else {
	// ignore faults & accesses to uncacheable space... treat as no-op
	eff_addr = phys_eff_addr = MemReq::inval_addr;
    }

    store_size = size;
    store_data = 0;
}

/**
 * @todo Need to find a way to get the cache block size here.
 */
Fault
DynInst::copySrcTranslate(Addr src)
{
    static bool no_warn = true;
    int blk_size = 64;
    int offset = src & (blk_size - 1);

    // Make sure block doesn't span page
    if (no_warn && (src & (~8191)) == ((src + blk_size) & (~8191))) {
	warn("Copied block source spans pages.");
	no_warn = false;
    }

    MemReqPtr req = new MemReq(src & ~(blk_size - 1), xc, blk_size);
    req->asid = asid;

    // translate to physical address
    Fault fault = xc->translateDataReadReq(req);
    
    assert(fault != Alignment_Fault);

    if (fault == No_Fault) {
	xc->copySrcAddr = src;
	xc->copySrcPhysAddr = req->paddr + offset;
    } else {
	xc->copySrcAddr = 0;
	xc->copySrcPhysAddr = 0;
    }
    return fault;
}

/**
 * @todo Need to find a way to get the cache block size here.
 */
Fault
DynInst::copy(Addr dest)
{
    static bool no_warn = true;
    int blk_size = 64;
    uint8_t data[blk_size];
    int offset = dest & (blk_size - 1);

    // Make sure block doesn't span page
    if (no_warn && (dest & (~8191)) == ((dest + blk_size) & (~8191))) {
	no_warn = false;
	warn("Copied block destination spans pages. ");
    }

    FunctionalMemory *mem = (xc->misspeculating()) ? xc->spec_mem : xc->mem;
    MemReqPtr req = new MemReq(dest & ~(blk_size - 1), xc, blk_size);
    req->asid = asid;

    // translate to physical address
    Fault fault = xc->translateDataWriteReq(req);
    
    assert(fault != Alignment_Fault);

    if (fault == No_Fault) {
	Addr dest_addr = req->paddr + offset;
	// Need to read straight from memory since we have more than 8 bytes.
	req->paddr = xc->copySrcPhysAddr;
	mem->read(req, data);
	req->paddr = dest_addr;
	mem->write(req, data);
    }
    return fault;
}

void
DynInst::dump()
{
    cprintf("#%d T%d S%d : %#08x %s\n",
	    fetch_seq, thread_number, spec_mode,
	    PC, staticInst->disassemble(PC));
}

void DynInst::dump(std::string &outstring)
{
    outstring = csprintf("#%d T%d S%d : %#08x %s",
			 fetch_seq, thread_number, spec_mode,
			 PC, staticInst->disassemble(PC));
}
