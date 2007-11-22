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

/* @file
 */

#ifndef __PHYSICAL_MEMORY_HH__
#define __PHYSICAL_MEMORY_HH__

#include "base/range.hh"
#include "mem/functional/functional.hh"

class MemoryController;

//
// Functional model for a contiguous block of physical memory. (i.e. RAM)
//
class PhysicalMemory : public FunctionalMemory
{
    friend class dma_access;
    friend class simple_disk;

  private:
    // prevent copying of a MainMemory object
    PhysicalMemory(const PhysicalMemory &specmem);
    const PhysicalMemory &operator=(const PhysicalMemory &specmem);

  protected:
    Addr base_addr;
    Addr pmem_size;
    uint8_t *pmem_addr;

  public:
    uint64_t size() { return pmem_size; }

  public:
    PhysicalMemory(const std::string &n, Range<Addr> range,
		   MemoryController *mmu, const std::string &fname);
    virtual ~PhysicalMemory();

  protected:
    // error handling for prot_* functions
    void prot_access_error(Addr addr, int size, const std::string &func);

  public:

    // Read/Write arbitrary amounts of data to simulated memory space
    virtual void prot_read(Addr addr, uint8_t *p, int size);
    virtual void prot_write(Addr addr, const uint8_t *p, int size);
    virtual void prot_memset(Addr addr, uint8_t val, int size);

    // fast back-door memory access for vtophys(), remote gdb, etc.
    uint64_t phys_read_qword(Addr addr) const;

  public:
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

    uint8_t *dma_addr(Addr addr, int count);
    void dma_read(uint8_t *data, Addr addr, int count);
    void dma_write(Addr addr, uint8_t *data, int count);

  public:
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

inline uint8_t *
PhysicalMemory::dma_addr(Addr addr, int count)
{
    mem_range_test(addr, addr + count, 0);
    if (addr + count > pmem_size)
	return NULL;

    return pmem_addr + addr;
}

inline void
PhysicalMemory::dma_read(uint8_t *data, Addr addr, int count)
{
    mem_range_test(addr, addr + count);
    assert(addr + count <= pmem_size && "reading beyond end of memory");

    memcpy(data, pmem_addr + addr, count);
}

inline void
PhysicalMemory::dma_write(Addr addr, uint8_t *data, int count)
{
    mem_range_test(addr, addr + count, data);
    assert(addr + count <= pmem_size && "writing beyond end of memory");

    memcpy(pmem_addr + addr, data, count);
}

inline uint64_t
PhysicalMemory::phys_read_qword(Addr addr) const
{
    mem_block_test(addr);
    if (addr + sizeof(uint64_t) > pmem_size)
	return 0;

    return *(uint64_t *)(pmem_addr + addr);
}


#endif //__PHYSICAL_MEMORY_HH__
