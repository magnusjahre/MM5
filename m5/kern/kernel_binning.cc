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

#include <map>
#include <stack>
#include <string>

#include "base/loader/symtab.hh"
#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "cpu/static_inst.hh"
#include "kern/kernel_stats.hh"

using namespace std;
using namespace Stats;

namespace Kernel {

Binning::Binning(System *sys)
    : system(sys), swctx(NULL), binned_fns(system->params->binned_fns), 
      bin(system->params->bin), fnbin(bin && !binned_fns.empty())
{
    if (!bin)
	return;

    for (int i = 0; i < cpu_mode_num; ++i)
	modeBin[i] = new Stats::MainBin(modestr[i]);
    modeBin[kernel]->activate();

    if (!fnbin)
	return;

    int end = binned_fns.size();
    assert(!(end & 1));
    fnEvents.resize(end / 2);

    for (int i = 0; i < end; i += 2) {
	Stats::MainBin *Bin = new Stats::MainBin(binned_fns[i]);
	fnBins.insert(make_pair(binned_fns[i], Bin));

	if (binned_fns[i+1] == "null")
	    populateMap(binned_fns[i], "");
	else
	    populateMap(binned_fns[i], binned_fns[i+1]);

	Addr address = 0;
	if (system->kernelSymtab->findAddress(binned_fns[i], address))
	    fnEvents[i/2] = new FnEvent(&system->pcEventQueue, binned_fns[i],
					address, Bin);
	else
	    panic("could not find kernel symbol %s\n", binned_fns[i]);
    }
}

Binning::~Binning()
{
    int end = fnEvents.size();
    for (int i = 0; i < end; ++i)
	delete fnEvents[i];

    fnEvents.clear();
}

void
Binning::regStats(const string &_name)
{
    myname = _name;

    fnCalls
	.name(name() + ".fnCalls")
	.desc("all fn calls being tracked")
	;
}

void
Binning::changeMode(cpu_mode mode)
{
    themode = mode;
    if (bin && (!swctx || swctx->callStack.empty()))
	modeBin[mode]->activate();
}

void
Binning::palSwapContext(ExecContext *xc)
{
    if (!fnbin)
	return;

    DPRINTF(TCPIP, "swpctx event\n");

    SWContext *out = swctx;
    if (out) {
	DPRINTF(TCPIP, "swapping context out with this stack!\n");
	dumpState(); 
	Addr oldPCB = xc->regs.ipr[TheISA::IPR_PALtemp23];

	if (out->callStack.empty()) {
	    DPRINTF(TCPIP, "but removing it, cuz empty!\n");
	    SWContext *find = findContext(oldPCB);
	    if (find) {
		assert(findContext(oldPCB) == out);
		remContext(oldPCB);
	    }
	    delete out;
	} else {
	    DPRINTF(TCPIP, "switching out context with pcb %#x, top fn %s\n",
		    oldPCB, out->callStack.top()->name);
	    if (!findContext(oldPCB)) {
		if (!addContext(oldPCB, out))
		    panic("could not add context");
	    }
	}
    }

    Addr newPCB = xc->regs.intRegFile[16];
    SWContext *in = findContext(newPCB);
    swctx = in;
        
    if (in) {
	assert(!in->callStack.empty() &&
	       "should not be switching in empty context");
	DPRINTF(TCPIP, "swapping context in with this callstack!\n");
	dumpState();
	remContext(newPCB);
	fnCall *top = in->callStack.top();
	DPRINTF(TCPIP, "switching in to pcb %#x, %s\n", newPCB, top->name);
	assert(top->myBin && "should not switch to context with no Bin");
	top->myBin->activate();
    } else {
	modeBin[kernel]->activate();
    }
    DPRINTF(TCPIP, "end swpctx\n");
}

void
Binning::execute(ExecContext *xc, const StaticInstBase *inst)
{
    assert(!xc->misspeculating() && fnbin);

    if (swctx->callStack.empty() ||
	swctx->callStack.top()->name == "idle_thread")
	return;

    if (inst->isCall())
	swctx->calls++;

    if (inst->isReturn()) { 
	if (swctx->calls == 0) {
	    fnCall *top = swctx->callStack.top();
	    DPRINTF(TCPIP, "Removing %s from callstack of size %d.\n", 
		    top->name, swctx->callStack.size());   
	    delete top;
	    swctx->callStack.pop();
	    if (swctx->callStack.empty())
		modeBin[kernel]->activate();
	    else  
		swctx->callStack.top()->myBin->activate(); 
                    
	    dumpState();
	} else {  
	    swctx->calls--;
	}
    }
}

void
Binning::call(ExecContext *xc, Stats::MainBin *myBin)
{
    assert(bin && fnbin && "FnEvent must be in a binned system");
    SWContext *ctx = swctx;
    DPRINTF(TCPIP, "%s: %s event!!!\n", "asdf", system->name());

    if (ctx && !ctx->callStack.empty()) {
	DPRINTF(TCPIP, "already a callstack!\n");
        fnCall *last = ctx->callStack.top();

	if (last->name == "idle_thread")
	    ctx->calls++;

	if (!findCaller(name(), "") && !findCaller(name(), last->name)) {
	    DPRINTF(TCPIP, "but can't find parent %s\n", last->name);
            return;
        }
        ctx->calls--;

#if 0
	assert(!ctx->calls &&
	       "on a binned fn, calls should == 0 (but can happen in boot)");
#endif
    } else {
	DPRINTF(TCPIP, "no callstack yet\n");
        if (!findCaller(name(), "")) {
	    DPRINTF(TCPIP, "not the right function, returning\n");
            return;
        }    
        if (!ctx)  {	    
            DPRINTF(TCPIP, "creating new context for %s\n", name());
            ctx = new SWContext;
            swctx = ctx;
        } 
    }
    DPRINTF(TCPIP, "adding fn %s to context\n", name());
    fnCall *call = new fnCall;  
    call->myBin = myBin;
    call->name = name();
    ctx->callStack.push(call);
    myBin->activate();
    fnCalls++;
    DPRINTF(TCPIP, "fnCalls for %s is %d\n", "asdf", fnCalls.value());
    dumpState();
}

void
Binning::populateMap(string callee, string caller)
{
    multimap<const string, string>::const_iterator i;
    i = callerMap.insert(make_pair(callee, caller));
    assert(i != callerMap.end() && "should not fail populating callerMap");
}

bool
Binning::findCaller(string callee, string caller) const
{
    typedef multimap<const string, string>::const_iterator iter;
    pair<iter, iter> range;
    
    range = callerMap.equal_range(callee);
    for (iter i = range.first; i != range.second; ++i) {
        if ((*i).second == caller)
            return true;
    }
    return false;
}

Stats::MainBin *
Binning::getBin(const string &name)
{
    map<const string, Stats::MainBin *>::const_iterator i;
    i = fnBins.find(name);
    if (i == fnBins.end())
        panic("trying to getBin %s that is not on system map!", name);
    return (*i).second;
}

Binning::SWContext *
Binning::findContext(Addr pcb)
{
  map<Addr, SWContext *>::const_iterator iter;
  iter = swCtxMap.find(pcb);
  if (iter != swCtxMap.end()) {
      SWContext *ctx = (*iter).second;
      assert(ctx != NULL && "should never have a null ctx in ctxMap");
      return ctx;
  } else
      return NULL;
}

void
Binning::dumpState() const
{   
    if (!swctx)
	return;

    stack<fnCall *> copy(swctx->callStack);
    if (copy.empty())
	return;
    DPRINTF(TCPIP, "swctx, size: %d:\n", copy.size());
    fnCall *top;
    DPRINTF(TCPIP, "||     call : %d\n", swctx->calls);
    for (top = copy.top(); !copy.empty(); copy.pop() ) {
	top = copy.top();
	DPRINTF(TCPIP, "||  %13s : %s \n", top->name, top->myBin->name());
    }
}

void
Binning::serialize(ostream &os)
{
    int exemode = themode;
    SERIALIZE_SCALAR(exemode);

    if (!bin)
	return;

    Stats::MainBin *cur = Stats::MainBin::curBin();	
    string bin_name = cur->name();
    SERIALIZE_SCALAR(bin_name);

    if (!fnbin)
	return;

    bool ctx = (bool)swctx;
    SERIALIZE_SCALAR(ctx);

    if (ctx) {
	SERIALIZE_SCALAR(swctx->calls);
	stack<fnCall *> *stack = &(swctx->callStack);
	fnCall *top;
	int size = stack->size();
	SERIALIZE_SCALAR(size);

	for (int j = 0; j < size; ++j) {
	    top = stack->top();
	    paramOut(os, csprintf("stackpos[%d]",j), top->name);    
	    delete top;
	    stack->pop();
	}
    }

    map<const Addr, SWContext *>::const_iterator iter, end;
    iter = swCtxMap.begin();
    end = swCtxMap.end();

    int numCtxs = swCtxMap.size();
    SERIALIZE_SCALAR(numCtxs);
    for (int i = 0; iter != end; ++i, ++iter) {
	paramOut(os, csprintf("Addr[%d]",i), (*iter).first);
	SWContext *ctx = (*iter).second;
	paramOut(os, csprintf("calls[%d]",i), ctx->calls);
	    
	stack<fnCall *> *stack = &(ctx->callStack);	
	fnCall *top;
	int size = stack->size();
	paramOut(os, csprintf("stacksize[%d]",i), size);
	for (int j = 0; j < size; ++j) {
	    top = stack->top();
	    paramOut(os, csprintf("ctx[%d].stackpos[%d]", i, j), top->name);
	    delete top;
	    stack->pop();
	}	
    }
}

void
Binning::unserialize(Checkpoint *cp, const string &section)
{
    int exemode;
    UNSERIALIZE_SCALAR(exemode);
    themode = (cpu_mode)exemode;

    if (!bin)
	return;

    modeBin[system->execContexts[0]->kernelStats->themode]->activate();

    if (!fnbin)
	return;

    bool ctx;
    UNSERIALIZE_SCALAR(ctx);
    if (ctx) {
	swctx = new SWContext;
	UNSERIALIZE_SCALAR(swctx->calls);
	int size;
	UNSERIALIZE_SCALAR(size);

	vector<fnCall *> calls;
	fnCall *call;
	for (int i = 0; i < size; ++i) {
	    call = new fnCall;
	    paramIn(cp, section, csprintf("stackpos[%d]",i), call->name);
	    call->myBin = getBin(call->name);
	    calls.push_back(call);
	}

	for (int i = size - 1; i >= 0; --i)
	    swctx->callStack.push(calls[i]);
    }

    string bin_name;
    UNSERIALIZE_SCALAR(bin_name);
    getBin(bin_name)->activate();	

    int numCtxs;
    UNSERIALIZE_SCALAR(numCtxs);

    for(int i = 0; i < numCtxs; ++i) {
	Addr addr;
	paramIn(cp, section, csprintf("Addr[%d]",i), addr);	

	SWContext *ctx = new SWContext;
	paramIn(cp, section, csprintf("calls[%d]",i), ctx->calls);

	int size;
	paramIn(cp, section, csprintf("stacksize[%d]",i), size);
	    
	vector<fnCall *> calls;
	fnCall *call;
	for (int j = 0; j < size; ++j) {	    
	    call = new fnCall;
	    paramIn(cp, section, csprintf("ctx[%d].stackpos[%d]", i, j),
		    call->name);
	    call->myBin = getBin(call->name);
	    calls.push_back(call);
	}
	    
	for (int j = size - 1; j >= 0; --j)
	    ctx->callStack.push(calls[j]);
	    
	addContext(addr, ctx);
    }
}

/* end namespace Kernel */ }
