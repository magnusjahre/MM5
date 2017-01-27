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

#define MAX_ENTRY_COUNT 1000

// create a flat memory space and initialize memory system
MainMemory::MainMemory(const string &n, int _maxMemMB, int _cpuID, int _victimEntries)
: FunctionalMemory(n)
{

	cpuID = _cpuID;

	if(!IsPowerOf2(_maxMemMB)){
		fatal("Maximum memory consumption in functional memory must be a power of two");
	}

	memPageTabSize = _maxMemMB*128; // Since VMPageSize is 8192
	memPageTabSizeLog2 = FloorLog2(memPageTabSize);

	ptab = new entry[memPageTabSize];

	pblob = new uint8_t[VMPageSize*memPageTabSize];
	for (int i = 0; i < memPageTabSize; ++i){
		ptab[i].page = pblob + (i*VMPageSize);
		::memset(ptab[i].page, 0, VMPageSize);
	}

	break_address = 0;
	break_thread = 1;
	break_size = 4;

	curFileEnd = 0;
	stringstream tmp;
	tmp << "diskpages" << cpuID << ".bin";
	diskpages.open(tmp.str().c_str(), ios::binary | ios::trunc | ios::out | ios::in);

	if(_victimEntries < 1) fatal("You need to provide at least 1 victim buffer");
	blob = new uint8_t[VMPageSize*_victimEntries];
	for(int i=0;i<_victimEntries;i++){
		VictimEntry ve = VictimEntry();
		ve.page = blob+(i*VMPageSize);
		::memset(ve.page, 0, VMPageSize);
		victimBuffer.push_back(ve);
	}

	allocatedVictims = 0;
}

void
MainMemory::VictimEntry::reset(){
	pageAddress = INVALID_TAG;
	timestamp = 0;
	::memset(page, 0, VMPageSize);
}

MainMemory::~MainMemory()
{
	diskpages.close();
	delete blob;
	delete pblob;
	delete ptab;
}

// translate address to host page
uint8_t *
MainMemory::translate(Addr addr)
{
	// this method should only be called if the page mapping is wrong
	assert(ptab[ptab_set(addr)].tag != ptab_tag(addr));

	swapPages(page_addr(addr));
	return ptab[ptab_set(addr)].page;
}

void
MainMemory::insertVictim(Addr newAddr, Addr oldAddr){

	int useIndex = -1;
	if(allocatedVictims < victimBuffer.size()){ // insert into free slots
		useIndex = allocatedVictims;
		DPRINTF(FuncMem, "Free victim slots, allocating at %d (max %d)\n", useIndex, victimBuffer.size());
		allocatedVictims++;
	}
	else{
		assert(victimBuffer.size() > 0);
		int oldestID = 0;
		Tick oldestTick = victimBuffer[0].timestamp;
		for(int i=1;i<victimBuffer.size();i++){
			if(victimBuffer[i].timestamp < oldestTick){
				oldestTick = victimBuffer[i].timestamp;
				oldestID = i;
			}
		}

		DPRINTF(FuncMem, "Oldest page is %x (at %d), writing to disk\n", victimBuffer[oldestID].page, victimBuffer[oldestID].timestamp);

		diskWrites++;
		writeDiskEntry(victimBuffer[oldestID].pageAddress, victimBuffer[oldestID].page);
		useIndex = oldestID;
	}

	assert(useIndex != -1);
	uint8_t* tmp = victimBuffer[useIndex].page;
	victimBuffer[useIndex].pageAddress = page_addr(oldAddr);
	victimBuffer[useIndex].page = ptab[ptab_set(oldAddr)].page;
	victimBuffer[useIndex].timestamp = curTick;

	ptab[ptab_set(oldAddr)].page = tmp;
	ptab[ptab_set(newAddr)].tag = ptab_tag(newAddr);

	DPRINTF(FuncMem, "Page %x is now installed on position %d in victim buffer, page ptr %x\n",
			oldAddr,
			useIndex,
			(uint64_t) victimBuffer[useIndex].page);
}

