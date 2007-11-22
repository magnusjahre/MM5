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

#ifndef __REMOTE_GDB_HH__
#define __REMOTE_GDB_HH__

#include <map>

#include "base/kgdb.h"
#include "cpu/pc_event.hh"
#include "base/pollevent.hh"
#include "base/socket.hh"

class System;
class ExecContext;
class PhysicalMemory;

class RemoteGDB
{
  protected:
    class Event : public PollEvent
    {
      protected:
	RemoteGDB *gdb;

      public:
	Event(RemoteGDB *g, int fd, int e);
	void process(int revent);
    };

    friend class Event;
    Event *event;

  protected:
    int fd;
    uint64_t gdbregs[KGDB_NUMREGS];

  protected:
#ifdef notyet
    label_t recover;
#endif
    bool active;
    bool attached;

    System *system;
    PhysicalMemory *pmem;
    ExecContext *context;

  protected:
    uint8_t getbyte();
    void putbyte(uint8_t b);

    int recv(char *data, int len);
    void send(const char *data);

  protected:
    // Machine memory
    bool read(Addr addr, size_t size, char *data);
    bool write(Addr addr, size_t size, const char *data);

    template <class T> T read(Addr addr);
    template <class T> void write(Addr addr, T data);

  public:
    RemoteGDB(System *system, ExecContext *context);
    ~RemoteGDB();

    void replaceExecContext(ExecContext *xc) { context = xc; }

    void attach(int fd);
    void detach();
    bool isattached();

    bool acc(Addr addr, size_t len);
    static int signal(int type);
    bool trap(int type);

  protected:
    void getregs();
    void setregs();

    void clearSingleStep();
    void setSingleStep();

    PCEventQueue *getPcEventQueue();

  protected:
    class HardBreakpoint : public PCEvent
    {
      private:
	RemoteGDB *gdb;

      public:
	int refcount;

      public:
	HardBreakpoint(RemoteGDB *_gdb, Addr addr);
	std::string name() { return gdb->name() + ".hwbkpt"; }

	virtual void process(ExecContext *xc);
    };
    friend class HardBreakpoint;

    typedef std::map<Addr, HardBreakpoint *> break_map_t;
    typedef break_map_t::iterator break_iter_t;
    break_map_t hardBreakMap;

    bool insertSoftBreak(Addr addr, size_t len);
    bool removeSoftBreak(Addr addr, size_t len);
    bool insertHardBreak(Addr addr, size_t len);
    bool removeHardBreak(Addr addr, size_t len);

  protected:
    struct TempBreakpoint {
	Addr	address;		// set here
	MachInst	bkpt_inst;		// saved instruction at bkpt
	int		init_count;		// number of times to skip bkpt
	int		count;			// current count
    };

    TempBreakpoint notTakenBkpt;
    TempBreakpoint takenBkpt;

    void clearTempBreakpoint(TempBreakpoint &bkpt);
    void setTempBreakpoint(TempBreakpoint &bkpt, Addr addr);

  public:
    std::string name();
};

template <class T>
inline T
RemoteGDB::read(Addr addr)
{
    T temp;
    read(addr, sizeof(T), (char *)&temp);
    return temp;
}

template <class T>
inline void
RemoteGDB::write(Addr addr, T data)
{ write(addr, sizeof(T), (const char *)&data); }

class GDBListener
{
  protected:
    class Event : public PollEvent
    {
      protected:
	GDBListener *listener;

      public:
	Event(GDBListener *l, int fd, int e);
	void process(int revent);
    };

    friend class Event;
    Event *event;

  protected:
    ListenSocket listener;
    RemoteGDB *gdb;
    int port;

  public:
    GDBListener(RemoteGDB *g, int p);
    ~GDBListener();

    void accept();
    void listen();
    std::string name();
};

#endif /* __REMOTE_GDB_H__ */
