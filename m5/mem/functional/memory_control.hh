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
 */

#ifndef __MEMORY_CONTROL_HH__
#define __MEMORY_CONTROL_HH__

#include "base/intmath.hh"
#include "base/range.hh"
#include "base/trace.hh"
#include "mem/functional/functional.hh"

/*
 * Memory controller that takes physical address accesses and passes
 * them to the correct destination
 */
class MemoryController : public FunctionalMemory
{
  private:
    struct devmap {
	FunctionalMemory *obj;
	Range<Addr> range;
    };

    int mapsize;
    int mapcap;
    devmap *map;

  protected:
    devmap *find_child(MemReqPtr &req) const;

  public:
    virtual bool badaddr(Addr paddr) const;

  public:
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    virtual Fault read(MemReqPtr &req, uint8_t &data);
    virtual Fault read(MemReqPtr &req, uint16_t &data);
    virtual Fault read(MemReqPtr &req, uint32_t &data);
    virtual Fault read(MemReqPtr &req, uint64_t &data);

    virtual Fault write(MemReqPtr &req, uint8_t data);
    virtual Fault write(MemReqPtr &req, uint16_t data);
    virtual Fault write(MemReqPtr &req, uint32_t data);
    virtual Fault write(MemReqPtr &req, uint64_t data);

  public:
    MemoryController(const std::string &name, int cap);
    ~MemoryController();
    void add_child(FunctionalMemory *obj, const Range<Addr> &range);
    void update_child(FunctionalMemory *obj, const Range<Addr> &oldRange,
                      const Range<Addr> &newRange);

    void dump() const;

    virtual void serialize(std::ostream &os) { /* None needed */ }
    virtual void unserialize(Checkpoint *cp, const std::string &section) {}
};

#define MUX_MEMORY(FUNC, ARGS...)			\
    devmap *child = find_child(req);			\
    if (!child)						\
        return Machine_Check_Fault;			\
    return child->obj->FUNC(req, ARGS)

inline Fault
MemoryController::read(MemReqPtr &req, uint8_t *data)
{ MUX_MEMORY(read, data); }

inline Fault
MemoryController::write(MemReqPtr &req, const uint8_t *data)
{ MUX_MEMORY(write, data); }

inline Fault
MemoryController::read(MemReqPtr &req, uint8_t &data)
{ MUX_MEMORY(read, data); }

inline Fault
MemoryController::read(MemReqPtr &req, uint16_t &data)
{ MUX_MEMORY(read, data); }

inline Fault
MemoryController::read(MemReqPtr &req, uint32_t &data)
{ MUX_MEMORY(read, data); }

inline Fault
MemoryController::read(MemReqPtr &req, uint64_t &data)
{ MUX_MEMORY(read, data); }

inline Fault
MemoryController::write(MemReqPtr &req, uint8_t data)
{ MUX_MEMORY(write, data); }

inline Fault
MemoryController::write(MemReqPtr &req, uint16_t data)
{ MUX_MEMORY(write, data); }

inline Fault
MemoryController::write(MemReqPtr &req, uint32_t data)
{ MUX_MEMORY(write, data); }

inline Fault
MemoryController::write(MemReqPtr &req, uint64_t data)
{ MUX_MEMORY(write, data); }

#endif // __MEMORY_CONTROL_HH__
