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
 * Definition for the memory hierarchy parameter object.
 */

#include "sim/builder.hh"
#include "mem/hier_params.hh"

using namespace std;

HierParams::HierParams(const string &name, bool do_data, bool do_events, int _hpCpuCount)
    : SimObject(name), doData(do_data), doEvents(do_events), hpCpuCount(_hpCpuCount)
{
}

HierParams defaultHierParams("defHier", false, true, 0);

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(HierParams)

    Param<bool> do_data;
    Param<bool> do_events;
    Param<int> cpu_count;

END_DECLARE_SIM_OBJECT_PARAMS(HierParams)

BEGIN_INIT_SIM_OBJECT_PARAMS(HierParams)
    
    INIT_PARAM(do_data, "Store data in this hierarchy"),
    INIT_PARAM(do_events, "Simulate timing in this hierarchy"),
    INIT_PARAM(cpu_count, "Number of CPUs")

END_INIT_SIM_OBJECT_PARAMS(HierParams)

CREATE_SIM_OBJECT(HierParams)
{
    return new HierParams(getInstanceName(), do_data, do_events, cpu_count);
}

REGISTER_SIM_OBJECT("HierParams", HierParams);

#endif //DOXYGEN_SHOULD_SKIP_THIS
