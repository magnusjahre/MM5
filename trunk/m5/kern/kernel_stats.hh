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

#ifndef __KERNEL_STATS_HH__
#define __KERNEL_STATS_HH__

#include <map>
#include <stack>
#include <string>
#include <vector>

class BaseCPU;
class ExecContext;
class FnEvent;
// What does kernel stats expect is included?
class StaticInstBase;
class System;
enum Fault;

namespace Kernel {

enum cpu_mode { kernel, user, idle, interrupt, cpu_mode_num };
extern const char *modestr[];

class Binning
{
  private:
    std::string myname;
    System *system;

  private:
    // lisa's binning stuff
    struct fnCall
    {
	Stats::MainBin *myBin;
	std::string name;    
    };

    struct SWContext
    {
	Counter calls;
	std::stack<fnCall *> callStack;
    };

    std::map<const std::string, Stats::MainBin *> fnBins;
    std::map<const Addr, SWContext *> swCtxMap;

    std::multimap<const std::string, std::string> callerMap;
    void populateMap(std::string caller, std::string callee);

    std::vector<FnEvent *> fnEvents;

    Stats::Scalar<> fnCalls;

    Stats::MainBin *getBin(const std::string &name);
    bool findCaller(std::string, std::string) const; 

    SWContext *findContext(Addr pcb);
    bool addContext(Addr pcb, SWContext *ctx)
    {
        return (swCtxMap.insert(std::make_pair(pcb, ctx))).second;
    }

    void remContext(Addr pcb)
    {
        swCtxMap.erase(pcb);
    }

    void dumpState() const;
    
    SWContext *swctx;
    std::vector<std::string> binned_fns;

  private:
    Stats::MainBin *modeBin[cpu_mode_num];

  public:
    const bool bin;
    const bool fnbin;

    cpu_mode themode;
    void palSwapContext(ExecContext *xc);
    void execute(ExecContext *xc, const StaticInstBase *inst);
    void call(ExecContext *xc, Stats::MainBin *myBin);
    void changeMode(cpu_mode mode);

  public:
    Binning(System *sys);
    virtual ~Binning();

    const std::string name() const { return myname; }
    void regStats(const std::string &name);

  public:
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

class Statistics : public Serializable
{
    friend class Binning;

  private:
    std::string myname;
    ExecContext *xc;

    Addr idleProcess;
    cpu_mode themode;
    Tick lastModeTick;
    bool bin_int;

    void changeMode(cpu_mode newmode);

  private:
    Stats::Scalar<> _arm;
    Stats::Scalar<> _quiesce;
    Stats::Scalar<> _ivlb;
    Stats::Scalar<> _ivle;
    Stats::Scalar<> _hwrei;

    Stats::Vector<> _iplCount;
    Stats::Vector<> _iplGood;
    Stats::Vector<> _iplTicks;
    Stats::Formula _iplUsed;

    Stats::Vector<> _callpal;
    Stats::Vector<> _syscall;
    Stats::Vector<> _faults;

    Stats::Vector<> _mode;
    Stats::Vector<> _modeGood;
    Stats::Formula _modeFraction;
    Stats::Vector<> _modeTicks;

    Stats::Scalar<> _swap_context;

  private:
    int iplLast;
    Tick iplLastTick;

  public:
    Statistics(ExecContext *context);

    const std::string name() const { return myname; }
    void regStats(const std::string &name);

  public:
    void arm() { _arm++; }
    void quiesce() { _quiesce++; }
    void ivlb() { _ivlb++; }
    void ivle() { _ivle++; }
    void hwrei() { _hwrei++; }
    void fault(Fault fault) { _faults[fault]++; }
    void swpipl(int ipl);
    void mode(cpu_mode newmode);
    void context(Addr oldpcbb, Addr newpcbb);
    void callpal(int code);

    void setIdleProcess(Addr idle);

  public:
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

/* end namespace Kernel */ }

#endif // __KERNEL_STATS_HH__
