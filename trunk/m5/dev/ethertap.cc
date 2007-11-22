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

/* @file
 * Interface to connect a simulated ethernet device to the real world
 */

#if defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/param.h>
#endif
#include <netinet/in.h>

#include <unistd.h>

#include <deque>
#include <string>

#include "base/misc.hh"
#include "base/pollevent.hh"
#include "base/socket.hh"
#include "base/trace.hh"
#include "dev/etherdump.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "dev/ethertap.hh"
#include "sim/builder.hh"

using namespace std;

/**
 */
class TapListener
{
  protected:
    /**
     */
    class Event : public PollEvent
    {
      protected:
	TapListener *listener;

      public:
	Event(TapListener *l, int fd, int e)
	    : PollEvent(fd, e), listener(l) {}

	virtual void process(int revent) { listener->accept(); }
    };

    friend class Event;
    Event *event;

  protected:
    ListenSocket listener;
    EtherTap *tap;
    int port;

  public:
    TapListener(EtherTap *t, int p)
	: event(NULL), tap(t), port(p) {}
    ~TapListener() { if (event) delete event; }

    void accept();
    void listen();
};

void
TapListener::listen()
{
    while (!listener.listen(port, true)) {
	DPRINTF(Ethernet, "TapListener(listen): Can't bind port %d\n", port);
	port++;
    }

    ccprintf(cerr, "Listening for tap connection on port %d\n", port);
    event = new Event(this, listener.getfd(), POLLIN|POLLERR);
    pollQueue.schedule(event);
}

void
TapListener::accept()
{
    if (!listener.islistening())
	panic("TapListener(accept): cannot accept if we're not listening!");

    int sfd = listener.accept(true);
    if (sfd != -1)
	tap->attach(sfd);
}

/**
 */
class TapEvent : public PollEvent
{
  protected:
    EtherTap *tap;

  public:
    TapEvent(EtherTap *_tap, int fd, int e)
	: PollEvent(fd, e), tap(_tap) {}
    virtual void process(int revent) { tap->process(revent); }
};

EtherTap::EtherTap(const string &name, EtherDump *d, int port, int bufsz)
    : EtherInt(name), event(NULL), socket(-1), buflen(bufsz), dump(d),
      txEvent(this)
{
    buffer = new char[buflen];
    listener = new TapListener(this, port);
    listener->listen();
}

EtherTap::~EtherTap()
{
    if (event)
	delete event;
    if (buffer)
	delete [] buffer;

    delete listener;
}

void
EtherTap::attach(int fd)
{
    if (socket != -1)
	close(fd);

    buffer_offset = 0;
    data_len = 0;
    socket = fd;
    DPRINTF(Ethernet, "EtherTap attached\n");
    event = new TapEvent(this, socket, POLLIN|POLLERR);
    pollQueue.schedule(event);
}

void
EtherTap::detach()
{
    DPRINTF(Ethernet, "EtherTap detached\n");
    delete event;
    event = 0;
    close(socket);
    socket = -1;
}

bool
EtherTap::recvPacket(PacketPtr packet)
{
    if (dump)
	dump->dump(packet);

    DPRINTF(Ethernet, "EtherTap output len=%d\n", packet->length);
    DDUMP(EthernetData, packet->data, packet->length);
    u_int32_t len = htonl(packet->length);
    write(socket, &len, sizeof(len));
    write(socket, packet->data, packet->length);

    recvDone();

    return true;
}

void
EtherTap::sendDone()
{}