void
MainMemory::swapPages(Addr newAddr){
	assert(newAddr % VMPageSize == 0);

	if (ptab[ptab_set(newAddr)].tag != INVALID_TAG) {

		DPRINTF(FuncMem, "Page %x needs to be removed to make space for 0x%x in set %d\n",
				page_addr(page_addr(ptab[ptab_set(newAddr)].tag, ptab_set(newAddr))),
				page_addr(newAddr),
				ptab_set(newAddr));

		Addr oldAddr = page_addr(ptab[ptab_set(newAddr)].tag, ptab_set(newAddr));
		assert(oldAddr % VMPageSize == 0);

		int newVictimID = checkVictimBuffer(newAddr);
		if(newVictimID >= 0){ // victim buffer hit, swap
			uint8_t* tmp = victimBuffer[newVictimID].page;

			assert(ptab_set(newAddr) == ptab_set(oldAddr));

			victimBuffer[newVictimID].pageAddress = page_addr(oldAddr);
			victimBuffer[newVictimID].page = ptab[ptab_set(oldAddr)].page;
			victimBuffer[newVictimID].timestamp = curTick;

			DPRINTF(FuncMem, "Swapped contents of victim buffer %d, old page 0x%x, new page 0x%x\n",
								newVictimID,
								(uint64_t) ptab[ptab_set(oldAddr)].page,
								(uint64_t) tmp);

			ptab[ptab_set(newAddr)].page = tmp;
			ptab[ptab_set(newAddr)].tag = ptab_tag(newAddr);

			return;
		}
		else{
			insertVictim(newAddr, oldAddr);
		}
	}
	else{
		DPRINTF(FuncMem, "Installing page 0x%x in empty set %d\n",
				page_addr(newAddr),
				ptab_set(newAddr));
	}

	ptab[ptab_set(newAddr)].tag = ptab_tag(newAddr);
	int newAddrIndex = findDiskEntry(newAddr);
	assert(ptab_set(newAddr) < memPageTabSize);
	if(newAddrIndex == -1){
		DPRINTF(FuncMem, "New page addr 0x%x does not exist on disk, resetting memory content for set %d\n",
				page_addr(newAddr),
				ptab_set(newAddr));

		::memset(ptab[ptab_set(newAddr)].page, 0, VMPageSize);
	}
	else{
		diskReads++;
		readDiskEntry(newAddrIndex);
	}
}

int
MainMemory::findDiskEntry(Addr newAddr){
	assert(newAddr % VMPageSize == 0);
	for(int i=0;i<diskEntries.size();i++){
		if(page_addr(newAddr) == diskEntries[i].pageAddress){
			return i;
		}
	}
	return -1;
}

int
MainMemory::checkVictimBuffer(Addr newAddr){
	assert(newAddr % VMPageSize == 0);
	for(int i=0;i<victimBuffer.size();i++){
		if(newAddr == victimBuffer[i].pageAddress) return i;
	}
	return -1;
}

void
MainMemory::writeDiskEntry(Addr oldAddr, uint8_t* page){
	assert(oldAddr % VMPageSize == 0);
	int oldAddrIndex = findDiskEntry(oldAddr);

	DiskEntry d;
	if(oldAddrIndex == -1){

		d.offset = curFileEnd * VMPageSize;
		d.pageAddress = page_addr(oldAddr);

		diskEntries.push_back(d);
		curFileEnd++;

		DPRINTF(FuncMem, "Page addr %x does not exist on disk, creating entry at offset %d, disk entries size %d\n",
				d.pageAddress,
				d.offset,
				diskEntries.size());
	}
	else{
		d = diskEntries[oldAddrIndex];
	}

	DPRINTF(FuncMem, "Writing page addr %x to disk at offset %d\n", d.pageAddress, d.offset);

	assert(diskpages.good());
	diskpages.seekp(d.offset);
	diskpages.write((char*) page, VMPageSize);
	assert(diskpages.good());
}


