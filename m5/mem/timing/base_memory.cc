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
 * Definitions of the base main memory object.
 */

#include "mem/timing/base_memory.hh"
#include "cpu/smt.hh"

using namespace std;

BaseMemory::BaseMemory(const std::string &name, HierParams *hier,
		       Params &params)
    : BaseMem(name, hier, params.access_lat, params.addrRange),
#if FULL_SYSTEM
      funcMem(params.funcMem),
#endif
      uncacheLatency(params.uncache_lat),
      snarfUpdates(params.snarf_updates), doWrites(params.do_writes)
{

    assert(hier != NULL);
    bmCPUCount = hier->hpCpuCount;
}

void
BaseMemory::regStats()
{
    using namespace Stats;

    number_of_reads
            .name(name() + ".number_of_reads")
            .desc("Total number of reads")
            ;

    number_of_writes
            .name(name() + ".number_of_writes")
            .desc("Total number of writes")
            ;

    number_of_reads_hit
            .name(name() + ".number_of_read_hits")
            .desc("The number of reads that hit open page")
            ;

    number_of_writes_hit
            .name(name() + ".number_of_write_hits")
            .desc("The number of writes that hit open pages")
            ;

    total_latency
            .name(name() + ".total_latency")
            .desc("Total latency")
            ;

    average_latency
            .name(name() + ".average_latency")
            .desc("Average latency")
            ;

    average_latency = total_latency / (number_of_reads + number_of_writes);

    read_hit_rate
            .name(name() + ".read_hit_rate")
            .desc("Rate of hitting open pages on read")
            ;

    read_hit_rate = number_of_reads_hit / number_of_reads;

    write_hit_rate
            .name(name() + ".write_hit_rate")
            .desc("Rate of hitting write pages on write")
            ;

    write_hit_rate = number_of_writes_hit / number_of_writes;

    overall_hit_rate
            .name(name() + ".overall_hit_rate")
            .desc("Overall page hit rate")
            ;

    overall_hit_rate = (number_of_reads_hit + number_of_writes_hit) / (number_of_reads + number_of_writes);

    number_of_slow_read_hits
            .name(name() + ".number_of_slow_read_hits")
            .desc("Number of transitions from write to read")
            ;

    number_of_slow_write_hits
            .name(name() + ".number_of_slow_write_hits")
            .desc("Number of transitions from read to write")
            ;

    number_of_non_overlap_activate
            .name(name() + ".number_of_non_overlap_activate")
            .desc("Number of non overlapping activates")
            ;

    accessesPerBank.init(num_banks);
    accessesPerBank
            .name(name() + ".accesses_per_bank")
            .desc("number of accesses for each bank")
            .flags(total)
            ;

    pageConflicts
        .init(DRAM_CMD_CNT)
        .name(name() + ".number_of_page_conflicts")
        .desc("number of page conflits")
        ;

    pageMisses
        .init(DRAM_CMD_CNT)
        .name(name() + ".number_of_page_misses")
        .desc("number of page misses")
        ;

    pageHits
        .init(DRAM_CMD_CNT)
        .name(name() + ".number_of_page_hits")
        .desc("number of page hits")
        ;

    pageConflictLatency
        .init(DRAM_CMD_CNT)
        .name(name() + ".total_page_conflict_latency")
        .desc("total page conflict latency")
        ;

    pageConflictLatency.subname(0, "read");
    pageConflictLatency.subname(1, "write");

    pageMissLatency
        .init(DRAM_CMD_CNT)
        .name(name() + ".total_page_miss_latency")
        .desc("total page miss latency")
        ;

    pageMissLatency.subname(0, "read");
    pageMissLatency.subname(1, "write");

    pageHitLatency
        .init(DRAM_CMD_CNT)
        .name(name() + ".total_page_hit_latency")
        .desc("total page hit latency")
        ;

    pageHitLatency.subname(0, "read");
    pageHitLatency.subname(1, "write");

    avgPageConflictLatency
        .name(name() + ".average_page_conflict_latency")
        .desc("Average page conflict latency")
        ;

    avgPageConflictLatency.subname(0, "read");
    avgPageConflictLatency.subname(1, "write");

    avgPageConflictLatency = pageConflictLatency / pageConflicts;

    avgPageMissLatency
        .name(name() + ".average_page_miss_latency")
        .desc("Average page miss latency")
        ;

    avgPageMissLatency.subname(0, "read");
    avgPageMissLatency.subname(1, "write");

    avgPageMissLatency = pageMissLatency / pageMisses;

    avgPageHitLatency
        .name(name() + ".average_page_hit_latency")
        .desc("Average page hit latency")
        ;

    avgPageHitLatency.subname(0, "read");
    avgPageHitLatency.subname(1, "write");

    avgPageHitLatency = pageHitLatency / pageHits;

    pageConflictLatencyDistribution
         .init(DRAM_CMD_CNT, 40, 300, 5)
        .name(name() + ".conflict_distribution")
        .desc("Conflict latency distribution")
        .flags(total | pdf | cdf)
        ;

    pageConflictLatencyDistribution.subname(0, "_read");
    pageConflictLatencyDistribution.subname(1, "_write");

    pageMissLatencyDistribution
        .init(DRAM_CMD_CNT, 40, 130, 5)
        .name(name() + ".miss_distribution")
        .desc("Miss latency distribution")
        .flags(total | pdf | cdf)
        ;

    pageMissLatencyDistribution.subname(0, "_read");
    pageMissLatencyDistribution.subname(1, "_write");

    perCPUPageConflicts
        .init(bmCPUCount)
        .name(name() + ".per_cpu_page_conflicts")
        .desc("number of page conflicts per CPU")
        ;

    perCPUPageMisses
        .init(bmCPUCount)
        .name(name() + ".per_cpu_page_misses")
        .desc("number of page misses per CPU")
        ;

    perCPUPageHits
        .init(bmCPUCount)
        .name(name() + ".per_cpu_page_hits")
        .desc("number of page hits per CPU")
        ;

    perCPURequests
        .init(bmCPUCount)
        .name(name() + ".per_cpu_requests")
        .desc("number of requests per CPU")
        ;

    perCPUConflictRate
        .name(name() + ".per_cpu_page_conflict_rate")
        .desc("per CPU page conflict rate")
        ;

    perCPUConflictRate = perCPUPageConflicts / perCPURequests;

    perCPUMissRate
        .name(name() + ".per_cpu_page_miss_rate")
        .desc("per CPU page miss rate")
        ;

    perCPUMissRate = perCPUPageMisses / perCPURequests;

    perCPUHitRate
        .name(name() + ".per_cpu_page_hit_rate")
        .desc("per CPU page hit rate")
        ;

    perCPUHitRate = perCPUPageHits / perCPURequests;

}
