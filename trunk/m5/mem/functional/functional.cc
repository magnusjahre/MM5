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

#include <string>

#include "base/cprintf.hh"
#include "base/misc.hh"
#include "cpu/smt.hh"
#include "mem/functional/functional.hh"
#include "sim/debug.hh"
#include "sim/host.hh"
#include "sim/param.hh"
#include "sim/root.hh"

using namespace std;

FunctionalMemory::FunctionalMemory(const string &name)
    : SimObject(name)
{ }

FunctionalMemory::~FunctionalMemory()
{ }


DEFINE_SIM_OBJECT_CLASS_NAME("FunctionalMemory", FunctionalMemory)

void
FunctionalMemory::prot_read(Addr addr, uint8_t *p, int size)
{ panic("FunctionalMemory::prot_read unimplemented"); }

void
FunctionalMemory::prot_write(Addr addr, const uint8_t *p, int size)
{ panic("FunctionalMemory::prot_write unimplemented"); }

void
FunctionalMemory::prot_memset(Addr addr, uint8_t val, int size)
{ panic("FunctionalMemory::prot_memset unimplemented"); }


void
FunctionalMemory::access(MemCmd cmd, Addr addr, void *p, int nbytes)
{
    switch (cmd.toIndex()) {
      case Read:
	prot_read(addr, (uint8_t *)p, nbytes);
	return;

      case Write:
	prot_write(addr, (uint8_t *)p, nbytes);
	return;

      default:
	panic("unimplemented");
	return;
    }
}

Fault
FunctionalMemory::writeString(Addr addr, const char *str)
{
    uint8_t c;
    Fault fault;
    MemReqPtr req = new MemReq(addr, 0, 0);

    // EGH This is a hack to "translate" the address correctly
    // Only works in non FULL_SYSTEM
#if FULL_SYSTEM
    panic("Should not call this in full system mode.");
#endif
    req->paddr = req->vaddr;

    // copy until string terminator ('\0') is encountered
    do {
	c = *str++;
	fault = write(req, c);
	req->paddr++;
	req->vaddr++;
	if (fault != No_Fault)
	    return fault;
    } while (c);

    return No_Fault;
}


Fault
FunctionalMemory::readString(string &str, Addr addr)
{
    uint8_t c;
    Fault fault;
    MemReqPtr req = new MemReq(addr, 0, 0);

    // EGH This is a hack to "translate" the address correctly
    // Only works in non FULL_SYSTEM
#if FULL_SYSTEM
    panic("Should not call this in full system mode.");
#endif

   req->paddr = req->vaddr;

    // copy until string terminator ('\0') is encountered or n chars
    // have been copied
    do {
	fault = read(req, c);
	//EGH again, because of faxe translation, need to advance both of them.
	req->vaddr++;
	req->paddr++;
	if (fault != No_Fault)
	    return fault;
	str += c;
    } while (c);

    return No_Fault;
}

#ifdef DEBUG
uint64_t break_data64 = 0;
uint32_t break_data32 = 0;
uint16_t break_data16 = 0;
uint8_t break_data8 = 0;
Addr break_addr = 0;
bool break_physical = false;
bool break_reads = true;
int break_thread = 0;
FunctionalMemory *debug_mem = NULL;
Addr debug_addr = 0;
bool debug_physical = false;

void
print_data(const uint8_t *data)
{
    cprintf("%#016x\n"
	    "%#08x\n"
	    "%#04x\n"
	    "%#02x\n",
	    *(uint64_t *)data,
	    *(uint32_t *)data,
	    *(uint16_t *)data,
	    (int)*(uint8_t *)data);
}

void
debug_mem_read(Addr addr, bool phy)
{
    uint8_t buf[16];
    uint8_t *bufp = buf;
    if (debug_mem) {
#if FULL_SYSTEM
	MemReqPtr req = new MemReq(addr & ~7, NULL, 8, (phy ? PHYSICAL : 0));
	if (phy) req->paddr = req->vaddr & ULL(0xFFFFFFFFFF);
#else
	MemReqPtr req = new MemReq(addr & ~7, NULL, 8);
#endif
	debug_mem->read(req, bufp);
	print_data(bufp);
    }
}

void
debug_mem_read()
{ debug_mem_read(debug_addr, debug_physical); }

void
FunctionalMemory::mem_break() const
{
    if (!doDebugBreak || !break_reads)
	return;

    if (break_data8 || break_data16 || break_data32 || break_data64)
	return;

    debug_break();
}

void
FunctionalMemory::mem_break(const void *vdata) const
{
    if (!doDebugBreak)
	return;

    if (vdata) {
	const uint8_t *data = (const uint8_t *)vdata;
	if ((break_data64 != 0 && break_data64 != *(uint64_t *)data) ||
	    (break_data32 != 0 && break_data32 != *(uint32_t *)data) ||
	    (break_data16 != 0 && break_data16 != *(uint16_t *)data) ||
	    (break_data8  != 0 && break_data8  != *(uint8_t *)data) )
	    return;

	print_data(data);
    }

    debug_break();
}
#endif
