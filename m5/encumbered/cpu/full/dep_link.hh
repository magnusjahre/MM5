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

#ifndef __ENCUMBERED_CPU_FULL_DEP_LINK_HH__
#define __ENCUMBERED_CPU_FULL_DEP_LINK_HH__

#include "base/fast_alloc.hh"
#include "cpu/inst_seq.hh"
#include "encumbered/cpu/full/iq/iq_station.hh"
#include "encumbered/cpu/full/iq/iqueue.hh"
#include "encumbered/cpu/full/rob_station.hh"

//
// dependency list
//

struct DepLink : public FastAlloc
{
    DepLink *next_dep;			// next entry in list
    DepLink *prev_dep;
    BaseIQ::iterator iq_consumer;
    ROBStation *rob_producer;		// ROB entry of producer
    InstTag tag;			// resv station sequence number
    int idep_num;			// input operand number
    int odep_num;

    unsigned iq_number;         	// IQ identifier

    // constructor: link producer_rs's prod_odep_num'th output to
    // consumer_rs's cons_idep_num'th input arg
    DepLink(ROBStation *_rob_producer, int prod_odep_num,
	     BaseIQ::iterator consumer,
	     int cons_idep_num, unsigned iq_num);

    DepLink *next() {
	return next_dep;
    }

    DepLink *prev() {
	return prev_dep;
    }

    BaseIQ::iterator consumer() {
	return iq_consumer;
    }

    ROBStation *producer() {
	return rob_producer;
    }

    void dump();
    void dump_list();

    // is link still valid?  (target mq_entry has not been squashed)
    bool valid() {
	return iq_consumer.notnull() && iq_consumer.data_ptr() &&
	    tag == iq_consumer->tag;
    }

    // virtual destructor for FastAlloc
    ~DepLink() {
	if (prev_dep) {
	    // Not the first link in the chain
	    prev_dep->next_dep = next_dep;
	} else {
	    // The first element... fix the ROB entry
	    rob_producer->odep_list[odep_num][iq_number] = next_dep;
	}

	if (next_dep)
	    next_dep->prev_dep = prev_dep;
    }
};


#endif // __ENCUMBERED_CPU_FULL_DEP_LINK_HH__
