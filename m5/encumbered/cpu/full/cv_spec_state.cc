/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include <string>

#include "encumbered/cpu/full/cv_spec_state.hh"
#include "encumbered/cpu/full/rob_station.hh"

void
CreateVecSpecState::initialize(CreateVector *cv)
{
    true_cv = cv;

    //
    //  Copy the use_spec_cv bit vector
    //
    stored_spec_cv = cv->use_spec;

    //
    //  Copy the relevent CV links
    //
    num_entries = stored_spec_cv.count();
    cv_list->clear();

    cv_entry temp;
    unsigned n = 0;
    for (unsigned reg = 0; reg < TotalNumRegs && n != num_entries; ++reg) {
	if (stored_spec_cv[reg]) {
	    temp.link = cv->spec_cv[reg];
	    temp.reg = reg;

	    // the producer CAN be null!
	    temp.seq = temp.link.rs ? cv->spec_cv[reg].rs->seq : 0;
	    cv_list->add_tail(temp);

	    ++n;
	}
    }
}


void
CreateVecSpecState::restore()
{
    //
    //  Clear spec state prior to restoration
    //
    true_cv->init_spec();

    if (num_entries) {
	//
	//  Copy the original state back in...
	//
	for (cv_iterator j = cv_list->head(); j.notnull(); j = j.next()) {
	    if (j->link.rs) {
		//  Make sure that this producer hasn't committed...
		//  (we clear 'inst' when we remove the instruction
		//  from the ROB)
		if (j->seq == j->link.rs->seq && j->link.rs->inst != NULL) {
		    //  Also make sure that it hasn't written back yet...
		    if (!j->link.rs->completed)
			true_cv->spec_cv[j->reg] = j->link;
		}
	    } else {
		//  we need to keep the null producer
		true_cv->spec_cv[j->reg] = j->link;
	    }
	}

	true_cv->use_spec = stored_spec_cv;
    } else
	true_cv->use_spec.reset();
}

