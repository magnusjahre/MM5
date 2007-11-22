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

#ifndef __ENCUMBERED_CPU_FULL_PIPETRACE_HH__
#define __ENCUMBERED_CPU_FULL_PIPETRACE_HH__

#include <fstream>
#include <string>
#include <vector>

#include "base/str.hh"
#include "base/range.hh"
#include "cpu/inst_seq.hh"
#include "encumbered/cpu/full/dyn_inst.hh"


class PipeTrace : public SimObject
{
  public:
    static const unsigned CacheMiss  = 0x0001;
    static const unsigned TLBMiss    = 0x0002;
    static const unsigned MisPredict = 0x0004;
    static const unsigned MPDetected = 0x0008;
    static const unsigned AddressGen = 0x0010;

    enum stageID {
	Fetch = 0,
	Dispatch,
	Issue,
	Writeback,
	Commit,
	Num_stageIDs
    };

  private:
    static const char * const stageNames[];

    std::ostream *outfile;
    Range<Tick> range;

    bool active;

    bool exit_when_done;

    bool useStats;
//    std::vector<stat_stat_t *> statistics;
    std::vector<std::string> stat_names;

  public:
    PipeTrace(const std::string &name, const std::string &filename,
	      std::string &_range, bool exit_mode,
	      bool statsValid, std::vector<std::string> _stats);
    ~PipeTrace();

    //  This actually finishes initializing the stat tables for lookup
    void regStats();

    bool newCycle(Tick cycle);

    void newInst(DynInst *inst);

    void moveInst(DynInst *inst, stageID new_stage, unsigned events,
		  unsigned miss_latency, unsigned longest_event);

    void deleteInst(DynInst *inst);
};


#endif // __ENCUMBERED_CPU_FULL_PIPETRACE_HH__
