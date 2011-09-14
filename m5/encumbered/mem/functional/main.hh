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
 * $Id: main.hh 1.53 05/06/05 02:52:51-04:00 rdreslin@zazzer.eecs.umich.edu $
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

/* @file
 */

#ifndef __MAIN_MEMORY_HH__
#define __MAIN_MEMORY_HH__

#include <fstream>

#include "mem/functional/functional.hh"

#include "base/statistics.hh"
#include "sim/stats.hh"

//#define DO_SERIALIZE_VALIDATION

// number of entries in page translation hash table (must be power-of-two)
//#define MEM_PTAB_SIZE		(32*1024)
//#define MEM_LOG_PTAB_SIZE	15

#define INVALID_TAG 0xFFFFFFFFFFFFFFFF

/*
 * Model of infinite virtual memory for a standalone application
 */
class MainMemory : public FunctionalMemory
{
public:
	friend class simple_disk;

private:
	// prevent copying of a MainMemory object
	MainMemory(const MainMemory &specmem);
	const MainMemory &operator=(const MainMemory &specmem);

	int memPageTabSize;
	int memPageTabSizeLog2;
	int cpuID;

	uint8_t* pblob;
	uint8_t* blob;

	/*
	 *
	 */
	class LockedAddr {
		// on alpha, minimum LL/SC granularity is 16 bytes, so lower
		// bits need to masked off.
		static const Addr Addr_Mask = 0xf;

		Addr addr;			// locked address
		ExecContext *xc;	// locking context

	public:
		// mask off unneeded address bits
		static Addr maskAddr(Addr _addr) { return _addr & ~Addr_Mask; }

		// change locked address
		void setAddr(Addr _addr) { addr = maskAddr(_addr); }

		// check for matching reference address
		bool matchesAddr(Addr addr2) { return (addr == maskAddr(addr2)); }

		// check for matching execution context
		bool matchesContext(ExecContext *xc2)
		{ return (xc == xc2); }

		// return pointer to execution context
		ExecContext *getContext() { return xc; }

		LockedAddr(Addr _addr, ExecContext *_xc)
		: addr(maskAddr(_addr)), xc(_xc)
		{
		}
	};

	std::list<LockedAddr> lockedAddrList;

	// helper function for checkLockedAddrs(): we really want to
	// inline a quick check for an empty locked addr list (hopefully
	// the common case), and do the full list search (if necessary) in
	// this out-of-line function
	bool checkLockedAddrList(MemReqPtr &req);

protected:

	// page table entry
	struct entry
	{
		Addr tag;			// virtual page number tag
		uint8_t *page;		// page pointer

		entry()
		: tag(INVALID_TAG), page(NULL){

		}
	};

	struct DiskEntry{
		Addr pageAddress;
		int offset;

		DiskEntry()
		: pageAddress(INVALID_TAG), offset(-1){

		}
	};

	struct VictimEntry{
		Addr pageAddress;
		Tick timestamp;
		uint8_t *page;

		VictimEntry()
		: pageAddress(INVALID_TAG), timestamp(0), page(NULL){

		}
	};

	entry* ptab;	// inverted page table
	std::vector<DiskEntry> diskEntries;
	std::fstream diskpages;
	int curFileEnd;

	int allocatedVictims;
	std::vector<VictimEntry> victimBuffer;

	void swapPages(Addr newAddr);
	int findDiskEntry(Addr newAddr);

	int checkVictimBuffer(Addr newAddr);
	void insertVictim(Addr newAddr, Addr oldAddr);

	void writeDiskEntry(Addr oldAddr, uint8_t* page);
	void readDiskEntry(int diskEntryIndex);

	void flushPageTable();

#ifdef DO_SERIALIZE_VALIDATION
	void dumpPages(bool onSerialize);
	void dumpPage(int index, int pos, entry* page, std::ofstream& outfile);
	int uint8ToInt(uint8_t val);
#endif

	Stats::Scalar<> accesses;
	Stats::Scalar<> misses;
	Stats::Formula missRate;

	Addr offset(Addr addr);
	Addr ptab_set(Addr addr);
	Addr ptab_tag(Addr addr);
	Addr page_addr(Addr addr);
	Addr page_addr(Addr tag, Addr set);

	uint8_t *page(Addr addr);
	uint8_t *translate(Addr addr);

	// Check for access faults to page data
	Fault page_check(Addr addr, int size) const;

	// Read/Write arbitrary amounts of data within a page
	Fault page_read(Addr addr, uint8_t *p, int size);
	Fault page_write(Addr addr, const uint8_t *p, int size);
	Fault page_set(Addr addr, uint8_t val, int size);

	// Speed up access for certain data access sizes
	template <class T> Fault page_read(Addr addr, T &data);
	template <class T> Fault page_write(Addr addr, T data);

	// Record the address of a load-locked operation so that we can
	// clear the execution context's lock flag if a matching store is
	// performed
	void trackLoadLocked(MemReqPtr &req);

