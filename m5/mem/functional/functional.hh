/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#ifndef __FUNCTIONAL_MEMORY_HH__
#define __FUNCTIONAL_MEMORY_HH__

#include <string>

#include "base/range.hh"
#include "config/full_system.hh"
#include "mem/mem_cmd.hh"
#include "mem/mem_req.hh"
#include "sim/sim_object.hh"
#include "targetarch/byte_swap.hh"

#ifdef DEBUG
extern Addr break_addr;
#endif

/*
 * Base class for functional memory objects
 */
class FunctionalMemory : public SimObject
{
    friend class MemoryController;
    friend class SpeculativeMemory;

  protected:
    Addr break_address;
    int break_thread;
    int break_size;

  public:
    FunctionalMemory(const std::string &name);
    virtual ~FunctionalMemory();

    // Read/Write arbitrary amounts of data to simulated memory space
    virtual void prot_read(Addr addr, uint8_t *p, int size);
    virtual void prot_write(Addr addr, const uint8_t *p, int size);
    virtual void prot_memset(Addr addr, uint8_t val, int size);

  public:
    static Addr block_addr(Addr addr);
    virtual Fault read(MemReqPtr &req, uint8_t *data) = 0;
    virtual Fault write(MemReqPtr &req, const uint8_t *data) = 0;

    virtual Fault read(MemReqPtr &req, uint8_t &data);
    virtual Fault read(MemReqPtr &req, uint16_t &data);
    virtual Fault read(MemReqPtr &req, uint32_t &data);
    virtual Fault read(MemReqPtr &req, uint64_t &data);

    virtual Fault write(MemReqPtr &req, uint8_t data);
    virtual Fault write(MemReqPtr &req, uint16_t data);
    virtual Fault write(MemReqPtr &req, uint32_t data);
    virtual Fault write(MemReqPtr &req, uint64_t data);

    void access(MemCmd cmd, Addr addr, void *p, int nbytes);

    // write null-terminated string 'str' into memory at 'addr'
    Fault writeString(Addr addr, const char *str);
    // read null-terminated string from 'addr' into 'str'.
    Fault readString(std::string &str, Addr addr);

    virtual void remap(Addr vaddr, int64_t size, Addr new_vaddr){
    	fatal("remap not implemented");
    }

    virtual void clearMemory(Addr fromAddr, Addr toAddr){
    	fatal("clearMemory not implemented");
    }

#if FULL_SYSTEM
  public:
    virtual bool badaddr(Addr paddr) const { return false; }
#endif

  protected:
#ifdef DEBUG
    void mem_break() const;
    void mem_break(const void *data) const;

    void
    mem_addr_test(Addr addr) const
    {
	if (break_addr && addr == break_addr)
	    mem_break();
    }

    void
    mem_block_test(Addr addr) const
    {
	if (break_addr && (addr & ~(sizeof(uint64_t) - 1)) ==
	    (break_addr & ~(sizeof(uint64_t) - 1)))
	    mem_break();
    }

    void
    mem_range_test(Addr start, Addr end) const
    {
	if (break_addr && (break_addr >= start) && (break_addr < end))
	    mem_break();
    }

    void
    mem_addr_test(Addr addr, const void *data) const
    {
	if (break_addr && addr == break_addr)
	    mem_break(data);
    }

    void
    mem_block_test(Addr addr, const void *data) const
    {
	if (break_addr && (addr & ~(sizeof(uint64_t) - 1)) ==
	    (break_addr & ~(sizeof(uint64_t) - 1)))
	    mem_break(data);
    }

    void
    mem_range_test(Addr start, Addr end, const void *data) const
    {
	if (break_addr && (break_addr >= start) && (break_addr < end))
	    mem_break((const char *)data + break_addr - start);
    }

#else
    void mem_addr_test(Addr addr) const { }
    void mem_block_test(Addr addr) const { }
    void mem_range_test(Addr start, Addr end) const { }
    void mem_addr_test(Addr addr, const void *data) const { }
    void mem_block_test(Addr addr, const void *data) const { }
    void mem_range_test(Addr start, Addr end, const void *data) const {}
#endif
};

inline Fault
FunctionalMemory::read(MemReqPtr &req, uint8_t &data)
{ Fault ft; req->size = 1; ft = read(req, (uint8_t *)&data);
  data = gtoh(data); return ft; }

inline Fault
FunctionalMemory::read(MemReqPtr &req, uint16_t &data)
{ Fault ft; req->size = 2; ft = read(req, (uint8_t *)&data);
  data = gtoh(data); return ft; }

inline Fault
FunctionalMemory::read(MemReqPtr &req, uint32_t &data)
{ Fault ft; req->size = 4; ft = read(req, (uint8_t *)&data);
  data = gtoh(data); return ft; }

inline Fault
FunctionalMemory::read(MemReqPtr &req, uint64_t &data)
{ Fault ft; req->size = 8; ft = read(req, (uint8_t *)&data);
 data = gtoh(data); return ft; }

inline Fault
FunctionalMemory::write(MemReqPtr &req, uint8_t data)
{ req->size = 1; data = htog(data); return write(req, (uint8_t *)&data); }

inline Fault
FunctionalMemory::write(MemReqPtr &req, uint16_t data)
{ req->size = 2; data = htog(data); return write(req, (uint8_t *)&data); }

inline Fault
FunctionalMemory::write(MemReqPtr &req, uint32_t data)
{ req->size = 4; data = htog(data); return write(req, (uint8_t *)&data); }

inline Fault
FunctionalMemory::write(MemReqPtr &req, uint64_t data)
{ req->size = 8; data = htog(data); return write(req, (uint8_t *)&data); }

#endif // __FUNCTIONAL_MEMORY_HH__
