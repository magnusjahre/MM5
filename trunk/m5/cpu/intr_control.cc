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

#include <string>
#include <vector>

#include "cpu/base.hh"
#include "cpu/intr_control.hh"
#include "sim/builder.hh"
#include "sim/sim_object.hh"

using namespace std;

IntrControl::IntrControl(const string &name, BaseCPU *c)
    : SimObject(name), cpu(c)
{}

/* @todo
 *Fix the cpu sim object parameter to be a system pointer
 *instead, to avoid some extra dereferencing
 */
void
IntrControl::post(int int_num, int index)
{ 
    std::vector<ExecContext *> &xcvec = cpu->system->execContexts;
    BaseCPU *temp = xcvec[0]->cpu;
    temp->post_interrupt(int_num, index); 
}

void
IntrControl::post(int cpu_id, int int_num, int index)
{ 
    std::vector<ExecContext *> &xcvec = cpu->system->execContexts;
    BaseCPU *temp = xcvec[cpu_id]->cpu;
    temp->post_interrupt(int_num, index); 
}

void
IntrControl::clear(int int_num, int index)
{
    std::vector<ExecContext *> &xcvec = cpu->system->execContexts;
    BaseCPU *temp = xcvec[0]->cpu;
    temp->clear_interrupt(int_num, index); 
}

void
IntrControl::clear(int cpu_id, int int_num, int index)
{
    std::vector<ExecContext *> &xcvec = cpu->system->execContexts;
    BaseCPU *temp = xcvec[cpu_id]->cpu;
    temp->clear_interrupt(int_num, index); 
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(IntrControl)

    SimObjectParam<BaseCPU *> cpu;

END_DECLARE_SIM_OBJECT_PARAMS(IntrControl)

BEGIN_INIT_SIM_OBJECT_PARAMS(IntrControl)

    INIT_PARAM(cpu, "the cpu")

END_INIT_SIM_OBJECT_PARAMS(IntrControl)

CREATE_SIM_OBJECT(IntrControl)
{
    return new IntrControl(getInstanceName(), cpu);
}

REGISTER_SIM_OBJECT("IntrControl", IntrControl)
