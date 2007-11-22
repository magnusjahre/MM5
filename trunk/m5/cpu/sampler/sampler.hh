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

#ifndef __SAMPLING_CPU_HH__
#define __SAMPLING_CPU_HH__

#include <vector>

#include "sim/sim_object.hh"
#include "sim/eventq.hh"

class BaseCPU;

class Sampler : public SimObject
{
  private:

    std::vector<BaseCPU *> phase0_cpus;
    std::vector<BaseCPU *> phase1_cpus;
    std::vector<Tick> periods;

    int	phase;
    int numFinished;
    int size;

  protected:

    friend class CpuSwitchEvent;

  public:

    Sampler(const std::string &_name,
	    std::vector<BaseCPU *> &_phase0_cpus, 
	    std::vector<BaseCPU *> &_phase1_cpus, 
	    std::vector<Tick> &_periods);

    virtual ~Sampler();
    virtual void init();
    virtual void startup();

    void switchCPUs();

    void signalSwitched();

    void serialize(std::ostream &os);

    void unserialize(Checkpoint *cp, const std::string &section);
    
};

#endif // __SAMPLING_CPU_HH__
