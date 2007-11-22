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

/**
 * @file
 * Declarations of memory commands.
 */

#ifndef __MEM_CMD_HH__
#define __MEM_CMD_HH__

#include <string>
#include <iosfwd>

/**
 * Informational return values for memory functions.
 * These are primarily used for statistics.
 * Memory modules always return MA_HIT.
 */
enum MemAccessResult {
    MA_HIT,
    MA_CACHE_MISS,
    MA_TLB_MISS,
    MA_BAD_ADDRESS,
    MA_MSHRS_FULL,
    MA_TARGET_FULL,
    MA_MISS_PENDING,
    MA_NOT_ISSUED,
    MA_NOT_PREDICTED,
    MA_CANCELED,
    MA_DROPPED,
    MA_FILTERED,
    BA_NO_RESULT,
    BA_SUCCESS,
    BA_BLOCKED_HARD_PREFETCH,
    BA_BLOCKED_PREFETCH,
    BA_BLOCKED,
    NUM_MEM_ACCESS_RESULTS
};


/**
 * Memory commands.
 */
enum MemCmdEnum
{
    InvalidCmd,
    Read,
    Write,
    Soft_Prefetch,
    Hard_Prefetch,
    Writeback,
    Invalidate,
    ReadEx,
    WriteInvalidate,
    Upgrade,
    Copy,
    Squash,
    DirWriteback,
    DirRedirectRead,
    DirOwnerTransfer,
    DirOwnerWriteback,
    DirSharerWriteback,
    DirNewOwnerMulticast,
    NUM_MEM_CMDS
};

/**
 * Memory commands and their associated behaviors.
 */
class MemCmd
{
  public:
    /** The memory command. */
    MemCmdEnum cmd;

    /**
     * Default constructor.
     */
    MemCmd()
	: cmd(InvalidCmd)
    {
    }

    /**
     * Build a MemCmd with given command.
     * @param _cmd The new command.
     */
    MemCmd(MemCmdEnum _cmd)
	: cmd(_cmd)
    {
    }

    /** Assign this command from an MemCmdEnum. */
    MemCmd &operator=(MemCmdEnum rhs) { cmd=rhs; return *this; }
    /** Check for equality with an MemCmdEnum. */
    bool operator==(MemCmdEnum rhs) const { return cmd == rhs; }
    /** Check for non equality with a MemCmdEnum. */
    bool operator!=(MemCmdEnum rhs) const { return cmd != rhs; }

    /** Assign this command from a MemCmd. */
    MemCmd &operator=(MemCmd rhs) { cmd=rhs.cmd; return *this; }
    /** Check for equality with a MemCmd. */
    bool operator==(MemCmd rhs) const { return cmd == rhs.cmd; }
    /** Check for non equality with a MemCmd. */
    bool operator!=(MemCmd rhs) const { return cmd != rhs.cmd; }
    /** Return a reference to this command. */
    operator const MemCmdEnum &() const { return cmd; }

    /** Return true if this command is a read. */
    inline bool isRead() const { return behaviors[cmd] & rd; }
    /** Return true if this command is a write. */
    inline bool isWrite() const { return behaviors[cmd] & wr; }
    /** Return true if this command is an invalidate. */
    inline bool isInvalidate() const { return behaviors[cmd] & in; }
    /** Return true if this command expects no response. */
    inline bool isNoResponse() const { return behaviors[cmd] & nr; }
    
    inline bool isDirectoryMessage() const { return behaviors[cmd] & directory; }

    /** Return the string representation of this command. */
    inline char *toString() const { return strings[cmd]; }
    /** Return the index of this command. */
    inline int toIndex() const { return (int) cmd; }
    /** Return the MemCmdEnum of this comand. */
    MemCmdEnum toEnum() const { return cmd; }

    /** The descriptions for each memory access result @sa MemAccessResult */
    static const char *memAccessDesc[NUM_MEM_ACCESS_RESULTS];
    
  private:
    /** This command is a read. */
    static const int rd   = 0x00000001;
    /** This command is a write. */
    static const int wr   = 0x00000002;
    /** This command is a prefetch. */
    static const int pf   = 0x00000004;
    /** This command is an invalidate. */
    static const int in   = 0x00000008;
    /** This command originated in hardware. */
    static const int hw   = 0x00000010;
    /** This command does not expect a response. */
    static const int nr   = 0x00000020;
    
    static const int directory = 0x00000040;

    /** The behaviors for each command. @sa MemCmdEnum */
    static int behaviors[NUM_MEM_CMDS];
    /** The string representation for each command. @sa MemCmdEnum */
    static char *strings[NUM_MEM_CMDS];
};

/**
 * Output a MemCmd to the given ostream.
 * @param out The output stream.
 * @param cmd The command to output.
 * @return The output stream.
 */
std::ostream &operator<<(std::ostream &out, MemCmd cmd);

#endif // __MEM_CMD_H__