void
MainMemory::readDiskEntry(int diskEntryIndex){

	DPRINTF(FuncMem, "Reading page addr 0x%x from disk at offset %d to set %d\n",
			diskEntries[diskEntryIndex].pageAddress,
			diskEntries[diskEntryIndex].offset,
			ptab_set(diskEntries[diskEntryIndex].pageAddress));

	assert(diskpages.good());
	diskpages.seekp(diskEntries[diskEntryIndex].offset);
	diskpages.read((char*) ptab[ptab_set(diskEntries[diskEntryIndex].pageAddress)].page, VMPageSize);
	assert(diskpages.good());
}

void
MainMemory::flushPageTable(){

	DPRINTF(FuncMem, "---- Flushing page table to disk...\n");

	for(int i=0;i<victimBuffer.size();i++){
		if(victimBuffer[i].pageAddress != INVALID_TAG){
			assert(page_addr(victimBuffer[i].pageAddress) % VMPageSize == 0);
			writeDiskEntry(victimBuffer[i].pageAddress, victimBuffer[i].page);

			victimBuffer[i].pageAddress = INVALID_TAG;
			::memset(victimBuffer[i].page, 0, VMPageSize);
			victimBuffer[i].timestamp = 0;

			allocatedVictims--;
		}
	}
	assert(allocatedVictims == 0);

	for(int i=0;i<memPageTabSize;i++){
		if(ptab[i].tag != INVALID_TAG){
			assert(page_addr(ptab[i].tag, i) % VMPageSize == 0);
			Addr addr = page_addr(page_addr(ptab[i].tag, i));
			writeDiskEntry(addr, ptab[i].page);

			ptab[i].tag = INVALID_TAG;
			::memset(ptab[i].page, 0, VMPageSize);
		}

		assert(ptab[i].tag == INVALID_TAG);
	}
	DPRINTF(FuncMem, "---- Flushing done\n");
}

// locate host page for virtual address ADDR, returns NULL if unallocated
uint8_t *
MainMemory::page(Addr addr)
{
	DPRINTF(FuncMem, "CPU%d page access for addr 0x%x, page addr is %d, set %d\n",
			cpuID,
			addr,
			ptab_set(addr),
			ptab_tag(addr));

	accesses++;
	// first attempt to hit in first entry, otherwise call xlation fn
	if (ptab[ptab_set(addr)].tag == ptab_tag(addr))
	{
		return ptab[ptab_set(addr)].page;
	}
	else
	{
		// first level miss - call the translation helper function
		misses++;
		return translate(addr);
	}
}

void
MainMemory::remap(Addr vaddr, int64_t size, Addr new_vaddr){
	assert(vaddr % TheISA::VMPageSize == 0);
	assert(new_vaddr % TheISA::VMPageSize == 0);

	DPRINTF(SyscallVerbose, "moving pages from vaddr %08p to %08p, size = %d\n", vaddr, new_vaddr, size);
	DPRINTF(FuncMem, "---- Remaping pages from vaddr %x to %x, size = %d\n", vaddr, new_vaddr, size);

	flushPageTable();
	for(int i=0;i<memPageTabSize;i++) assert(ptab[i].tag == INVALID_TAG);

	for(int i=0;i<diskEntries.size();i++){
		if(diskEntries[i].pageAddress >= vaddr && diskEntries[i].pageAddress < vaddr + size){
			Addr offset = diskEntries[i].pageAddress - vaddr;
			assert(offset >= 0);

			DPRINTF(FuncMem, "Moving page %d to %d\n", diskEntries[i].pageAddress, new_vaddr + offset);

			diskEntries[i].pageAddress = new_vaddr + offset;
		}
	}
	DPRINTF(FuncMem, "---- Remap done!\n");
}

