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
 * Implements the user interface to a serial console 
 */

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "base/misc.hh"
#include "base/output.hh"
#include "base/socket.hh"
#include "base/trace.hh"
#include "dev/platform.hh"
#include "dev/simconsole.hh"
#include "dev/uart.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////
//
//

SimConsole::Event::Event(SimConsole *c, int fd, int e)
    : PollEvent(fd, e), cons(c)
{
}

void
SimConsole::Event::process(int revent)
{
    if (revent & POLLIN)
        cons->data();
    else if (revent & POLLNVAL)
        cons->detach();
}

SimConsole::SimConsole(const string &name, ostream *os, int num)
    : SimObject(name), event(NULL), number(num), in_fd(-1), out_fd(-1),
      listener(NULL), txbuf(16384), rxbuf(16384), outfile(os)
#if TRACING_ON == 1
      , linebuf(16384)
#endif
{
    if (outfile)
        outfile->setf(ios::unitbuf);
}

SimConsole::~SimConsole()
{
    close();
}

void
SimConsole::close()
{
    if (in_fd != -1)
        ::close(in_fd);

    if (out_fd != in_fd && out_fd != -1)
        ::close(out_fd);
}

void
SimConsole::attach(int in, int out, ConsoleListener *l)
{
    in_fd = in;
    out_fd = out;
    listener = l;

    event = new Event(this, in, POLLIN);
    pollQueue.schedule(event);

    stringstream stream;
    ccprintf(stream, "==== m5 slave console: Console %d ====", number);

    // we need an actual carriage return followed by a newline for the
    // terminal
    stream << "\r\n";

    write((const uint8_t *)stream.str().c_str(), stream.str().size());


    DPRINTFN("attach console %d\n", number);

    txbuf.readall(out);
}

void
SimConsole::detach()
{
    close();
    in_fd = -1;
    out_fd = -1;

    pollQueue.remove(event);

    if (listener) {
        listener->add(this);
        listener = NULL;
    }

    DPRINTFN("detach console %d\n", number);
}

void
SimConsole::data()
{
    uint8_t buf[1024];
    int len;

    len = read(buf, sizeof(buf));
    if (len) {
        rxbuf.write((char *)buf, len);
        // Inform the UART there is data available
        uart->dataAvailable(); 
    }
}

size_t
SimConsole::read(uint8_t *buf, size_t len)
{
    if (in_fd < 0)
        panic("Console not properly attached.\n");

    size_t ret;
    do {
      ret = ::read(in_fd, buf, len);
    } while (ret == -1 && errno == EINTR);


    if (ret < 0)
        DPRINTFN("Read failed.\n");

    if (ret <= 0) {
        detach();
        return 0;
    }

    return ret;
}

// Console output.
size_t
SimConsole::write(const uint8_t *buf, size_t len)
{
    if (out_fd < 0)
        panic("Console not properly attached.\n");

    size_t ret;
    for (;;) {
      ret = ::write(out_fd, buf, len);

      if (ret >= 0)
        break;

      if (errno != EINTR)
      detach();
    }

    return ret;
}

#define MORE_PENDING (ULL(1) << 61)
#define RECEIVE_SUCCESS (ULL(0) << 62)
#define RECEIVE_NONE (ULL(2) << 62)
#define RECEIVE_ERROR (ULL(3) << 62)

bool
SimConsole::in(uint8_t &c)
{
    bool empty, ret;

    empty = rxbuf.empty();
    ret = !empty;
    if (!empty) {
        rxbuf.read((char *)&c, 1);
        empty = rxbuf.empty();
    }

    DPRINTF(ConsoleVerbose, "in: \'%c\' %#02x more: %d, return: %d\n",
            isprint(c) ? c : ' ', c, !empty, ret);

    return ret;
}

uint64_t
SimConsole::console_in()
{
    uint8_t c;
    uint64_t value;

    if (in(c)) {
        value = RECEIVE_SUCCESS | c;
        if (!rxbuf.empty())
            value  |= MORE_PENDING;
    } else {
        value = RECEIVE_NONE;
    }

    DPRINTF(ConsoleVerbose, "console_in: return: %#x\n", value);

    return value;
}

