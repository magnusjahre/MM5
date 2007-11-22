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

#ifndef __DEV_PKTFIFO_HH__
#define __DEV_PKTFIFO_HH__

#include <iosfwd>
#include <list>
#include <string>

#include "dev/etherpkt.hh"
#include "sim/serialize.hh"

class Checkpoint;
class PacketFifo
{
  protected:
    std::list<PacketPtr> fifo;
    int _maxsize;
    int _size;
    int _reserved;

  public:
    explicit PacketFifo(int max) : _maxsize(max), _size(0), _reserved(0) {}
    virtual ~PacketFifo() {}

    int packets() const { return fifo.size(); }
    int maxsize() const { return _maxsize; }
    int size() const { return _size; }
    int reserved() const { return _reserved; }
    int avail() const { return _maxsize - _size - _reserved; }
    bool empty() const { return size() <= 0; }
    bool full() const { return avail() <= 0; }

    int reserve(int len = 0)
    {
	_reserved += len;
	assert(avail() >= 0);
	return _reserved;
    }

    bool push(PacketPtr ptr)
    {
	assert(_reserved <= ptr->length);
	if (avail() < ptr->length - _reserved)
	    return false;

	_size += ptr->length;
	fifo.push_back(ptr);
	_reserved = 0;
	return true;
    }

    PacketPtr front() { return fifo.front(); }

    void pop()
    {
	if (empty())
	    return;
	
	_size -= fifo.front()->length;
	fifo.front() = NULL;
	fifo.pop_front();
    }

    void clear()
    {
	fifo.clear();
	_size = 0;
	_reserved = 0;
    }

/**
 * Serialization stuff
 */
  public:
    void serialize(const std::string &base, std::ostream &os);
    void unserialize(const std::string &base,
		     Checkpoint *cp, const std::string &section);
};

#endif // __DEV_PKTFIFO_HH__
