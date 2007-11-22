/*
 * memory.c - flat memory space routines
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use. 
 * 
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 * 
 *    This source code is distributed for non-commercial use only. 
 *    Please contact the maintainer for restrictions applying to 
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 *
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 * $Id: main.cc 1.58 05/06/05 02:52:51-04:00 rdreslin@zazzer.eecs.umich.edu $
 *
 * $Log: <Not implemented> $
 * Revision 1.1.1.1  1999/02/02 19:29:55  sraasch
 * Import source
 *
 * Revision 1.6  1998/08/27 15:38:28  taustin
 * implemented host interface description in host.h
 * added target interface support
 * memory module updated to support 64/32 bit address spaces on 64/32
 *       bit machines, now implemented with a dynamically optimized hashed
 *       page table
 * added support for quadword's
 * added fault support
 *
 * Revision 1.5  1997/03/11  01:15:25  taustin
 * updated copyright
 * mem_valid() added, indicates if an address is bogus, used by DLite!
 * long/int tweaks made for ALPHA target support
 *
 * Revision 1.4  1997/01/06  16:00:51  taustin
 * stat_reg calls now do not initialize stat variable values
 *
 * Revision 1.3  1996/12/27  15:52:46  taustin
 * updated comments
 * integrated support for options and stats packages
 *
 * Revision 1.1  1996/12/05  18:52:32  taustin
 * Initial revision
 *
 *
 */

#include <cassert>
#include <string>
#include <sstream>

#include "base/intmath.hh"
#include "base/statistics.hh"
#include "cpu/exec_context.hh"
#include "encumbered/mem/functional/main.hh"
#include "sim/builder.hh"
#include "sim/stats.hh"

using namespace std;

// create a flat memory space and initialize memory system
MainMemory::MainMemory(const string &n)
    : FunctionalMemory(n), takeStats(false)
{
    for (int i = 0; i < MEM_PTAB_SIZE; ++i)
	ptab[i] = NULL;

    break_address = 0;
    break_thread = 1;
    break_size = 4;
}

MainMemory::~MainMemory()
{
    for (int i = 0; i < MEM_PTAB_SIZE; i++) {
	if (ptab[i]) {
	    free(ptab[i]->page);
	    free(ptab[i]);
	}
    }
}

void
MainMemory::startup()
{
    takeStats = true;
}

// translate address to host page
uint8_t *
MainMemory::translate(Addr addr)
{
    entry *pte, *prev;

    if (takeStats) {
	// got here via a first level miss in the page tables
	ptab_misses++;
	ptab_accesses++;
    }

    // locate accessed PTE
    for (prev = NULL, pte = ptab[ptab_set(addr)];
	 pte != NULL; prev = pte, pte = pte->next) {
	if (pte->tag == ptab_tag(addr)) {
	    // move this PTE to head of the bucket list
	    if (prev) {
		prev->next = pte->next;
		pte->next = ptab[ptab_set(addr)];
		ptab[ptab_set(addr)] = pte;
	    }
	    return pte->page;
	}
    }

    // no translation found, return NULL
    return NULL;
}

// allocate a memory page
uint8_t *
MainMemory::newpage(Addr addr)
{
    uint8_t *page;
    entry *pte;

    // see misc.c for details on the getcore() function
    page = new uint8_t[VMPageSize];
    if (!page)
	fatal("MainMemory::newpage: out of virtual memory");

    ::memset(page, 0, VMPageSize);

    // generate a new PTE
    pte = new entry;
    if (!pte)
	fatal("MainMemory::newpage: out of virtual memory (2)");

    pte->tag = ptab_tag(addr);
    pte->page = page;

    // insert PTE into inverted hash table
    pte->next = ptab[ptab_set(addr)];
    ptab[ptab_set(addr)] = pte;

    if (takeStats) {
	// one more page allocated
	page_count++;
    }

    return page;
}

