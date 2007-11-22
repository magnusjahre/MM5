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
 * Definitions of the Generational replacement policy.
 */

#include <sstream>
#include <string>

#include "base/misc.hh"
#include "mem/cache/tags/iic.hh"
#include "mem/cache/tags/repl/gen.hh"
#include "sim/builder.hh"
#include "sim/host.hh"

using namespace std;

GenRepl::GenRepl(const string &_name,
		 int _num_pools,
		 int _fresh_res,
		 int _pool_res) // fix this, should be set by cache
    : Repl(_name)
{
    num_pools = _num_pools;
    fresh_res = _fresh_res;
    pool_res = _pool_res;
    num_entries = 0;
    num_pool_entries = 0;
    misses = 0;
    pools = new GenPool[num_pools+1];
}

GenRepl::~GenRepl()
{
    delete [] pools;
}

unsigned long
GenRepl::getRepl()
{
    unsigned long tmp;
    GenReplEntry *re;
    int i;
    int num_seen = 0;
    if (!(num_pool_entries>0)) {
	fatal("No blks available to replace");
    }
    num_entries--;
    num_pool_entries--;
    for (i = 0; i < num_pools; i++) {
	while ((re = pools[i].pop())) {
	    num_seen++;
	    // Remove invalidated entries
	    if (!re->valid) {
		delete re;
		continue;
	    }
	    if (iic->clearRef(re->tag_ptr)) {
		pools[(((i+1)== num_pools)? i :i+1)].push(re, misses);
	    }
	    else {
		tmp = re->tag_ptr;
		delete re;

		repl_pool.sample(i);

		return tmp;
	    }
	}
    }
    fatal("No replacement found");
    return 0xffffffff;
}

unsigned long *
GenRepl::getNRepl(int n)
{
    unsigned long *tmp;
    GenReplEntry *re;
    int i;
    if (!(num_pool_entries>(n-1))) {
	fatal("Not enough blks available to replace");
    }
    num_entries -= n;
    num_pool_entries -= n;
    tmp = new unsigned long[n]; /* array of cache_blk pointers */
    int blk_index = 0;
    for (i = 0; i < num_pools && blk_index < n; i++) {
	while (blk_index < n && (re = pools[i].pop())) {
	    // Remove invalidated entries
	    if (!re->valid) {
		delete re;
		continue;
	    }
	    if (iic->clearRef(re->tag_ptr)) {
		pools[(((i+1)== num_pools)? i :i+1)].push(re, misses);
	    }
	    else {
		tmp[blk_index] = re->tag_ptr;
		blk_index++;
		delete re;
		repl_pool.sample(i);
	    }
	}
    }
    if (blk_index >= n)
	return tmp;
    /* search the fresh pool */

    fatal("No N  replacements found");
    return NULL;
}

void
GenRepl::doAdvance(std::list<unsigned long> &demoted)
{
    int i;
    int num_seen = 0;
    GenReplEntry *re;
    misses++;
    for (i=0; i<num_pools; i++) {
	while (misses-pools[i].oldest > pool_res && (re = pools[i].pop())!=NULL) {
	    if (iic->clearRef(re->tag_ptr)) {
		pools[(((i+1)== num_pools)? i :i+1)].push(re, misses);
		/** @todo Not really demoted, but use it for now. */
		demoted.push_back(re->tag_ptr);
		advance_pool.sample(i);
	    }
	    else {
		pools[(((i-1)<0)?i:i-1)].push(re, misses);
		demoted.push_back(re->tag_ptr);
		demote_pool.sample(i);
	    }
	}
	num_seen += pools[i].size;
    }
    while (misses-pools[num_pools].oldest > fresh_res
	  && (re = pools[num_pools].pop())!=NULL) {
	num_pool_entries++;
	if (iic->clearRef(re->tag_ptr)) {
	    pools[num_pools/2].push(re, misses);
	    /** @todo Not really demoted, but use it for now. */
	    demoted.push_back(re->tag_ptr);
	    advance_pool.sample(num_pools);
	}
	else {
	    pools[num_pools/2-1].push(re, misses);
	    demoted.push_back(re->tag_ptr);
	    demote_pool.sample(num_pools);
	}
    }
}

void*
GenRepl::add(unsigned long tag_index)
{
    GenReplEntry *re = new GenReplEntry;
    re->tag_ptr = tag_index;
    re->valid = true;
    pools[num_pools].push(re, misses);
    num_entries++;
    return (void*)re;
}

void
GenRepl::regStats(const string name)
{
    using namespace Stats;

    /** GEN statistics */
    repl_pool
	.init(0, 16, 1)
	.name(name + ".repl_pool_dist")
	.desc("Dist. of Repl. across pools")
	.flags(pdf)
	;

    advance_pool
	.init(0, 16, 1)
	.name(name + ".advance_pool_dist")
	.desc("Dist. of Repl. across pools")
	.flags(pdf)
	;

    demote_pool
	.init(0, 16, 1)
	.name(name + ".demote_pool_dist")
	.desc("Dist. of Repl. across pools")
	.flags(pdf)
	;
}

int
GenRepl::fixTag(void* _re, unsigned long old_index, unsigned long new_index)
{
    GenReplEntry *re = (GenReplEntry*)_re;
    assert(re->valid);
    if (re->tag_ptr == old_index) {
	re->tag_ptr = new_index;
	return 1;
    }
    fatal("Repl entry: tag ptrs do not match");
    return 0;
}

bool
GenRepl::findTagPtr(unsigned long index)
{
    for (int i = 0; i < num_pools + 1; ++i) {
	list<GenReplEntry*>::const_iterator iter = pools[i].entries.begin();
	list<GenReplEntry*>::const_iterator end = pools[i].entries.end();
	for (; iter != end; ++iter) {
	    if ((*iter)->valid && (*iter)->tag_ptr == index) {
		return true;
	    }
	}
    }
    return false;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(GenRepl)

    Param<int> num_pools;
    Param<int> fresh_res;
    Param<int> pool_res;

END_DECLARE_SIM_OBJECT_PARAMS(GenRepl)


BEGIN_INIT_SIM_OBJECT_PARAMS(GenRepl)

    INIT_PARAM(num_pools, "capacity in bytes"),
    INIT_PARAM(fresh_res, "associativity"),
    INIT_PARAM(pool_res, "block size in bytes")

END_INIT_SIM_OBJECT_PARAMS(GenRepl)


CREATE_SIM_OBJECT(GenRepl)
{
    return new GenRepl(getInstanceName(), num_pools, fresh_res, pool_res);
}

REGISTER_SIM_OBJECT("GenRepl", GenRepl)

#endif // DOXYGEN_SHOULD_SKIP_THIS
