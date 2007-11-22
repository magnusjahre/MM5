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

#ifndef __ALPHA_MEMORY_HH__
#define __ALPHA_MEMORY_HH__

#include <map>

#include "arch/alpha/isa_traits.hh"
#include "base/statistics.hh"
#include "mem/mem_req.hh"
#include "sim/sim_object.hh"

class ExecContext;

class AlphaTLB : public SimObject
{
  protected:
    typedef std::multimap<Addr, int> PageTable;
    PageTable lookupTable;	// Quick lookup into page table

    AlphaISA::PTE *table;	// the Page Table
    int size;			// TLB Size
    int nlu;			// not last used entry (for replacement)

    void nextnlu() { if (++nlu >= size) nlu = 0; }
    AlphaISA::PTE *lookup(Addr vpn, uint8_t asn) const;

  public:
    AlphaTLB(const std::string &name, int size);
    virtual ~AlphaTLB();

    int getsize() const { return size; }

    AlphaISA::PTE &index(bool advance = true);
    void insert(Addr vaddr, AlphaISA::PTE &pte);

    void flushAll();
    void flushProcesses();
    void flushAddr(Addr addr, uint8_t asn);

    // static helper functions... really EV5 VM traits
    static bool validVirtualAddress(Addr vaddr) {
	// unimplemented bits must be all 0 or all 1
	Addr unimplBits = vaddr & EV5::VAddrUnImplMask;
	return (unimplBits == 0) || (unimplBits == EV5::VAddrUnImplMask);
    }

    static void checkCacheability(MemReqPtr &req);

    // Checkpointing
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

class AlphaITB : public AlphaTLB
{
  protected:
    mutable Stats::Scalar<> hits;
    mutable Stats::Scalar<> misses;
    mutable Stats::Scalar<> acv;
    mutable Stats::Formula accesses;

  protected:
    void fault(Addr pc, ExecContext *xc) const;

  public:
    AlphaITB(const std::string &name, int size);
    virtual void regStats();

    Fault translate(MemReqPtr &req) const;
};

class AlphaDTB : public AlphaTLB
{
  protected:
    mutable Stats::Scalar<> read_hits;
    mutable Stats::Scalar<> read_misses;
    mutable Stats::Scalar<> read_acv;
    mutable Stats::Scalar<> read_accesses;
    mutable Stats::Scalar<> write_hits;
    mutable Stats::Scalar<> write_misses;
    mutable Stats::Scalar<> write_acv;
    mutable Stats::Scalar<> write_accesses;
    Stats::Formula hits;
    Stats::Formula misses;
    Stats::Formula acv;
    Stats::Formula accesses;

  protected:
    void fault(MemReqPtr &req, uint64_t flags) const;

  public:
    AlphaDTB(const std::string &name, int size);
    virtual void regStats();

    Fault translate(MemReqPtr &req, bool write) const;
};

#endif // __ALPHA_MEMORY_HH__