// locate host page for virtual address ADDR, returns NULL if unallocated
uint8_t *
MainMemory::page(Addr addr)
{
    // first attempt to hit in first entry, otherwise call xlation fn
    if (ptab[ptab_set(addr)] && ptab[ptab_set(addr)]->tag == ptab_tag(addr))
    {
	if (takeStats) {
	    // hit - return the page address on host
	    ptab_accesses++;
	}
	return ptab[ptab_set(addr)]->page;
    }
    else
    {
	// first level miss - call the translation helper function
	return translate(addr);
    }
}

void
MainMemory::prot_read(Addr addr, uint8_t *p, int size)
{
    int count = min((Addr)size,
		    ((addr - 1) & ~(VMPageSize - 1)) + VMPageSize - addr);

    page_read(addr, p, count);
    addr += count;
    p += count;
    size -= count;

    while (size >= VMPageSize) {
	page_read(addr, p, VMPageSize);
	addr += VMPageSize;
	p += VMPageSize;
	size -= VMPageSize;
    }

    if (size > 0)
	page_read(addr, p, size);
}

void
MainMemory::prot_write(Addr addr, const uint8_t *p, int size)
{
    int count = min((Addr)size,
		    ((addr - 1) & ~(VMPageSize - 1)) + VMPageSize - addr);

    page_write(addr, p, count);
    addr += count;
    p += count;
    size -= count;

    while (size >= VMPageSize) {
	page_write(addr, p, VMPageSize);
	addr += VMPageSize;
	p += VMPageSize;
	size -= VMPageSize;
    }

    if (size > 0)
	page_write(addr, p, size);
}

void
MainMemory::prot_memset(Addr addr, uint8_t val, int size)
{
    int count = min((Addr)size,
		    ((addr - 1) & ~(VMPageSize - 1)) + VMPageSize - addr);

    page_set(addr, val, count);
    addr += count;
    size -= count;

    while (size >= VMPageSize) {
	page_set(addr, val, VMPageSize);
	addr += VMPageSize;
	size -= VMPageSize;
    }

    if (size > 0)
	page_set(addr, val, size);
}

// generic memory access function, it's safe because alignments and
// permissions are checked, handles any natural transfer sizes; note,
// faults out if request is not a power-of-two or if it is larger then
// VMPageSize

Fault
MainMemory::page_check(Addr addr, int size) const
{
    if (size < sizeof(uint64_t)) {
	if (!IsPowerOf2(size)) {
	    panic("Invalid request size!\n");
	    return Machine_Check_Fault;
	}

	if ((size - 1) & addr)
	    return Alignment_Fault;
    }
    else {
	if ((addr & (VMPageSize - 1)) + size > VMPageSize) {
	    panic("Invalid request size!\n");
	    return Machine_Check_Fault;
	}

	if ((sizeof(uint64_t) - 1) & addr)
	    return Alignment_Fault;
    }

    return No_Fault;
}

Fault
MainMemory::read(MemReqPtr &req, uint8_t *p)
{
    mem_block_test(req->paddr);
    Fault fault = page_check(req->paddr, req->size);

    if (fault == No_Fault)
	page_read(req->paddr, p, req->size);

    return fault;
}

Fault
MainMemory::write(MemReqPtr &req, const uint8_t *p)
{
    mem_block_test(req->paddr);
    Fault fault = page_check(req->paddr, req->size);

    if (fault == No_Fault)
	page_write(req->paddr, p, req->size);

    return fault;
}


// Add load-locked to tracking list.  Should only be called if the
// operation is a load and the LOCKED flag is set.
void
MainMemory::trackLoadLocked(MemReqPtr &req)
{
    // set execution context's lock_addr and lock_flag.  Note that
    // this is done in virtual_access.hh in full-system
    // mode... eventually we should settle on a common spot for this
    // to happen.
    req->xc->regs.miscRegs.lock_addr = req->paddr;
    req->xc->regs.miscRegs.lock_flag = true;

    // first we check if we already have a locked addr for this
    // xc.  Since each xc only gets one, we just update the
    // existing record with the new address.
    list<LockedAddr>::iterator i;

    for (i = lockedAddrList.begin(); i != lockedAddrList.end(); ++i) {
	if (i->matchesContext(req->xc)) {
	    i->setAddr(req->paddr);
	    return;
	}
    }

    // no record for this xc: need to allocate a new one
    lockedAddrList.push_front(LockedAddr(req->paddr, req->xc));
}