void
EtherTap::process(int revent)
{
    if (revent & POLLERR) {
	detach();
	return;
    }

    char *data = buffer + sizeof(u_int32_t);
    if (!(revent & POLLIN))
	return;

    if (buffer_offset < data_len + sizeof(u_int32_t)) {
	int len = read(socket, buffer + buffer_offset, buflen - buffer_offset);
	if (len == 0) {
	    detach();
	    return;
	}

	buffer_offset += len;

	if (data_len == 0)
	    data_len = ntohl(*(u_int32_t *)buffer);

	DPRINTF(Ethernet, "Received data from peer: len=%d buffer_offset=%d "
		"data_len=%d\n", len, buffer_offset, data_len);
    }

    while (data_len != 0 && buffer_offset >= data_len + sizeof(u_int32_t)) {
	PacketPtr packet;
	packet = new PacketData(data_len);
	packet->length = data_len;
	memcpy(packet->data, data, data_len);

	buffer_offset -= data_len + sizeof(u_int32_t);
	assert(buffer_offset >= 0);
	if (buffer_offset > 0) {
	    memmove(buffer, data + data_len, buffer_offset);
	    data_len = ntohl(*(u_int32_t *)buffer);
	} else
	    data_len = 0;

	DPRINTF(Ethernet, "EtherTap input len=%d\n", packet->length);
	DDUMP(EthernetData, packet->data, packet->length);
	if (!sendPacket(packet)) {
	    DPRINTF(Ethernet, "bus busy...buffer for retransmission\n");
	    packetBuffer.push(packet);
	    if (!txEvent.scheduled())
		txEvent.schedule(curTick + retryTime);
	} else if (dump) {
	    dump->dump(packet);
	}
    }
}

void
EtherTap::retransmit()
{
    if (packetBuffer.empty())
	return;

    PacketPtr packet = packetBuffer.front();
    if (sendPacket(packet)) {
	if (dump)
	    dump->dump(packet);
	DPRINTF(Ethernet, "EtherTap retransmit\n");
	packetBuffer.front() = NULL;
	packetBuffer.pop();
    }

    if (!packetBuffer.empty() && !txEvent.scheduled())
	txEvent.schedule(curTick + retryTime);
}

//=====================================================================

void
EtherTap::serialize(ostream &os)
{
    SERIALIZE_SCALAR(socket);
    SERIALIZE_SCALAR(buflen);
    uint8_t *buffer = (uint8_t *)this->buffer;
    SERIALIZE_ARRAY(buffer, buflen);
    SERIALIZE_SCALAR(buffer_offset);
    SERIALIZE_SCALAR(data_len);

    bool tapevent_present = false;
    if (event) {
	tapevent_present = true;
	SERIALIZE_SCALAR(tapevent_present);
	event->serialize(os);
    }
    else {
	SERIALIZE_SCALAR(tapevent_present);
    }
}

void
EtherTap::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_SCALAR(socket);
    UNSERIALIZE_SCALAR(buflen);
    uint8_t *buffer = (uint8_t *)this->buffer;
    UNSERIALIZE_ARRAY(buffer, buflen);
    UNSERIALIZE_SCALAR(buffer_offset);
    UNSERIALIZE_SCALAR(data_len);

    bool tapevent_present;
    UNSERIALIZE_SCALAR(tapevent_present);
    if (tapevent_present) {
	event = new TapEvent(this, socket, POLLIN|POLLERR);

	event->unserialize(cp,section);

	if (event->queued()) {
	    pollQueue.schedule(event);
	}
    }
}

//=====================================================================

BEGIN_DECLARE_SIM_OBJECT_PARAMS(EtherTap)

    SimObjectParam<EtherInt *> peer;
    SimObjectParam<EtherDump *> dump;
    Param<unsigned> port;
    Param<unsigned> bufsz;

END_DECLARE_SIM_OBJECT_PARAMS(EtherTap)

BEGIN_INIT_SIM_OBJECT_PARAMS(EtherTap)

    INIT_PARAM_DFLT(peer, "peer interface", NULL),
    INIT_PARAM_DFLT(dump, "object to dump network packets to", NULL),
    INIT_PARAM_DFLT(port, "tap port", 3500),
    INIT_PARAM_DFLT(bufsz, "tap buffer size", 10000)

END_INIT_SIM_OBJECT_PARAMS(EtherTap)


CREATE_SIM_OBJECT(EtherTap)
{
    EtherTap *tap = new EtherTap(getInstanceName(), dump, port, bufsz);

    if (peer) {
	tap->setPeer(peer);
	peer->setPeer(tap);
    }

    return tap;
}

REGISTER_SIM_OBJECT("EtherTap", EtherTap)
