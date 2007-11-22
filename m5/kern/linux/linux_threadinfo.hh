/*
 * Copyright (c) 2004
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


#ifndef __KERN_LINUX_LINUX_TREADNIFO_HH__
#define __KERN_LINUX_LINUX_TREADNIFO_HH__

#include "kern/linux/thread_info.hh"
#include "kern/linux/sched.hh"
#include "targetarch/vptr.hh"

namespace Linux {

class ThreadInfo
{
  private:
    ExecContext *xc;

  public:
    ThreadInfo(ExecContext *exec) : xc(exec) {}
    ~ThreadInfo() {}

    inline VPtr<thread_info>
    curThreadInfo()
    {
        Addr current;

        /* Each kernel stack is only 2 pages, the start of which is the
         * thread_info struct. So we can get the address by masking off
         * the lower 14 bits.
         */
        current = xc->regs.intRegFile[StackPointerReg] & ~0x3fff;
        return VPtr<thread_info>(xc, current);
    }

    inline VPtr<task_struct>
    curTaskInfo()
    {
        Addr task = curThreadInfo()->task;
        return VPtr<task_struct>(xc, task);
    }

    std::string
    curTaskName()
    {
        return curTaskInfo()->name;
    }

    int32_t
    curTaskPID()
    {
        return curTaskInfo()->pid;
    }

    uint64_t
    curTaskStart()
    {
        return curTaskInfo()->start;
    }
};

/* namespace Linux */ }

#endif // __KERN_LINUX_LINUX_THREADINFO_HH__
