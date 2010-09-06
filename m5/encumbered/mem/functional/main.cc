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
MainMemory::MainMemory(const string &n, int _maxMemMB, int _cpuID)
: FunctionalMemory(n), takeStats(false)
{

	cpuID = _cpuID;

	if(!IsPowerOf2(_maxMemMB)){
		fatal("Maximum memory consumption in functional memory must be a power of two");
	}

	memPageTabSize = _maxMemMB*128; // Since VMPageSize is 8192
	memPageTabSizeLog2 = FloorLog2(memPageTabSize);

	ptab = new entry*[memPageTabSize];

	for (int i = 0; i < memPageTabSize; ++i){
		ptab[i] = NULL;
	}

	break_address = 0;
	break_thread = 1;
	break_size = 4;

	buffer = new char[VMPageSize];
	::memset(buffer, 0, VMPageSize);
	currentFileEndPos = 0;

	//	stringstream filename;
	//	filename << "pagefile" << _cpuID << ".bin";
	//	pagefileName = filename.str();
	//	ofstream pagefile(pagefileName.c_str(), ios::binary);
	//	pagefile << "";
	//	pagefile.flush();
	//	pagefile.close();
}

MainMemory::~MainMemory()
{
	for (int i = 0; i < memPageTabSize; i++) {
		if (ptab[i]) {
			free(ptab[i]->page);
			free(ptab[i]);
		}
	}
	delete ptab;
}

void
MainMemory::startup()
{
	takeStats = true;
}

uint8_t*
MainMemory::writeEntryToFile(entry* entry){
	ofstream pagefile(pagefileName.c_str(), ios::binary | ios::app);
	assert(pagefile.is_open());

	uint8_t* pagePtr = entry->page;
	entry->page = NULL;
	entry->inMemory = false;
	entry->fileStartPosition = currentFileEndPos;

	assert(sizeof(char) == sizeof(uint8_t));
	pagefile.write((char*) pagePtr, VMPageSize);
	pagefile.flush();
	pagefile.close();
	currentFileEndPos += VMPageSize;

	::memset(pagePtr, 0, VMPageSize);

	// return the reusable page
	return pagePtr;

}

