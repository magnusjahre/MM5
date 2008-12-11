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
 * Declaration of a memory hierarchy parameter object.
 */

#ifndef __HIER_PARAMS_HH__
#define __HIER_PARAMS_HH__

#include "sim/sim_object.hh"

/**
 * A object to store parameters universal to a given hierarchy.
 */
class HierParams : public SimObject
{
    
    friend class BaseHier;
    
  public:
    /** Store data in the hierarchy memories. */
    const bool doData;
    /** Use events for timing on the bus. */
    const bool doEvents;
    
    const int hpCpuCount;

  public:
    /**
     * Construct a parameter object.
     * @param name The name of this parameter ogject.
     * @param do_data Store data in the hierarchy.
     * @param do_events Use events for timing.
     */
    HierParams(const std::string &name, bool do_data, bool do_events, int _hpCpuCount);
};

/** Default HierParams Object. */
extern HierParams defaultHierParams;

#endif //__HIER_PARAMS_HH__
