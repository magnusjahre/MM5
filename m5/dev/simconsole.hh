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



/* @file
 * User Console Interface
 */

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <iostream>

#include "base/circlebuf.hh"
#include "cpu/intr_control.hh"
#include "base/pollevent.hh"
#include "base/socket.hh"
#include "sim/sim_object.hh"

class ConsoleListener;
class Uart;

class SimConsole : public SimObject
{
  public:
    Uart *uart;

  protected:
    class Event : public PollEvent
    {
      protected:
        SimConsole *cons;

      public:
        Event(SimConsole *c, int fd, int e);
        void process(int revent);
    };

    friend class Event;
    Event *event;

  protected:
    int number;
    int in_fd;
    int out_fd;
    ConsoleListener *listener;

  public:
    SimConsole(const std::string &name, std::ostream *os, int num);
    ~SimConsole();

  protected:
    CircleBuf txbuf;
    CircleBuf rxbuf;
    std::ostream *outfile;
#if TRACING_ON == 1
    CircleBuf linebuf;
#endif

  public:
    ///////////////////////
    // Terminal Interface

    void attach(int fd, ConsoleListener *l = NULL) { attach(fd, fd, l); }
    void attach(int in, int out, ConsoleListener *l = NULL);
    void detach();

    void data();

    void close();
    void read(uint8_t &c) { read(&c, 1); }
    size_t read(uint8_t *buf, size_t len);
    void write(uint8_t c) { write(&c, 1); }
    size_t write(const uint8_t *buf, size_t len);
    
  public:
    /////////////////
    // OS interface

    // Get a character from the console.
    bool in(uint8_t &value);

    // get a character from the console in the console specific format
    // corresponds to GETC:
    // retval<63:61>
    //     000: success: character received
    //     001: success: character received, more pending
    //     100: failure: no character ready
    //     110: failure: character received with error
    //     111: failure: character received with error, more pending
    // retval<31:0>
    //     character read from console
    //
    // Interrupts are cleared when the buffer is empty.
    uint64_t console_in();

    // Send a character to the console
    void out(char c);

    //Ask the console if data is available
    bool dataAvailable() { return !rxbuf.empty(); }

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
};

class ConsoleListener : public SimObject
{
  protected:
    class Event : public PollEvent
    {
      protected:
        ConsoleListener *listener;

      public:
        Event(ConsoleListener *l, int fd, int e)
            : PollEvent(fd, e), listener(l) {}
        void process(int revent);
    };

    friend class Event;
    Event *event;

    typedef std::list<SimConsole *> list_t;
    typedef list_t::iterator iter_t;
    list_t ConsoleList;

  protected:
    ListenSocket listener;

  public:
    ConsoleListener(const std::string &name);
    ~ConsoleListener();

    void add(SimConsole *cons);

    void accept();
    void listen(int port);
};

#endif // __CONSOLE_HH__