void
MainMemory::clearMemory(Addr fromAddr, Addr toAddr){

	DPRINTF(SyscallVerbose, "Clearing addresses from %#X to %#X\n", fromAddr, toAddr);

	assert(fromAddr < toAddr);
	for(Addr i=fromAddr;i<=toAddr;i++){
		page_set(i, 0, 1);
	}

	DPRINTF(SyscallVerbose, "Done clearing addresses\n");
}

void
MainMemory::prot_read(Addr addr, uint8_t *p, int size)
{
	DPRINTF(FuncMem, "Protected read of %d bytes for address %x\n", addr, size);

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
	DPRINTF(FuncMem, "Protected write of %d bytes for address %x\n", addr, size);

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
	DPRINTF(FuncMem, "Reading %d bytes for address %x\n", req->paddr, req->size);

	mem_block_test(req->paddr);
	Fault fault = page_check(req->paddr, req->size);

	if (fault == No_Fault)
		page_read(req->paddr, p, req->size);

	return fault;
}

Fault
MainMemory::write(MemReqPtr &req, const uint8_t *p)
{
	DPRINTF(FuncMem, "Writing %d bytes for address %x\n", req->paddr, req->size);

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

	accesses
	.name(name() + ".accesses")
	.desc("Number of page table accesses (simulator performance stat)")
	;

	misses
	.name(name() + ".misses")
	.desc("Number of page table misses (simulator performance stat)")
	;

	diskReads
	.name(name() + ".disk_reads")
	.desc("Number of pages read back from harddisk (simulator performance stat)")
	;

	diskWrites
	.name(name() + ".disk_writes")
	.desc("Number of pages written to the harddisk (simulator performance stat)")
	;

	missRate
	.name(name() + ".miss_rate")
	.desc("Number page table miss rate (simulator performance stat)")
	;
	missRate = misses / accesses;

	readRate
	.name(name() + ".read_rate")
	.desc("Fraction of disk reads over misses (simulator performance stat)")
	;
	readRate = diskReads / misses;

	writeRate
	.name(name() + ".write_rate")
	.desc("Fraction of disk writes over misses (simulator performance stat)")
	;
	writeRate = diskWrites / misses;
}

void
MainMemory::serialize(std::ostream &os){

#ifdef DO_SERIALIZE_VALIDATION
	dumpPages(true);
#endif

	SERIALIZE_SCALAR(break_address);
	SERIALIZE_SCALAR(break_thread);
	SERIALIZE_SCALAR(break_size);

	flushPageTable();

	stringstream filenamestream;
	filenamestream << "diskpages-cpt" << cpuID << ".bin";
	string diskpagefilename(filenamestream.str());
	SERIALIZE_SCALAR(diskpagefilename);

	stringstream filenamestream2;
	filenamestream2 << "pagefile-content" << cpuID << ".bin";
	string filename(filenamestream2.str());
	SERIALIZE_SCALAR(filename);

	ofstream pagefile(filename.c_str(), ios::binary | ios::trunc);
	int entries = diskEntries.size();
	writeEntry(&entries, sizeof(int), pagefile);
	for(int i=0;i<diskEntries.size();i++){
		assert(diskEntries[i].pageAddress != INVALID_TAG);
		Addr address =diskEntries[i].pageAddress;
		uint64_t offset = diskEntries[i].offset;

		writeEntry(&address, sizeof(Addr), pagefile);
		writeEntry(&offset, sizeof(uint64_t), pagefile);
	}

	pagefile.close();
}

std::string
MainMemory::generateID(const char* prefix, int index, int linkedListNum){
	stringstream tmp;
	tmp << prefix << "-" << index << "-" << linkedListNum;
	return tmp.str();
}

void
MainMemory::clearDiskpages(){
	DPRINTF(Restart, "Clearing diskEntries and closing diskpages...\n");
	curFileEnd = 0;
	diskEntries.clear();
	diskpages.close();
}

