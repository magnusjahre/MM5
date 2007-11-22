/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

/*
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1997, 1998 by Todd M. Austin
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
 */

#ifndef __ENCUMBERED_CPU_FULL_CREATE_VECTOR_HH__
#define __ENCUMBERED_CPU_FULL_CREATE_VECTOR_HH__

#include <bitset>
#include <cassert>
#include <iostream>

#include "sim/root.hh"
#include "targetarch/isa_traits.hh"

/*
 * the create vector maps a logical register to a creator in the IQ (and
 * specific output operand) or the architected register file (if RS_link
 * is NULL)
 */

class ROBStation;

/* an entry in the create vector */
struct CVLink
{
    ROBStation *rs;	/* creator's reservation station */
    int odep_num;		/* specific output operand */
};

/* a NULL create vector entry */
/* static CVLink CVLINK_NULL = { NULL, 0 }; */
#define CVLINK_NULL (CVLink){NULL, 0}

class CreateVector
{
  public:

    // use correct-path or misspeculated create vector?
    std::bitset<TotalNumRegs> use_spec;

    // the create vector tracks the dynamic instruction that produces
    // each architectural register
    CVLink cv[TotalNumRegs];
    CVLink spec_cv[TotalNumRegs];

    // this array shadows the create vector and indicates when a
    // register was last created
    Tick timestamp[TotalNumRegs];
    Tick spec_timestamp[TotalNumRegs];

    void init_spec();

    void init();

    void set_entry(int reg_idx, ROBStation *creator_rs, int creator_odep_num,
		   bool spec_mode)
    {
	if (spec_mode) {
	    use_spec[reg_idx] = true;
	    spec_cv[reg_idx].rs = creator_rs;
	    spec_cv[reg_idx].odep_num = creator_odep_num;
	} else {
	    cv[reg_idx].rs = creator_rs;
	    cv[reg_idx].odep_num = creator_odep_num;
	}
    }


    CVLink entry(int reg_idx)
    {
	return (use_spec[reg_idx] ? spec_cv[reg_idx] : cv[reg_idx]);
    }

    Tick create_time(int reg_idx)
    {
	return (use_spec[reg_idx]
		? spec_timestamp[reg_idx] : timestamp[reg_idx]);
    }

    void clear_spec() {
	use_spec.reset();
    }
};

#endif // __ENCUMBERED_CPU_FULL_CREATE_VECTOR_HH__
