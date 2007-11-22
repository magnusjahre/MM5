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

#ifndef __ENCUMBERED_CPU_FULL_SPEC_MEMORY_HH__
#define __ENCUMBERED_CPU_FULL_SPEC_MEMORY_HH__

#include <cassert>
#include <deque>

#include "base/hashmap.hh"
#include "mem/functional/functional.hh"

struct HashAddr {
    size_t operator()(Addr addr) const {
	return (addr >> 24) ^ (addr >> 16) ^ (addr >> 8) ^
	    (addr & (sizeof(Addr)-1));
    }
};

class SpeculativeMemory : public FunctionalMemory
{
  public:
    typedef uint64_t Block;

  protected:
    typedef std::deque<Block> data_queue;
    typedef m5::hash_map<Addr, data_queue, HashAddr> htable_t;
    typedef htable_t::iterator hash_iter_t;
    typedef htable_t::const_iterator hash_citer_t;
    htable_t table;

    FunctionalMemory *child;

    static Addr block_addr(Addr addr)
    {
	return addr & ~((Addr)(sizeof(Block) - 1));
    }

    bool read_block(Addr addr, Block &data);
    bool write_block(Addr addr, Block data);
    bool erase_block(Addr addr);
    Fault writeback_block(MemReqPtr &req);

  protected:
    // Read/Write arbitrary amounts of data to simulated memory space
    virtual void prot_read(Addr addr, uint8_t *p, int size);
    virtual void prot_write(Addr addr, const uint8_t *p, int size);
    virtual void prot_memset(Addr addr, uint8_t val, int size);

  public:
    SpeculativeMemory(const std::string &n, FunctionalMemory *c);
    virtual ~SpeculativeMemory();

  public:
    void writeback();
    void clear();

    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    virtual Fault read(MemReqPtr &req, uint8_t &data);
    virtual Fault read(MemReqPtr &req, uint16_t &data);
    virtual Fault read(MemReqPtr &req, uint32_t &data);
    virtual Fault read(MemReqPtr &req, uint64_t &data);

    virtual Fault write(MemReqPtr &req, uint8_t data);
    virtual Fault write(MemReqPtr &req, uint16_t data);
    virtual Fault write(MemReqPtr &req, uint32_t data);
    virtual Fault write(MemReqPtr &req, uint64_t data);

    bool erase(Addr addr);
};

inline bool
SpeculativeMemory::erase(Addr addr)
{
    // Remove the oldest entry from speculative memory for this
    // address.

    addr = block_addr(addr);
    mem_block_test(addr);

    hash_iter_t iter = table.find(addr);
    if (iter == table.end())
	return false;

    assert(!(iter->second.empty()));
    iter->second.pop_back();
    if (iter->second.empty()) {
	table.erase(iter);
    }

    return true;
}

inline void
SpeculativeMemory::clear()
{
    table.clear();
}

//
// Ideally these would be template functions, but you can't make
// template functions virtual.
//
#define SPEC_MEM_READ(TYPE)				\
inline Fault						\
SpeculativeMemory::read(MemReqPtr &req, TYPE &data)	\
{							\
    int offset = req->vaddr & (sizeof(Block) - 1);	\
    if (offset & (sizeof(TYPE) - 1))			\
	return Alignment_Fault;				\
							\
    Addr baddr = block_addr(req->vaddr);		\
							\
    Block block;					\
    if (!read_block(baddr, block))			\
	return child->read(req, data);			\
							\
    uint8_t *b = (uint8_t *)&block;			\
    data = *(TYPE *)(b + offset);			\
							\
    return No_Fault;					\
}

SPEC_MEM_READ(uint8_t)
SPEC_MEM_READ(uint16_t)
SPEC_MEM_READ(uint32_t)

// Specialize the quadword version for efficiency
inline Fault
SpeculativeMemory::read(MemReqPtr &req, uint64_t &data)
{
    if (req->vaddr & (sizeof(Block) - 1))
	return Alignment_Fault;

    if (!read_block(req->vaddr, data))
	return child->read(req, data);

    return No_Fault;
}

#define SPEC_MEM_WRITE(TYPE)				\
inline Fault						\
SpeculativeMemory::write(MemReqPtr &req, TYPE data)	\
{							\
    int offset = req->vaddr & (sizeof(Block) - 1);	\
    if (offset & (sizeof(TYPE) - 1))			\
	return Alignment_Fault;				\
							\
    Addr baddr = block_addr(req->vaddr);		\
							\
    Block block;					\
    if (!read_block(baddr, block)) {			\
	Fault fault;					\
	MemReqPtr new_req = new MemReq();		\
	new_req->vaddr = baddr;				\
	new_req->paddr = block_addr(req->paddr);	\
        new_req->size = sizeof(Block);			\
        fault = child->read(new_req, block);		\
        if (fault != No_Fault)				\
	    return fault;				\
    }							\
							\
    uint8_t *b = (uint8_t *)&block;			\
    *(TYPE *)(b + offset) = data;			\
    if (!write_block(baddr, block))			\
	return Machine_Check_Fault;			\
							\
    return No_Fault;					\
}

SPEC_MEM_WRITE(uint8_t)
SPEC_MEM_WRITE(uint16_t)
SPEC_MEM_WRITE(uint32_t)

// Specialize the quadword version for efficiency
inline Fault
SpeculativeMemory::write(MemReqPtr &req, uint64_t data)
{
    if (req->vaddr & (sizeof(Block) - 1))
	return Alignment_Fault;

    if (!write_block(req->vaddr, data))
	return Machine_Check_Fault;

    return No_Fault;
}

#undef SPEC_MEM_READ//(TYPE)
#undef SPEC_MEM_WRITE//(TYPE)

#endif // __ENCUMBERED_CPU_FULL_SPEC_MEMORY_HH__