void
MainMemory::unserialize(Checkpoint *cp, const std::string &section){
	UNSERIALIZE_SCALAR(break_address);
	UNSERIALIZE_SCALAR(break_thread);
	UNSERIALIZE_SCALAR(break_size);

	// remove any previously allocated pages
	for(int i = 0; i< memPageTabSize; i++){
		if(ptab[i].tag != INVALID_TAG){
			ptab[i].tag = INVALID_TAG;
			::memset(ptab[i].page, 0, VMPageSize);
		}
	}

	// clear the victim buffer (neccessary on restarts)
	for(int i=0;i<victimBuffer.size();i++){
		victimBuffer[i].reset();
	}
	allocatedVictims = 0;

	// open checkpointed diskpages
	assert(diskEntries.empty());
	assert(!diskpages.is_open());

	string diskpagefilename;
	UNSERIALIZE_SCALAR(diskpagefilename);
	DPRINTF(Restart, "Unserializing disk page file %s\n", diskpagefilename);
	diskpages.open(diskpagefilename.c_str(), ios::binary | ios::out | ios::in);
	if(!diskpages.is_open()) fatal("could not read file %s", diskpagefilename.c_str());

	// read diskpage metadata
	string filename;
	UNSERIALIZE_SCALAR(filename);
	DPRINTF(Restart, "Unserializing page file %s\n", filename);
	ifstream pagefile(filename.c_str(),  ios::binary);
	if(!pagefile.is_open()) fatal("could not read file %s", filename.c_str());

	int numIndexes = *((int*) readEntry(sizeof(int), pagefile));

	while(curFileEnd < numIndexes){
		DiskEntry d;
		d.pageAddress = *((Addr*) readEntry(sizeof(Addr), pagefile));
		d.offset = *((uint64_t*) readEntry(sizeof(uint64_t), pagefile));

		diskEntries.push_back(d);

		curFileEnd += 1;
	}

	pagefile.close();

//	addr = 0x12a01de30;
//	data = 0;
//	page_read(addr, (uint8_t*) &data, sizeof(uint64_t));
//	DPRINTF(Restart, "Unserialize end: The value at address 0x%x is 0x%x\n", addr, data);
}

#ifdef DO_SERIALIZE_VALIDATION

void
MainMemory::dumpPages(bool onSerialize){

	string filename;
	if(onSerialize) filename = "pre-serialize-pages.txt";
	else filename = "post-unserialize-pages.txt";

	ofstream outfile(filename.c_str(), ios::out | ios::trunc);

	for(int i=0;i<memPageTabSize;i++){
		if(ptab[i] != NULL){
			int pos = 0;
			entry* current = ptab[i];
			while(current != NULL){
				dumpPage(i, pos, current, outfile);
				current = current->next;
			}
		}
	}
	outfile << "--- end of file ---\n";
	outfile.close();
}

void
MainMemory::dumpPage(int index, int pos, entry* current, ofstream& outfile){
	outfile << index << ";" << pos << ";" << current->tag << "\n";
	assert(current->page != NULL);
	outfile << uint8ToInt(current->page[0]);
	for(int i=1;i<VMPageSize;i++){
		outfile << ";" << uint8ToInt(current->page[i]);
	}
	outfile << "\n";
}

int
MainMemory::uint8ToInt(uint8_t val){
	return (int) val;
}

#endif

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MainMemory)

Param<bool> do_data;

END_DECLARE_SIM_OBJECT_PARAMS(MainMemory)

BEGIN_INIT_SIM_OBJECT_PARAMS(MainMemory)

INIT_PARAM_DFLT(do_data, "dummy param", false)

END_INIT_SIM_OBJECT_PARAMS(MainMemory)

CREATE_SIM_OBJECT(MainMemory)
{
	// Should not be created in this way, 42 is not a power of two and creation will fail
	return new MainMemory(getInstanceName(), 42, 1, 42);
}

REGISTER_SIM_OBJECT("MainMemory", MainMemory)
