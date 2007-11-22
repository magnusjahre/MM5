/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include <fstream>
#include <iomanip>

#include "sim/param.hh"
#include "encumbered/cpu/full/dyn_inst.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/cpu/full/issue.hh"
#include "cpu/exetrace.hh"
#include "cpu/exec_context.hh"
#include "base/loader/symtab.hh"
#include "cpu/base.hh"
#include "cpu/static_inst.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////
//
//  Methods for the InstRecord object
//


void
Trace::InstRecord::dump(ostream &outs)
{
    
    if (flags[PRINT_CYCLE])
	ccprintf(outs, "%7d: ", cycle);

    outs << cpu->name() << " ";

    if (flags[TRACE_MISSPEC])
	outs << (misspeculating ? "-" : "+") << " ";

    if (flags[PRINT_THREAD_NUM])
	outs << "T" << thread << " : ";


    std::string sym_str;
    Addr sym_addr;
    if (debugSymbolTable
	&& debugSymbolTable->findNearestSymbol(PC, sym_str, sym_addr)) {
	if (PC != sym_addr)
	    sym_str += csprintf("+%d", PC - sym_addr);
        outs << "@" << sym_str << " : ";
    }
    else {
        outs << "0x" << hex << PC << " : ";
    }

    //
    //  Print decoded instruction
    //

#if defined(__GNUC__) && (__GNUC__ < 3)
    // There's a bug in gcc 2.x library that prevents setw()
    // from working properly on strings
    string mc(staticInst->disassemble(PC, debugSymbolTable));
    while (mc.length() < 26)
	mc += " ";
    outs << mc;
#else
    outs << setw(26) << left << staticInst->disassemble(PC, debugSymbolTable);
#endif

    outs << " : ";

    if (flags[PRINT_OP_CLASS]) {
	outs << opClassStrings[staticInst->opClass()] << " : ";
    }

    if (flags[PRINT_RESULT_DATA] && data_status != DataInvalid) {
	outs << " D=";
#if 0
	if (data_status == DataDouble)
	    ccprintf(outs, "%f", data.as_double);
	else
	    ccprintf(outs, "%#018x", data.as_int);
#else
	ccprintf(outs, "%#018x", data.as_int);
#endif
    }

    if (flags[PRINT_EFF_ADDR] && addr_valid)
	outs << " A=0x" << hex << addr;

    if (flags[PRINT_INT_REGS] && regs_valid) {
	for (int i = 0; i < 32;)
	    for (int j = i + 1; i <= j; i++)
		ccprintf(outs, "r%02d = %#018x%s", i, iregs->regs[i],
			 ((i == j) ? "\n" : "    "));
	outs << "\n";
    }

    if (flags[PRINT_FETCH_SEQ] && fetch_seq_valid)
	outs << "  FetchSeq=" << dec << fetch_seq;

    if (flags[PRINT_CP_SEQ] && cp_seq_valid)
        outs << "  CPSeq=" << dec << cp_seq;

    //
    //  End of line...
    //
    outs << endl;
}


vector<bool> Trace::InstRecord::flags(NUM_BITS);

////////////////////////////////////////////////////////////////////////
//
// Parameter space for per-cycle execution address tracing options.
// Derive from ParamContext so we can override checkParams() function.
//
class ExecutionTraceParamContext : public ParamContext
{
  public:
    ExecutionTraceParamContext(const string &_iniSection)
	: ParamContext(_iniSection)
	{
	}

    void checkParams();	// defined at bottom of file
};

ExecutionTraceParamContext exeTraceParams("exetrace");

Param<bool> exe_trace_spec(&exeTraceParams, "speculative",
			   "capture speculative instructions", true);

Param<bool> exe_trace_print_cycle(&exeTraceParams, "print_cycle",
				  "print cycle number", true);
Param<bool> exe_trace_print_opclass(&exeTraceParams, "print_opclass",
				  "print op class", true);
Param<bool> exe_trace_print_thread(&exeTraceParams, "print_thread",
				  "print thread number", true);
Param<bool> exe_trace_print_effaddr(&exeTraceParams, "print_effaddr",
				  "print effective address", true);
Param<bool> exe_trace_print_data(&exeTraceParams, "print_data",
				  "print result data", true);
Param<bool> exe_trace_print_iregs(&exeTraceParams, "print_iregs",
				  "print all integer regs", false);
Param<bool> exe_trace_print_fetchseq(&exeTraceParams, "print_fetchseq",
				  "print fetch sequence number", false);
Param<bool> exe_trace_print_cp_seq(&exeTraceParams, "print_cpseq",
				  "print correct-path sequence number", false);

//
// Helper function for ExecutionTraceParamContext::checkParams() just
// to get us into the InstRecord namespace
//
void
Trace::InstRecord::setParams()
{
    flags[TRACE_MISSPEC]     = exe_trace_spec;

    flags[PRINT_CYCLE]       = exe_trace_print_cycle;
    flags[PRINT_OP_CLASS]    = exe_trace_print_opclass;
    flags[PRINT_THREAD_NUM]  = exe_trace_print_thread;
    flags[PRINT_RESULT_DATA] = exe_trace_print_effaddr;
    flags[PRINT_EFF_ADDR]    = exe_trace_print_data;
    flags[PRINT_INT_REGS]    = exe_trace_print_iregs;
    flags[PRINT_FETCH_SEQ]   = exe_trace_print_fetchseq;
    flags[PRINT_CP_SEQ]      = exe_trace_print_cp_seq;
}

void
ExecutionTraceParamContext::checkParams()
{
    Trace::InstRecord::setParams();
}

