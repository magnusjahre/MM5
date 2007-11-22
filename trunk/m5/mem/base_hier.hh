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
 * Declaration a base class for all hierarchy objects.
 */

#ifndef __BASE_HIER_HH__
#define __BASE_HIER_HH__

#include "sim/sim_object.hh"
#include "mem/hier_params.hh"

/**
 * A base hierarchy class that contains a pointer to the universal hierarchy
 * parameters.
 */
class BaseHier : public SimObject
{
  protected:
    /** Pointer to the HierParams object for this hierarchy. */
    HierParams *params;
    
  public:
    /**
     * Create and initialize this object.
     * @param name The name of this memory.
     * @param _params The HierParams object for this hierarchy.
     */
    BaseHier(const std::string &name, HierParams *_params)
	: SimObject(name), params(_params)
    {
    }

    /**
     * True if the hierarchy is storing data internally.
     * @return The hierarchy is storing data.
     */
    bool doData()
    {
	return params->doData;
    }

    /**
     * True if the hierarchy is using events for timing.
     * @return The hierarchy is using events for timing.
     */
    bool doEvents()
    {
	return params->doEvents;
    }
};

#endif //__BASE_HIER_HH__
