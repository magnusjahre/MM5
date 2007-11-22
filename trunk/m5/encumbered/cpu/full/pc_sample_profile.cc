/*
 * Copyright (c) 2004, 2005
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

#include <iostream>

#include "base/loader/symtab.hh"
#include "base/output.hh"
#include "encumbered/cpu/full/cpu.hh"

FullCPU::PCSampleEvent::PCSampleEvent(int _interval, FullCPU *_cpu)
    : Event(&mainEventQueue), cpu(_cpu), interval(_interval)
{
}

void
FullCPU::PCSampleEvent::process()
{
    for (int i = 0; i < cpu->number_of_threads; ++i) {
	Addr pc = cpu->commitPC[i];
	std::string sym;

	std::string sym_str;
	Addr sym_addr;
	if (debugSymbolTable &&
	    debugSymbolTable->findNearestSymbol(pc, sym_str, sym_addr)) {
	    cpu->pcSampleHist[sym_addr]++;
	}
	else {
	    // record PC even if we don't have a symbol to avoid
	    // silently biasing the histogram
	    cpu->pcSampleHist[pc]++;
	}
    }

    // reschedule for next sample
    schedule(curTick + interval);
}


void
FullCPU::dumpPCSampleProfile()
{
    std::ostream *os = simout.create(csprintf("m5prof.%s", name()));

    m5::hash_map<Addr, Counter>::iterator i = pcSampleHist.begin();
    for (i = pcSampleHist.begin(); i != pcSampleHist.end(); ++i) {
	Addr pc = i->first;
	std::string sym_str;
	if (debugSymbolTable->findSymbol(pc, sym_str)
	    && sym_str != "") {
	    ccprintf(*os, "%s %d\n", sym_str, i->second);
	} else {
	    ccprintf(*os, "0x%x %d\n", pc, i->second);
	}
    }

    delete os;
}