void
SimConsole::out(char c)
{
#if TRACING_ON == 1
    if (DTRACE(Console)) {
        static char last = '\0';

        if (c != '\n' && c != '\r' ||
            last != '\n' && last != '\r') {
            if (c == '\n' || c == '\r') {
                int size = linebuf.size();
                char *buffer = new char[size + 1];
                linebuf.read(buffer, size);
                buffer[size] = '\0';
                DPRINTF(Console, "%s\n", buffer);
                delete [] buffer;
            } else {
                linebuf.write(c);
            }
        }

        last = c;
    }
#endif

    txbuf.write(c);

    if (out_fd >= 0)
        write(c);

    if (outfile)
        outfile->write(&c, 1);

    DPRINTF(ConsoleVerbose, "out: \'%c\' %#02x\n",
            isprint(c) ? c : ' ', (int)c);

}

    
void
SimConsole::serialize(ostream &os)
{
}

void
SimConsole::unserialize(Checkpoint *cp, const std::string &section)
{
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(SimConsole)

    SimObjectParam<ConsoleListener *> listener;
    SimObjectParam<IntrControl *> intr_control;
    Param<string> output;
    Param<bool> append_name;
    Param<int> number;

END_DECLARE_SIM_OBJECT_PARAMS(SimConsole)

BEGIN_INIT_SIM_OBJECT_PARAMS(SimConsole)

    INIT_PARAM(listener, "console listener"),
    INIT_PARAM(intr_control, "interrupt controller"),
    INIT_PARAM(output, "file to dump output to"),
    INIT_PARAM_DFLT(append_name, "append name() to filename", true),
    INIT_PARAM_DFLT(number, "console number", 0)

END_INIT_SIM_OBJECT_PARAMS(SimConsole)

CREATE_SIM_OBJECT(SimConsole)
{
    string filename = output;
    ostream *stream = NULL;

    if (!filename.empty()) {
	if (append_name)
	    filename += "." + getInstanceName();
	stream = simout.find(filename);
    }

    SimConsole *console = new SimConsole(getInstanceName(), stream, number);
    ((ConsoleListener *)listener)->add(console);

    return console;
}

REGISTER_SIM_OBJECT("SimConsole", SimConsole)

////////////////////////////////////////////////////////////////////////
//
//

ConsoleListener::ConsoleListener(const string &name)
    : SimObject(name), event(NULL)
{}

ConsoleListener::~ConsoleListener()
{
    if (event)
        delete event;
}

void
ConsoleListener::Event::process(int revent)
{
    listener->accept();
}

///////////////////////////////////////////////////////////////////////
// socket creation and console attach
//

void
ConsoleListener::listen(int port)
{
    while (!listener.listen(port, true)) {
        DPRINTF(Console,
                ": can't bind address console port %d inuse PID %d\n",
                port, getpid());
        port++;
    }

    ccprintf(cerr, "Listening for console connection on port %d\n", port);

    event = new Event(this, listener.getfd(), POLLIN);
    pollQueue.schedule(event);
}

void
ConsoleListener::add(SimConsole *cons)
{ ConsoleList.push_back(cons);}

void
ConsoleListener::accept()
{
    if (!listener.islistening())
        panic("%s: cannot accept a connection if not listening!", name());

    int sfd = listener.accept(true);
    if (sfd != -1) {
        iter_t i = ConsoleList.begin();
        iter_t end = ConsoleList.end();
        if (i == end) {
            close(sfd);
        } else {
            (*i)->attach(sfd, this);
            i = ConsoleList.erase(i);
        }
    }
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ConsoleListener)

    Param<int> port;

END_DECLARE_SIM_OBJECT_PARAMS(ConsoleListener)

BEGIN_INIT_SIM_OBJECT_PARAMS(ConsoleListener)

    INIT_PARAM_DFLT(port, "listen port", 3456)

END_INIT_SIM_OBJECT_PARAMS(ConsoleListener)

CREATE_SIM_OBJECT(ConsoleListener)
{
    ConsoleListener *listener = new ConsoleListener(getInstanceName());
    listener->listen(port);

    return listener;
}

REGISTER_SIM_OBJECT("ConsoleListener", ConsoleListener)
