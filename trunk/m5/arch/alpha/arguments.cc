/*
 * Copyright (c) 2003, 2004, 2005
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

#include "arch/alpha/arguments.hh"
#include "arch/alpha/vtophys.hh"
#include "cpu/exec_context.hh"
#include "mem/functional/physical.hh"

AlphaArguments::Data::~Data()
{
    while (!data.empty()) {
	delete [] data.front();
	data.pop_front();
    }
}

char *
AlphaArguments::Data::alloc(size_t size)
{
    char *buf = new char[size];
    data.push_back(buf);
    return buf;
}

uint64_t
AlphaArguments::getArg(bool fp)
{
    if (number < 6) {
	if (fp)
	    return xc->regs.floatRegFile.q[16 + number];
	else
	    return xc->regs.intRegFile[16 + number];
    } else {
	Addr sp = xc->regs.intRegFile[30];
	Addr paddr = vtophys(xc, sp + (number-6) * sizeof(uint64_t));
	return xc->physmem->phys_read_qword(paddr);
    }
}

