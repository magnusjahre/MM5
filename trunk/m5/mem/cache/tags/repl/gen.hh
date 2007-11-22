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
 * Declarations of generational replacement policy
 */

#ifndef ___GEN_HH__
#define __GEN_HH__

#include <list>

#include "base/statistics.hh"
#include "mem/cache/tags/repl/repl.hh"

/**
 * Generational Replacement entry.
 */
class GenReplEntry
{
  public:
    /** Valid flag, used to quickly invalidate bogus entries. */
    bool valid;
    /** The difference between this entry and the previous in the pool. */
    int delta;
    /** Pointer to the corresponding tag in the IIC. */
    unsigned long tag_ptr;
};

/**
 * Generational replacement pool
 */
class GenPool
{
  public:
    /** The time the last entry was added. */
    Tick newest;
    /** The time the oldest entry was added. */
    Tick oldest;
    /** List of the replacement entries in this pool. */
    std::list<GenReplEntry*> entries;

    /** The number of entries in this pool. */
    int size;

    /**
     * Simple constructor.
     */
    GenPool() {
	newest = 0;
	oldest = 0;
	size = 0;
    }

    /**
     * Add an entry to this pool.
     * @param re The entry to add.
     * @param now The current time.
     */
    void push(GenReplEntry *re, Tick now) {
	++size;
	if (!entries.empty()) {
	    re->delta = now - newest;
	    newest = now;
	} else {
	    re->delta = 0;
	    newest = oldest = now;
	}
	entries.push_back(re);
    }

    /**
     * Remove an entry from the pool.
     * @return The entry at the front of the list.
     */
    GenReplEntry* pop() {
	GenReplEntry *tmp = NULL;
	if (!entries.empty()) {
	    --size;
	    tmp = entries.front();
	    entries.pop_front();
	    oldest += tmp->delta;
	}
	return tmp;
    }
    
    /**
     * Return the entry at the front of the list.
     * @return the entry at the front of the list.
     */
    GenReplEntry* top() {
	return entries.front();
    }
    
    /**
     * Destructor.
     */
    ~GenPool() {
	while (!entries.empty()) {
	    GenReplEntry *tmp = entries.front();
	    entries.pop_front();
	    delete tmp;
	}
    }
};

/**
 * Generational replacement policy for use with the IIC.
 * @todo update to use STL and for efficiency
 */
class GenRepl : public Repl
{
  public:
    /** The array of pools. */
    GenPool *pools;
    /** The number of pools. */
    int num_pools;
    /** The amount of time to stay in the fresh pool. */
    int fresh_res;
    /** The amount of time to stay in the normal pools. */
    int pool_res;
    /** The maximum number of entries */
    int num_entries;
    /** The number of entries currently in the pools. */
    int num_pool_entries;
    /** The number of misses. Used as the internal time. */
    Tick misses;
    
    // Statistics
    
    /**
     * @addtogroup CacheStatistics
     * @{
     */
    /** The number of replacements from each pool. */
    Stats::Distribution<> repl_pool;
    /** The number of advances out of each pool. */
    Stats::Distribution<> advance_pool;
    /** The number of demotions from each pool. */
    Stats::Distribution<> demote_pool;
    /**
     * @}
     */
    
    /**
     * Constructs and initializes this replacement policy.
     * @param name The name of the policy.
     * @param num_pools The number of pools to use.
     * @param fresh_res The amount of time to wait in the fresh pool.
     * @param pool_res The amount of time to wait in the normal pools.
     */
    GenRepl(const std::string &name, int num_pools,
	    int fresh_res, int pool_res);

    /**
     * Destructor.
     */
    ~GenRepl();

    /**
     * Returns the tag pointer of the cache block to replace.
     * @return The tag to replace.
     */
    virtual unsigned long getRepl();

    /**
     * Return an array of N tag pointers to replace.
     * @param n The number of tag pointer to return.
     * @return An array of tag pointers to replace.
     */
    virtual unsigned long *getNRepl(int n);

    /**
     * Update replacement data
     */
    virtual void doAdvance(std::list<unsigned long> &demoted);

    /**
     * Add a tag to the replacement policy and return a pointer to the
     * replacement entry.
     * @param tag_index The tag to add.
     * @return The replacement entry.
     */
    virtual void* add(unsigned long tag_index);

    /**
     * Register statistics.
     * @param name The name to prepend to statistic descriptions.
     */
    virtual void regStats(const std::string name);

    /**
     * Update the tag pointer to when the tag moves.
     * @param re The replacement entry of the tag.
     * @param old_index The old tag pointer.
     * @param new_index The new tag pointer.
     * @return 1 if successful, 0 otherwise.
     */
    virtual int fixTag(void *re, unsigned long old_index,
		       unsigned long new_index);

    /**
     * Remove this entry from the replacement policy.
     * @param re The replacement entry to remove
     */
    virtual void removeEntry(void *re)
    {
	((GenReplEntry*)re)->valid = false;
    }
    
  protected:
    /**
     * Debug function to verify that there is only one repl entry per tag.
     * @param index The tag index to check.
     */
    bool findTagPtr(unsigned long index);
};

#endif /* __GEN_HH__ */