void
MainMemory::swapEntries(entry* curHead, entry* newHead){
	fstream pagefile(pagefileName.c_str(), ios::binary | ios::ate | ios::in | ios::out);
	assert(pagefile.is_open());

	assert(newHead->fileStartPosition != (fstream::pos_type) -1);

	pagefile.seekg(newHead->fileStartPosition);
	pagefile.read(buffer, VMPageSize);

	assert(sizeof(char) == sizeof(uint8_t));
	pagefile.seekp(newHead->fileStartPosition);
	pagefile.write((char*) curHead->page, VMPageSize);
	pagefile.flush();
	pagefile.close();

	newHead->page = curHead->page;
	for(int i=0;i<VMPageSize;i++){
		newHead->page[i] = (uint8_t) buffer[i];
	}

	curHead->inMemory = false;
	curHead->fileStartPosition = newHead->fileStartPosition;
	curHead->page = NULL;

	newHead->inMemory = true;
	newHead->fileStartPosition = -1;
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
	for (prev = NULL, pte = ptab[ptab_set(addr)]; pte != NULL; prev = pte, pte = pte->next) {
		if (pte->tag == ptab_tag(addr)) {

			assert(pte->inMemory);
			//			assert(!pte->inMemory);
			//			swapEntries(ptab[ptab_set(addr)], pte);
			//			assert(pte->inMemory && pte->page != NULL);

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

	//	if(!firstIsPage(addr) && ptab[ptab_set(addr)] != 0){
	//		page = writeEntryToFile(ptab[ptab_set(addr)]);
	//		assert(page != NULL);
	//
	//		pte = new entry(ptab_tag(addr), page);
	//		if (!pte) fatal("MainMemory::newpage: out of virtual memory (3)");
	//
	//		assert(ptab[ptab_set(addr)]->page == NULL);
	//		pte->next = ptab[ptab_set(addr)];
	//		ptab[ptab_set(addr)] = pte;
	//	}
	//	else{
	//	// see misc.c for details on the getcore() function
	//		assert(ptab[ptab_set(addr)] == NULL);
	//
	//		allocations++;
	//		page = new uint8_t[VMPageSize];
	//		if (!page) fatal("MainMemory::newpage: out of virtual memory");
	//
	//		::memset(page, 0, VMPageSize);
	//
	//		// generate a new PTE
	//		pte = new entry(ptab_tag(addr), page);
	//		if (!pte) fatal("MainMemory::newpage: out of virtual memory (2)");
	//
	//		ptab[ptab_set(addr)] = pte;
	//	}

	page = new uint8_t[VMPageSize];
	if (!page) fatal("MainMemory::newpage: out of virtual memory");

	::memset(page, 0, VMPageSize);

	pte = new entry(ptab_tag(addr), page);
	if (!pte) fatal("MainMemory::newpage: out of virtual memory (2)");

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

	accesses++;
	// first attempt to hit in first entry, otherwise call xlation fn
	if (firstIsPage(addr))
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
		misses++;
		return translate(addr);
	}
}

int
MainMemory::countPages(Addr vaddr, int64_t size, Addr new_vaddr){

	int pagesToMove = 0;
	for (; size > 0; size -= TheISA::VMPageSize, vaddr += TheISA::VMPageSize, new_vaddr += TheISA::VMPageSize){
		entry* e = ptab[ptab_set(vaddr)];
		bool found = false;
		while(e != NULL){
			if(e->tag == ptab_tag(vaddr)){
				assert(!found);
				pagesToMove++;
				found = true;
			}
			e = e->next;
		}
	}
	return pagesToMove;
}

int
MainMemory::getNumMemPages(){
	int mempages = 0;
	for(int i = 0; i< memPageTabSize; i++){
		entry* e = ptab[i];
		while(e != NULL){
			mempages++;
			e = e->next;
		}
	}
	return mempages;
}

void
MainMemory::printList(int set){
	cout << "Set " << set << " status @ " << curTick << ": ";
	entry* e = ptab[set];
	while(e != NULL){
		cout << e->tag << "(" << e << ") --> ";
		e = e->next;
	}
	cout << "NULL\n";
}

void
MainMemory::remap(Addr vaddr, int64_t size, Addr new_vaddr){
	assert(vaddr % TheISA::VMPageSize == 0);
	assert(new_vaddr % TheISA::VMPageSize == 0);

	DPRINTF(SyscallVerbose, "moving pages from vaddr %08p to %08p, size = %d\n", vaddr, new_vaddr, size);

	int prevMemPages = getNumMemPages();

	DPRINTF(SyscallVerbose, "There are %d pages in memory\n", prevMemPages);

	int pagesToMove = countPages(vaddr, size, new_vaddr);

	DPRINTF(SyscallVerbose, "%d pages are in the range to be moved\n", pagesToMove);

	int movedPages = 0;
	for (; size > 0; size -= TheISA::VMPageSize, vaddr += TheISA::VMPageSize, new_vaddr += TheISA::VMPageSize){
		entry* e = ptab[ptab_set(vaddr)];
		entry* prev = NULL;
		bool found = false;
		while(e != NULL){
			if(e->tag == ptab_tag(vaddr)){

				entry* elementToMove = e;

				if(prev == NULL){
					ptab[ptab_set(vaddr)] = e->next;
					prev = NULL;
				}
				else{
					prev->next = e->next;
				}
				e = e->next;

				assert(!found);
				found = true;
				elementToMove->tag = ptab_tag(new_vaddr);
				elementToMove->next = ptab[ptab_set(new_vaddr)];
				ptab[ptab_set(new_vaddr)] = elementToMove;

				movedPages++;
			}
			else{
				prev = e;
				e = e->next;
			}
		}
	}

	DPRINTF(SyscallVerbose, "moved %d pages to new addresses, should move %d pages\n", movedPages, pagesToMove);
	assert(pagesToMove == movedPages);


	int postMemPages = getNumMemPages();

	DPRINTF(SyscallVerbose, "%d pages in memory at start, %d pages at end\n", prevMemPages, postMemPages);
	assert(prevMemPages == postMemPages);

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

	accesses
	.name(name() + ".accesses")
	.desc("Number of page table accesses (simulator performance stat)")
	;

	misses
	.name(name() + ".misses")
	.desc("Number of page table misses (simulator performance stat)")
	;

	missRate
	.name(name() + ".miss_rate")
	.desc("Number page table miss rate (simulator performance stat)")
	;
	missRate = accesses / misses;

	allocations
	.name(name() + ".page_allocations")
	.desc("Number of memory pages allocated (simulator performance stat)")
	;

	allocationPercentage
	.name(name() + ".page_allocation_percentage")
	.desc("Precenttage of memory pages allocated out of the maximum count (simulator performance stat)")
	;

	allocationPercentage = allocations / memPageTabSize;
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

void
MainMemory::serialize(std::ostream &os){

#ifdef DO_SERIALIZE_VALIDATION
	dumpPages(true);
#endif

	SERIALIZE_SCALAR(break_address);
	SERIALIZE_SCALAR(break_thread);
	SERIALIZE_SCALAR(break_size);

	stringstream filenamestream;
	filenamestream << "pages" << cpuID << ".bin";
	string filename(filenamestream.str());
	SERIALIZE_SCALAR(filename);

	ofstream pagefile(filename.c_str(), ios::binary | ios::trunc);

	int indexCnt = 0;
	for(int i = 0; i< memPageTabSize; i++){
		if(ptab[i] != NULL){
			indexCnt++;
		}
	}

	writeEntry(&indexCnt, sizeof(int), pagefile);

	int writtenIndexes = 0;
	for(int i = 0; i< memPageTabSize; i++){
		if(ptab[i] != NULL){
			writtenIndexes++;

			writeEntry(&i, sizeof(int), pagefile);

			entry* firstPtr = ptab[i];
			entry* pte = firstPtr;

			int listCnt = 0;
			while(pte != NULL){
				listCnt++;
				pte = pte->next;
			}

			writeEntry(&listCnt, sizeof(int), pagefile);

			pte = firstPtr;
			while(pte != NULL){
				writeEntry(&(pte->tag), sizeof(Addr), pagefile);
				writeEntry(pte->page, VMPageSize * sizeof(uint8_t), pagefile);
				pte = pte->next;
			}
		}
	}

	assert(writtenIndexes == indexCnt);

	pagefile.close();
}

std::string
MainMemory::generateID(const char* prefix, int index, int linkedListNum){
	stringstream tmp;
	tmp << prefix << "-" << index << "-" << linkedListNum;
	return tmp.str();
}

void
MainMemory::unserialize(Checkpoint *cp, const std::string &section){
	UNSERIALIZE_SCALAR(break_address);
	UNSERIALIZE_SCALAR(break_thread);
	UNSERIALIZE_SCALAR(break_size);

	// remove any previously allocated pages
	for(int i = 0; i< memPageTabSize; i++){
		if(ptab[i] != NULL){

			entry* pte = ptab[i];
			while(pte != NULL){
				entry* delPTE = pte;
				if(delPTE->page != NULL) delete [] delPTE->page;
				pte = delPTE->next;
				delete delPTE;
			}

			ptab[i] = NULL;
		}
	}

	string filename;
	UNSERIALIZE_SCALAR(filename);

	ifstream pagefile(filename.c_str(),  ios::binary);
	if(!pagefile.is_open()) fatal("could not read file %s", filename.c_str());

	int numIndexes = *((int*) readEntry(sizeof(int), pagefile));
	int writtenIndexes = 0;

	while(writtenIndexes < numIndexes){
		int index = *((int*) readEntry(sizeof(int), pagefile));
		int entryCnt = *((int*) readEntry(sizeof(int), pagefile));
		std::vector<entry*> tmpLinkedList;
		if(entryCnt > MAX_ENTRY_COUNT){
			fatal("Got %d entries for linked list index, assuming file corruption", entryCnt);
		}
		for(int i=0;i<entryCnt;i++){
			Addr tag = * ((Addr*) readEntry(sizeof(Addr), pagefile));
			uint8_t* bufferedPage = (uint8_t*) readEntry(VMPageSize* sizeof(uint8_t), pagefile);

			uint8_t* page = new uint8_t[VMPageSize];
			if(!page) fatal("MainMemory::newpage: out of virtual memory");
			::memset(page, 0, VMPageSize);
			for(int j=0;j<VMPageSize;j++) page[j] = bufferedPage[j];

			entry* newEntry = new entry(tag, page);
			if(!newEntry) fatal("MainMemory::newpage: out of virtual memory");

			tmpLinkedList.push_back(newEntry);
		}
		assert(pagefile.good());

		if(!tmpLinkedList.empty()){

			// populate next pointers and insert into hash table
			for(int j=tmpLinkedList.size()-2; j>=0; j--){
				tmpLinkedList[j]->next = tmpLinkedList[j+1];
			}
			ptab[index] = tmpLinkedList[0];
		}

		writtenIndexes += 1;
	}

	pagefile.close();

#ifdef DO_SERIALIZE_VALIDATION
	dumpPages(false);
#endif
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
	return new MainMemory(getInstanceName(), 42, 1);
}

REGISTER_SIM_OBJECT("MainMemory", MainMemory)
