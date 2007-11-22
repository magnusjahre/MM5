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

/* @file
 * Device module for modelling an ethernet hub
 */

#include <cmath>
#include <deque>
#include <string>
#include <vector>

#include "base/trace.hh"
#include "dev/etherbus.hh"
#include "dev/etherdump.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "sim/builder.hh"
#include "sim/root.hh"

using namespace std;

EtherBus::EtherBus(const string &name, double speed, bool loop,
		   EtherDump *packet_dump)
    : SimObject(name), ticksPerByte(speed), loopback(loop),
      event(&mainEventQueue, this), sender(0), dump(packet_dump)
{
}

void
EtherBus::txDone()
{
    devlist_t::iterator i = devlist.begin();
    devlist_t::iterator end = devlist.end();

    DPRINTF(Ethernet, "ethernet packet received: length=%d\n", packet->length);
    DDUMP(EthernetData, packet->data, packet->length);

    while (i != end) {
	if (loopback || *i != sender)
	    (*i)->sendPacket(packet);
	++i;
    }

    sender->sendDone();

    if (dump)
	dump->dump(packet);

    sender = 0;
    packet = 0;
}

void
EtherBus::reg(EtherInt *dev)
{ devlist.push_back(dev); }

bool
EtherBus::send(EtherInt *sndr, PacketPtr &pkt)
{
    if (busy()) {
	DPRINTF(Ethernet, "ethernet packet not sent, bus busy\n", curTick);
	return false;
    }

    DPRINTF(Ethernet, "ethernet packet sent: length=%d\n", pkt->length);
    DDUMP(EthernetData, pkt->data, pkt->length);

    packet = pkt;
    sender = sndr;
    int delay = (int)ceil(((double)pkt->length * ticksPerByte) + 1.0);
    DPRINTF(Ethernet, "scheduling packet: delay=%d, (rate=%f)\n",
	    delay, ticksPerByte);
    event.schedule(curTick + delay);

    return true;
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(EtherBus)

    Param<bool> loopback;
    Param<double> speed;
    SimObjectParam<EtherDump *> packet_dump;

END_DECLARE_SIM_OBJECT_PARAMS(EtherBus)

BEGIN_INIT_SIM_OBJECT_PARAMS(EtherBus)

    INIT_PARAM(loopback, "send the packet back to the sending interface"),
    INIT_PARAM(speed, "bus speed in ticks per byte"),
    INIT_PARAM(packet_dump, "object to dump network packets to")

END_INIT_SIM_OBJECT_PARAMS(EtherBus)

CREATE_SIM_OBJECT(EtherBus)
{
    return new EtherBus(getInstanceName(), speed, loopback, packet_dump);
}

REGISTER_SIM_OBJECT("EtherBus", EtherBus)
