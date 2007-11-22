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
 * Device module for modelling a fixed bandwidth full duplex ethernet link
 */

#ifndef __DEV_ETHERLINK_HH__
#define __DEV_ETHERLINK_HH__

#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "sim/eventq.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"

class EtherDump;
class Checkpoint;
/*
 * Model for a fixed bandwidth full duplex ethernet link
 */
class EtherLink : public SimObject
{
  protected:
    class Interface;

    friend class LinkDelayEvent;
     /*
      * Model for a single uni-directional link
      */
    class Link
    {
      protected:
        std::string objName;

        EtherLink *parent;
        int number;

	Interface *txint;
	Interface *rxint;

	double ticksPerByte;
	Tick linkDelay;
	EtherDump *dump;

      protected:
        /*
	 * Transfer is complete
	 */
	PacketPtr packet;
	void txDone();
	typedef EventWrapper<Link, &Link::txDone> DoneEvent;
	friend void DoneEvent::process();
	DoneEvent doneEvent;

	friend class LinkDelayEvent;
	void txComplete(PacketPtr packet);

      public:
	Link(const std::string &name, EtherLink *p, int num, 
             double rate, Tick delay, EtherDump *dump);
	~Link() {}

        const std::string name() const { return objName; }

	bool busy() const { return (bool)packet; }
	bool transmit(PacketPtr packet);

	void setTxInt(Interface *i) { assert(!txint); txint = i; }
	void setRxInt(Interface *i) { assert(!rxint); rxint = i; }

	void serialize(const std::string &base, std::ostream &os);
	void unserialize(const std::string &base, Checkpoint *cp, 
                                 const std::string &section);
    };

    /*
     * Interface at each end of the link
     */
    class Interface : public EtherInt
    {
      private:
	Link *txlink;

      public:
	Interface(const std::string &name, Link *txlink, Link *rxlink);
	bool recvPacket(PacketPtr packet) { return txlink->transmit(packet); }
	void sendDone() { peer->sendDone(); }
    };

    Link *link[2];
    EtherInt *interface[2];

  public:
    EtherLink(const std::string &name, EtherInt *peer0, EtherInt *peer1,
	      double rate, Tick delay, EtherDump *dump);
    virtual ~EtherLink();

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

};

#endif // __ETHERLINK_HH__
