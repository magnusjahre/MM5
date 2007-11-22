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

#include <sys/ioctl.h>
#include <sys/types.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "sim/async.hh"
#include "sim/host.hh"
#include "base/misc.hh"
#include "base/pollevent.hh"
#include "sim/root.hh"
#include "sim/serialize.hh"

using namespace std;

PollQueue pollQueue;

/////////////////////////////////////////////////////
//
PollEvent::PollEvent(int _fd, int _events)
    : queue(NULL), enabled(true)
{
    pfd.fd = _fd;
    pfd.events = _events;
}

PollEvent::~PollEvent()
{
    if (queue)
	queue->remove(this);
}

void
PollEvent::disable()
{
    if (!enabled) return;
    enabled = false;

    if (queue)
	queue->copy();
}

void
PollEvent::enable()
{
    if (enabled) return;
    enabled = true;

    if (queue)
	queue->copy();
}

void
PollEvent::serialize(ostream &os)
{
    SERIALIZE_SCALAR(pfd.fd);
    SERIALIZE_SCALAR(pfd.events);
    SERIALIZE_SCALAR(enabled);
}

void
PollEvent::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_SCALAR(pfd.fd);
    UNSERIALIZE_SCALAR(pfd.events);
    UNSERIALIZE_SCALAR(enabled);
}

/////////////////////////////////////////////////////
//
PollQueue::PollQueue()
    : poll_fds(NULL), max_size(0), num_fds(0)
{ }

PollQueue::~PollQueue()
{
    removeHandler();
    for (int i = 0; i < num_fds; i++)
	setupAsyncIO(poll_fds[0].fd, false);

    delete [] poll_fds;
}

void
PollQueue::copy()
{
    eventvec_t::iterator i = events.begin();
    eventvec_t::iterator end = events.end();

    num_fds = 0;

    while (i < end) {
	if ((*i)->enabled)
	    poll_fds[num_fds++] = (*i)->pfd;
	++i;
    }
}

void
PollQueue::remove(PollEvent *event)
{
    eventvec_t::iterator i = events.begin();
    eventvec_t::iterator end = events.end();

    while (i < end) {
	if (*i == event) {
	   events.erase(i);
	   copy();
	   event->queue = NULL;
	   return;
	}

	++i;
    }

    panic("Event does not exist.  Cannot remove.");
}

void
PollQueue::schedule(PollEvent *event)
{
    if (event->queue)
	panic("Event already scheduled!");

    event->queue = this;
    events.push_back(event);
    setupAsyncIO(event->pfd.fd, true);

    // if we ran out of space in the fd array, double the capacity
    // if this is the first time that we've scheduled an event, create
    // the array with an initial size of 16
    if (++num_fds > max_size) {
	if (max_size > 0) {
	    delete [] poll_fds;
	    max_size *= 2;
	} else {
	    max_size = 16;
	    setupHandler();
	}

	poll_fds = new pollfd[max_size];
    }

    copy();
}

void
PollQueue::service()
{
    int ret = poll(poll_fds, num_fds, 0);

    if (ret <= 0)
	return;

    for (int i = 0; i < num_fds; i++) {
	int revents = poll_fds[i].revents;
	if (revents) {
	    events[i]->process(revents);
	    if (--ret <= 0)
		break;
	}
    }
}

struct sigaction PollQueue::oldio;
struct sigaction PollQueue::oldalrm;
bool PollQueue::handler = false;

void
PollQueue::setupAsyncIO(int fd, bool set)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
	panic("Could not set up async IO");

    if (set)
	flags |= FASYNC;
    else
	flags &= ~(FASYNC);

    if (fcntl(fd, F_SETFL, flags) == -1)
	panic("Could not set up async IO");

    if (set) {
      if (fcntl(fd, F_SETOWN, getpid()) == -1)
	panic("Could not set up async IO");
    }
}

void
PollQueue::setupHandler()
{
    struct sigaction act;

    act.sa_handler = handleIO;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction(SIGIO, &act, &oldio) == -1)
	panic("could not do sigaction");

    act.sa_handler = handleALRM;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction(SIGALRM, &act, &oldalrm) == -1)
	panic("could not do sigaction");

    alarm(1);

    handler = true;
}

void
PollQueue::removeHandler()
{
    if (sigaction(SIGIO, &oldio, NULL) == -1)
	panic("could not remove handler");

    if (sigaction(SIGIO, &oldalrm, NULL) == -1)
	panic("could not remove handler");
}

void
PollQueue::handleIO(int sig)
{
    if (sig != SIGIO)
	panic("Wrong Handler");

    async_event = true;
    async_io = true;
}

void
PollQueue::handleALRM(int sig)
{
    if (sig != SIGALRM)
	panic("Wrong Handler");

    async_event = true;
    async_alarm = true;
    alarm(1);
}

