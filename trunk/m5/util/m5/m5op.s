/*
 * Copyright (c) 2003, 2005
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

#include <machine/asm.h>
#include <regdef.h>
		
#define m5_op 0x01

#define arm_func 0x00
#define quiesce_func 0x01
#define ivlb_func 0x10
#define ivle_func 0x11
#define exit_old_func 0x20 // deprectated!
#define exit_func 0x21
#define initparam_func 0x30
#define resetstats_func 0x40
#define dumpstats_func 0x41
#define dumprststats_func 0x42
#define ckpt_func 0x43
	
#define INST(op, ra, rb, func) \
	.long (((op) << 26) | ((ra) << 21) | ((rb) << 16) | (func))
	
#define	ARM(reg) INST(m5_op, reg, 0, arm_func)
#define QUIESCE() INST(m5_op, 0, 0, quiesce_func)
#define IVLB(reg) INST(m5_op, reg, 0, ivlb_func)
#define IVLE(reg) INST(m5_op, reg, 0, ivle_func)
#define M5EXIT(reg) INST(m5_op, reg, 0, exit_func)
#define INITPARAM(reg) INST(m5_op, reg, 0, initparam_func)
#define RESET_STATS(r1, r2) INST(m5_op, r1, r2, resetstats_func)
#define DUMP_STATS(r1, r2) INST(m5_op, r1, r2, dumpstats_func)
#define DUMPRST_STATS(r1, r2) INST(m5_op, r1, r2, dumprststats_func)
#define CHECKPOINT(r1, r2) INST(m5_op, r1, r2, ckpt_func)

	.set noreorder

	.align 4
LEAF(arm)
	ARM(16)
	RET
END(arm)

	.align 4
LEAF(quiesce)
	QUIESCE()
	RET
END(quiesce)

	.align 4
LEAF(ivlb)
	IVLB(16)
	RET
END(ivlb)

	.align 4
LEAF(ivle)
	IVLE(16)
	RET
END(ivle)

	.align 4
LEAF(m5exit)
	M5EXIT(16)
	RET
END(m5exit)

	.align 4
LEAF(initparam)
	INITPARAM(0)
	RET
END(initparam)

	.align 4
LEAF(reset_stats)
	RESET_STATS(16, 17)
	RET
END(reset_stats)

	.align 4
LEAF(dump_stats)
	DUMP_STATS(16, 17)
	RET
END(dump_stats)

	.align 4
LEAF(dumpreset_stats)
	DUMPRST_STATS(16, 17)
	RET
END(dumpreset_stats)

	.align 4
LEAF(checkpoint)
	CHECKPOINT(16, 17)
	RET
END(checkpoint)

