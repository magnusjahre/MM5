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

/**
 * @file
 * Definitions of BaseTags.
 */

#include "mem/cache/tags/base_tags.hh"

#include "mem/cache/base_cache.hh"
#include "cpu/smt.hh" //maxThreadsPerCPU
#include "sim/sim_exit.hh"

using namespace std;

void
BaseTags::setCache(BaseCache *_cache, bool useSwitchEvent)
{
    cache = _cache;
    objName = cache->name();
    
    if(cache->useUniformPartitioning && useSwitchEvent){
        assert(cache->uniformPartitioningStartTick >= 0);
        BaseTagsSwitchEvent* event = new BaseTagsSwitchEvent(this);
        event->schedule(cache->uniformPartitioningStartTick);
    }
}

void
BaseTags::regStats(const string &name)
{
    using namespace Stats;
    replacements
	.init(maxThreadsPerCPU)
	.name(name + ".replacements")
	.desc("number of replacements")
	.flags(total)
	;

    tagsInUse
	.name(name + ".tagsinuse")
	.desc("Cycle average of tags in use")
	;

    totalRefs
	.name(name + ".total_refs")
	.desc("Total number of references to valid blocks.")
	;

    sampledRefs
    	.name(name + ".sampled_refs")
    	.desc("Sample count of references to valid blocks.")
    	;

    avgRefs
	.name(name + ".avg_refs")
	.desc("Average number of references to valid blocks.")
	;

    avgRefs = totalRefs/sampledRefs;

    warmupCycle
	.name(name + ".warmup_cycle")
	.desc("Cycle when the warmup percentage was hit.")
	;

    registerExitCallback(new BaseTagsCallback(this));
}
