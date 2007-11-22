/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
 * Declaration of a base replacement policy class.
 */

#ifndef __REPL_HH__
#define __REPL_HH__

#include <string>
#include <list>

#include "cpu/smt.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"


class IIC;

/**
 * A pure virtual base class that defines the interface of a replacement
 * policy.
 */
class Repl : public SimObject
{
 public:
    /** Pointer to the IIC using this policy. */
    IIC *iic;

    /**
     * Construct and initialize this polixy.
     * @param name The instance name of this policy.
     */
    Repl (const std::string &name)
	: SimObject(name)
    {
	iic = NULL;
    }

    /**
     * Set the back pointer to the IIC.
     * @param iic_ptr Pointer to the IIC.
     */
    void setIIC(IIC *iic_ptr)
    {
	iic = iic_ptr;
    }

    /**
     * Returns the tag pointer of the cache block to replace.
     * @return The tag to replace.
     */
    virtual unsigned long getRepl() = 0;

    /**
     * Return an array of N tag pointers to replace.
     * @param n The number of tag pointer to return.
     * @return An array of tag pointers to replace.
     */
    virtual unsigned long  *getNRepl(int n) = 0;

    /**
     * Update replacement data
     */
    virtual void doAdvance(std::list<unsigned long> &demoted) = 0;

     /**
     * Add a tag to the replacement policy and return a pointer to the
     * replacement entry.
     * @param tag_index The tag to add.
     * @return The replacement entry.
     */
    virtual void* add(unsigned long tag_index) = 0;

    /**
     * Register statistics.
     * @param name The name to prepend to statistic descriptions.
     */
    virtual void regStats(const std::string name) = 0;

    /**
     * Update the tag pointer to when the tag moves.
     * @param re The replacement entry of the tag.
     * @param old_index The old tag pointer.
     * @param new_index The new tag pointer.
     * @return 1 if successful, 0 otherwise.
     */
    virtual int fixTag(void *re, unsigned long old_index,
		       unsigned long new_index) = 0;

    /**
     * Remove this entry from the replacement policy.
     * @param re The replacement entry to remove
     */
    virtual void removeEntry(void *re) = 0;
};

#endif /* SMT_REPL_HH */
