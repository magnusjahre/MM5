/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

#include <algorithm>
#include <string>


#include "base/intmath.hh"
#include "base/misc.hh"
#include "encumbered/cpu/full/spec_memory.hh"
#include "mem/functional/functional.hh"

using namespace std;

SpeculativeMemory::SpeculativeMemory(const string &n, FunctionalMemory *c)
    : FunctionalMemory(n), child(c)
{}

SpeculativeMemory::~SpeculativeMemory()
{}

bool
SpeculativeMemory::read_block(Addr addr, Block &data)
{
    if (addr != block_addr(addr))
	panic("read_block can only be called with a block aligned address!");

    mem_block_test(addr);

    hash_citer_t iter = table.find(addr);
    if (iter == table.end())
	return false;

    data = iter->second.front();

    return true;
}

bool
SpeculativeMemory::write_block(Addr addr, Block data)
{
    if (addr != block_addr(addr))
	panic("read_block can only be called with a block aligned address!");

    mem_block_test(addr);

    table[addr].push_front(data);
    return true;
}

bool
SpeculativeMemory::erase_block(Addr addr)
{
    if (addr != block_addr(addr))
	panic("erase_block can only be called with a block aligned address!");

    mem_block_test(addr);

    hash_iter_t iter = table.find(addr);
    if (iter == table.end()) {
	panic("Trying to erase a block that isn't there!");
	return false;
    }

    iter->second.pop_back();
    if (iter->second.empty())
	table.erase(iter);

    return true;
}

Fault
SpeculativeMemory::writeback_block(MemReqPtr &req)
{
    if (req->vaddr != block_addr(req->vaddr))
	panic("writeback_block must called with a block aligned address!");

    mem_block_test(req->vaddr);

    hash_iter_t iter = table.find(req->vaddr);
    if (iter == table.end()) {
	panic("Trying to writeback a block that isn't there!");
	return Machine_Check_Fault;
    }
    Block data = iter->second.back();

    Fault fault = child->write(req, data);
    if (fault != No_Fault)
	panic("If we've faulted on a writeback, there is either a bug, or we\n"
	      "need some sort of coherence between threads.  (Or something\n"
	      "likethat)");

    iter->second.pop_back();
    if (iter->second.empty())
	table.erase(iter);

    return No_Fault;
}

Fault
SpeculativeMemory::read(MemReqPtr &req, uint8_t *data)
{
    switch(req->size) {
      case sizeof(uint8_t):
	return read(req, *(uint8_t *)data);
      case sizeof(uint16_t):
	return read(req, *(uint16_t *)data);
      case sizeof(uint32_t):
	return read(req, *(uint32_t *)data);
      case sizeof(uint64_t):
	return read(req, *(uint64_t *)data);
      default:
	return Machine_Check_Fault;
    }
}

Fault
SpeculativeMemory::write(MemReqPtr &req, const uint8_t *data)
{
    switch(req->size) {
      case sizeof(uint8_t):
	return write(req, *(uint8_t *)data);
      case sizeof(uint16_t):
	return write(req, *(uint16_t *)data);
      case sizeof(uint32_t):
	return write(req, *(uint32_t *)data);
      case sizeof(uint64_t):
	return write(req, *(uint64_t *)data);
      default:
	return Machine_Check_Fault;
    }
}

void
SpeculativeMemory::writeback()
{
    while (!table.empty()) {
	hash_iter_t iter = table.begin();

	Addr addr = iter->first;
	Block data = iter->second.front();
	mem_block_test(addr);

	// We're setting the thread to zero here.  The thread is only currently
	// used on a store conditional though.
	MemReqPtr req = new MemReq(addr, 0, sizeof(Block));
	child->write(req, data);

	table.erase(iter);
    }

    clear();
}

void
SpeculativeMemory::prot_read(Addr addr, uint8_t *p, int size)
{
#if 0
    child->prot_read(addr, p, size);
#else
    int offset = addr & (sizeof(uint64_t) - 1);
    if (offset) {
	int len = min((int)(sizeof(uint64_t) - offset), size);
	hash_citer_t iter =
	    table.find(addr & ~((uint64_t)sizeof(uint64_t) - 1));
	if (iter == table.end())
	    child->prot_read(addr, p, len);
	else {
	    uint64_t data = iter->second.front();
	    memcpy(p, ((char *)&data) + offset, len);
	}
	addr += len;
	p += len;
	size -= len;
    }

    if (size == 0)
	return;

    if (addr & (sizeof(uint64_t) - 1))
	fatal("Invalid address!");

    while (size > sizeof(uint64_t)) {
	hash_citer_t iter = table.find(addr);
	if (iter == table.end())
	    child->prot_read(addr, p, sizeof(uint64_t));
	else {
	    uint64_t data = iter->second.front();
	    memcpy(p, &data, sizeof(uint64_t));
	}
	addr += sizeof(uint64_t);
	p += sizeof(uint64_t);
	size -= sizeof(uint64_t);
    }

    hash_citer_t iter = table.find(addr);
    if (iter == table.end())
	child->prot_read(addr, p, size);
    else {
	uint64_t data = iter->second.front();
	memcpy(p, &data, size);
    }
#endif
}

void
SpeculativeMemory::prot_write(Addr addr, const uint8_t *p, int size)
{ child->prot_write(addr, p, size); }

void
SpeculativeMemory::prot_memset(Addr addr, uint8_t val, int size)
{ child->prot_memset(addr, val, size); }
