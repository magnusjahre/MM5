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

/**
 * @file
 * Defines some debug callouts to make debugging templates easier.
 */

/**
 * Defines empty function calls for easily placing breakpoints in template
 * code. These function calls should be optimized out so shouldn't change
 * performance. Since GDB does not play nice with GCC3* assign important
 * parts of MemReq to local variable so we can see them.
 */
#ifndef __MEM_DEBUG_HH__
#define __MEM_DEBUG_HH__

#ifdef DEBUG
#include "mem/mem_req.hh"

namespace MemDebug
{
   /**
     * Called from cache access functions.
     */
    void cacheAccess(MemReqPtr &req);

    /**
     * Called from cache response functions.
     */
    void cacheResponse(MemReqPtr &req);

    /**
     * Called from cache probe functions.
     */
    void cacheProbe(MemReqPtr &req);

    /**
     * Called from cache startCopy functions.
     */
    void cacheStartCopy(MemReqPtr &req);

    /**
     * Called from cache handleCopy functions.
     */
    void cacheHandleCopy(MemReqPtr &req);
}
#else // NOT DEBUG

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace MemDebug
{
    inline void cacheAccess(MemReqPtr &req) {}
    inline void cacheResponse(MemReqPtr &req) {}
    inline void cacheProbe(MemReqPtr &req) {}
    inline void cacheStartCopy(MemReqPtr &req) {}
    inline void cacheHandleCopy(MemReqPtr &req) {}
}

#endif //DOXYGEN_SHOULD_SKIP_THIS

#endif

#endif // __MEM_DEBUG_HH__
