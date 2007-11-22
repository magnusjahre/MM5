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

/** @file
 * Implementation of Tsunami platform.
 */

#include <deque>
#include <string>
#include <vector>

#include "cpu/intr_control.hh"
#include "dev/simconsole.hh"
#include "dev/ide_ctrl.hh"
#include "dev/tsunami_cchip.hh"
#include "dev/tsunami_pchip.hh"
#include "dev/tsunami_io.hh"
#include "dev/tsunami.hh"
#include "dev/pciconfigall.hh"
#include "sim/builder.hh"
#include "sim/system.hh"

using namespace std;

Tsunami::Tsunami(const string &name, System *s, IntrControl *ic,
                 PciConfigAll *pci)
    : Platform(name, ic, pci), system(s)
{
    // set the back pointer from the system to myself
    system->platform = this;

    for (int i = 0; i < Tsunami::Max_CPUs; i++)
        intr_sum_type[i] = 0;
}

Tick
Tsunami::intrFrequency()
{
    return io->frequency();
}

void
Tsunami::postConsoleInt()
{
    io->postPIC(0x10);
}

void
Tsunami::clearConsoleInt()
{
    io->clearPIC(0x10);
}

void
Tsunami::postPciInt(int line)
{
    cchip->postDRIR(line);
}

void
Tsunami::clearPciInt(int line)
{
    cchip->clearDRIR(line);
}

Addr
Tsunami::pciToDma(Addr pciAddr) const
{
    return pchip->translatePciToDma(pciAddr);
}

void
Tsunami::serialize(std::ostream &os)
{
    SERIALIZE_ARRAY(intr_sum_type, Tsunami::Max_CPUs);
}

void
Tsunami::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(intr_sum_type, Tsunami::Max_CPUs);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Tsunami)

    SimObjectParam<System *> system;
    SimObjectParam<IntrControl *> intrctrl;
    SimObjectParam<PciConfigAll *> pciconfig;

END_DECLARE_SIM_OBJECT_PARAMS(Tsunami)

BEGIN_INIT_SIM_OBJECT_PARAMS(Tsunami)

    INIT_PARAM(system, "system"),
    INIT_PARAM(intrctrl, "interrupt controller"),
    INIT_PARAM(pciconfig, "PCI configuration")

END_INIT_SIM_OBJECT_PARAMS(Tsunami)

CREATE_SIM_OBJECT(Tsunami)
{
    return new Tsunami(getInstanceName(), system, intrctrl, pciconfig);
}

REGISTER_SIM_OBJECT("Tsunami", Tsunami)
