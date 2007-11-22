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

#ifndef __POLLEVENT_H__
#define __POLLEVENT_H__

#include <vector>
#include <poll.h>
#include "sim/root.hh"

class Checkpoint;
class PollQueue;

class PollEvent
{
  private:
    friend class PollQueue;

  protected:
    pollfd pfd;
    PollQueue *queue;
    bool enabled;

  public:
    PollEvent(int fd, int event);
    virtual ~PollEvent();

    void disable();
    void enable();
    virtual void process(int revent) = 0;

    bool queued() { return queue != 0; }

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

class PollQueue
{
  private:
    typedef std::vector<PollEvent *> eventvec_t;
    eventvec_t events;

    pollfd *poll_fds;
    int max_size;
    int num_fds;

  public:
    PollQueue();
    ~PollQueue();

    void copy();
    void remove(PollEvent *event);
    void schedule(PollEvent *event);
    void service();

  protected:
    static bool handler;
    static struct sigaction oldio;
    static struct sigaction oldalrm;

  public:
    static void setupAsyncIO(int fd, bool set);
    static void handleIO(int);
    static void handleALRM(int);
    static void removeHandler();
    static void setupHandler();
};

extern PollQueue pollQueue;

#endif // __POLLEVENT_H__