	// Compare a store address with any locked addresses so we can
	// clear the lock flag appropriately.  Return value set to 'false'
	// if store operation should be suppressed (because it was a
	// conditional store and the address was no longer locked by the
	// requesting execution context), 'true' otherwise.  Note that
	// this method must be called on *all* stores since even
	// non-conditional stores must clear any matching lock addresses.
	bool checkLockedAddrs(MemReqPtr &req) {
		if (lockedAddrList.empty()) {
			// no locked addrs: nothing to check, store_conditional fails
			return !(req->flags & LOCKED);
		} else {
			// iterate over list...
			return checkLockedAddrList(req);
		}
	}

	std::string generateID(const char* prefix, int index, int linkedListNum);

public:
	MainMemory(const std::string &n, int _maxMemMB, int _cpuID, int _victimEntries);
	virtual ~MainMemory();

	// Read/Write arbitrary amounts of data to simulated memory space
	virtual void prot_read(Addr addr, uint8_t *p, int size);
	virtual void prot_write(Addr addr, const uint8_t *p, int size);
	virtual void prot_memset(Addr addr, uint8_t val, int size);

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

	virtual void remap(Addr vaddr, int64_t size, Addr new_vaddr);
	virtual void clearMemory(Addr fromAddr, Addr toAddr);

public:
	virtual void regStats();

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

// compute address of access within a host page
inline Addr
MainMemory::offset(Addr addr)
{ return addr & (VMPageSize - 1); }

inline Addr
MainMemory::page_addr(Addr addr)
{ return addr & ~(VMPageSize-1); }

// compute page table set
inline Addr
MainMemory::ptab_set(Addr addr)
{ return (addr >> LogVMPageSize) & (memPageTabSize - 1); }

// compute page table tag
inline Addr
MainMemory::ptab_tag(Addr addr)
{ return addr >> (LogVMPageSize + memPageTabSizeLog2); }

inline Addr
MainMemory::page_addr(Addr tag, Addr set)
{
	Addr tmp = 0;
	tmp = tag << (LogVMPageSize + memPageTabSizeLog2);
	tmp = tmp | (set << LogVMPageSize);
	return tmp;
}

inline Fault
MainMemory::page_read(Addr addr, uint8_t *data, int size)
{
	uint8_t *p = page(addr);
	::memcpy(data, p + offset(addr), size);

	mem_addr_test(addr);
	return No_Fault;
}

inline Fault
MainMemory::page_write(Addr addr, const uint8_t *data, int size)
{
	uint8_t *p = page(addr);

	::memcpy(p + offset(addr), data, size);

	mem_addr_test(addr, data);
	return No_Fault;
}

inline Fault
MainMemory::page_set(Addr addr, uint8_t val, int size)
{
	uint8_t *p = page(addr);

	::memset(p + offset(addr), val, size);

	mem_addr_test(addr);
	return No_Fault;
}

template <class T>
inline Fault
MainMemory::page_read(Addr addr, T &data)
{
	uint8_t *p = page(addr);
	data = *((T *)(p + offset(addr)));

	mem_addr_test(addr);
	return No_Fault;
}

template <class T>
inline Fault
MainMemory::page_write(Addr addr, T data)
{
	*((T *)(page(addr) + offset(addr))) = data;

	mem_addr_test(addr);
	return No_Fault;
}

inline Fault
MainMemory::read(MemReqPtr &req, uint8_t &data)
{
	mem_block_test(req->paddr);
	if (req->flags & LOCKED)
		trackLoadLocked(req);
	page_read(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::read(MemReqPtr &req, uint16_t &data)
{
	mem_block_test(req->paddr);
	if (req->paddr & (sizeof(uint16_t) - 1)) return Alignment_Fault;
	if (req->flags & LOCKED)
		trackLoadLocked(req);
	page_read(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::read(MemReqPtr &req, uint32_t &data)
{
	mem_block_test(req->paddr);
	if (req->paddr & (sizeof(uint32_t) - 1)) return Alignment_Fault;
	if (req->flags & LOCKED)
		trackLoadLocked(req);
	page_read(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::read(MemReqPtr &req, uint64_t &data)
{
	mem_block_test(req->paddr);
	if (req->paddr & (sizeof(uint64_t) - 1)) return Alignment_Fault;
	if (req->flags & LOCKED)
		trackLoadLocked(req);
	page_read(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::write(MemReqPtr &req, uint8_t data)
{
	mem_block_test(req->paddr, &data);
	if (checkLockedAddrs(req))
		page_write(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::write(MemReqPtr &req, uint16_t data)
{
	mem_block_test(req->paddr, &data);
	if (req->paddr & (sizeof(uint16_t) - 1)) return Alignment_Fault;
	if (checkLockedAddrs(req))
		page_write(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::write(MemReqPtr &req, uint32_t data)
{
	mem_block_test(req->paddr, &data);
	if (req->paddr & (sizeof(uint32_t) - 1)) return Alignment_Fault;
	if (checkLockedAddrs(req))
		page_write(req->paddr, data);
	return No_Fault;
}

inline Fault
MainMemory::write(MemReqPtr &req, uint64_t data)
{
	mem_block_test(req->paddr, &data);
	if (req->paddr & (sizeof(uint64_t) - 1)) return Alignment_Fault;
	if (checkLockedAddrs(req))
		page_write(req->paddr, data);
	return No_Fault;
}

#endif // __MAIN_MEMORY_HH__