// Called on *writes* only... both regular stores and
// store-conditional operations.  Check for conventional stores which
// conflict with locked addresses, and for success/failure of store
// conditionals.
bool
MainMemory::checkLockedAddrList(MemReqPtr &req)
{
    // Initialize return value.  Non-conditional stores always
    // succeed.  Assume conditional stores will fail until proven
    // otherwise.
    bool success = !(req->flags & LOCKED);

    // Iterate over list.
    list<LockedAddr>::iterator i = lockedAddrList.begin();

    while (i != lockedAddrList.end()) {

	if (i->matchesAddr(req->paddr)) {
	    // we have a matching address

	    if ((req->flags & LOCKED) && i->matchesContext(req->xc)) {
		// it's a store conditional, and as far as the memory
		// system can tell, the requesting context's lock is
		// still valid.  We still need to check the context's
		// lock flag, since this can get cleared by non-memory
		// events such as interrupts.  If it's still set, we
		// declare the store conditional successful.

		req->result = success = req->xc->regs.miscRegs.lock_flag;

		// Check for an execution context that has had lots of
		// SC failures (with no intervening successes), and
		// print a deadlock warning to the user.
		if (success) {
		    // success! clear the counter
		    req->xc->storeCondFailures = 0;
		}
		else {
		    if (++req->xc->storeCondFailures % 1000000 == 0) {
			// unfortunately the xc has no back
			// pointer to its CPU, so we just print out
			// the xc pointer value to distinguish
			// different contexts
			cerr << "Warning: " << req->xc->storeCondFailures
			     << " consecutive store conditional failures "
			     << "on execution context " << req->xc
			     << endl;
		    }
		}
	    }

	    // The execution context's copy of the lock address should
	    // not be changed without the memory system finding out
	    // about it (via a load_locked),
	    ExecContext *lockingContext = i->getContext();

	    assert(i->matchesAddr(lockingContext->regs.miscRegs.lock_addr));

	    // Clear the lock flag.
	    lockingContext->regs.miscRegs.lock_flag = false;

	    // Get rid of our record of this lock and advance to next
	    i = lockedAddrList.erase(i);
	}
	else {
	    // no match: advance to next record
	    ++i;
	}
    }

    return success;
}

void MainMemory::regStats()
{
    using namespace Stats;

    page_count
	.name(name() + ".page_count")
	.desc("total number of pages allocated")
	;

    ptab_misses
	.name(name() + ".ptab_misses")
	.desc("total first level page table misses")
	;

    ptab_accesses
	.name(name() + ".ptab_accesses")
	.desc("total page table accessess")
	;
}

void MainMemory::regFormulas()
{
    using namespace Stats;

    page_mem
	.name(name() + ".page_mem")
	.desc("total size of memory pages allocated")
	;
    page_mem = page_count * VMPageSize / 1024;

    ptab_miss_rate
	.name(name() + ".ptab_miss_rate")
	.desc("first level page table miss rate")
	.precision(4)
	;
    ptab_miss_rate = ptab_misses / ptab_accesses;
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MainMemory)

    Param<bool> do_data;

END_DECLARE_SIM_OBJECT_PARAMS(MainMemory)

BEGIN_INIT_SIM_OBJECT_PARAMS(MainMemory)

    INIT_PARAM_DFLT(do_data, "dummy param", false)

END_INIT_SIM_OBJECT_PARAMS(MainMemory)

CREATE_SIM_OBJECT(MainMemory)
{
    return new MainMemory(getInstanceName());
}

REGISTER_SIM_OBJECT("MainMemory", MainMemory)
