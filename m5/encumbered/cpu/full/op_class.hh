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

#ifndef __ENCUMBERED_CPU_FULL_OP_CLASS_HH__
#define __ENCUMBERED_CPU_FULL_OP_CLASS_HH__

/**
 * @file
 * Definition of operation classes.
 */

/**
 * Instruction operation classes.  These classes are used for
 * assigning instructions to functional units.
 */
enum OpClass {
    No_OpClass = 0,	/* inst does not use a functional unit */
    IntAluOp,		/* integer ALU */
    IntMultOp,		/* integer multiplier */
    IntDivOp,		/* integer divider */
    FloatAddOp,		/* floating point adder/subtractor */
    FloatCmpOp,		/* floating point comparator */
    FloatCvtOp,		/* floating point<->integer converter */
    FloatMultOp,	/* floating point multiplier */
    FloatDivOp,		/* floating point divider */
    FloatSqrtOp,	/* floating point square root */
    MemReadOp,		/* memory read port */
    MemWriteOp,		/* memory write port */
    IprAccessOp,	/* Internal Processor Register read/write port */
    InstPrefetchOp,	/* instruction prefetch port (on I-cache) */
    Num_OpClasses	/* total functional unit classes */
};

/**
 * Array mapping OpClass enum values to strings.  Defined in fu_pool.cc.
 */
extern const char *opClassStrings[];

#endif // __ENCUMBERED_CPU_FULL_OP_CLASS_HH__
