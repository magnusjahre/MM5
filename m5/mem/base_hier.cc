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

/**
 * @file
 * SimObject instantiation of BaseHier.
 */

#include "sim/param.hh"
#include "mem/base_hier.hh"

#define MAX_MEM_ADDR ULL(0xffffffffffffffff)

Addr
BaseHier::relocateAddrForCPU(int cpuId, Addr originalAddr, int cpu_count){

	assert(cpuId != -1 && cpu_count != -1);

	// Add cpu-id to keep addresses aligned
	Addr cpuAddrBase = ((MAX_MEM_ADDR / cpu_count) * cpuId) + cpuId;

	Addr newAddr = cpuAddrBase + originalAddr;

	/* error checking */
	if(newAddr < cpuAddrBase){
		fatal("A memory address was moved out of this CPU's memory space at the low end");
	}
	if(newAddr >= ((MAX_MEM_ADDR / cpu_count) * (cpuId+1) + cpuId)){
		fatal("A memory address was moved out of this CPU's memory space at the high end");
	}

	return newAddr;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("BaseHier", BaseHier);

#endif
