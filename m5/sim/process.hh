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

#ifndef __PROCESS_HH__
#define __PROCESS_HH__

//
// The purpose of this code is to fake the loader & syscall mechanism
// when there's no OS: thus there's no reason to use it in FULL_SYSTEM
// mode when we do have an OS.
//
#include "config/full_system.hh"

#if !FULL_SYSTEM

#include <vector>

#include "targetarch/isa_traits.hh"
#include "sim/sim_object.hh"
#include "sim/stats.hh"
#include "base/statistics.hh"
#include "base/trace.hh"

class ExecContext;
class FunctionalMemory;

struct AuxVector
{
    uint64_t a_type;
    uint64_t a_val;

    AuxVector()
    {}

    AuxVector(uint64_t type, uint64_t val){
    	a_type = type;
    	a_val = val;
    }
};

enum AuxiliaryVectorType {
    M5_AT_NULL = 0,
    M5_AT_IGNORE = 1,
    M5_AT_EXECFD = 2,
    M5_AT_PHDR = 3,
    M5_AT_PHENT = 4,
    M5_AT_PHNUM = 5,
    M5_AT_PAGESZ = 6,
    M5_AT_BASE = 7,
    M5_AT_FLAGS = 8,
    M5_AT_ENTRY = 9,
    M5_AT_NOTELF = 10,
    M5_AT_UID = 11,
    M5_AT_EUID = 12,
    M5_AT_GID = 13,
    M5_AT_EGID = 14,
    // The following may be specific to Linux
    M5_AT_PLATFORM = 15,
    M5_AT_HWCAP = 16,
    M5_AT_CLKTCK = 17,

    M5_AT_SECURE = 23,
    M5_BASE_PLATFORM = 24,
    M5_AT_RANDOM = 25,

    M5_AT_EXECFN = 31,

    M5_AT_VECTOR_SIZE = 44
};

class Process : public SimObject
{
  public:

    // have we initialized an execution context from this process?  If
    // yes, subsequent contexts are assumed to be for dynamically
    // created threads and are not initialized.
    bool initialContextLoaded;

    // execution contexts associated with this process
    std::vector<ExecContext *> execContexts;

    // number of CPUs (esxec contexts, really) assigned to this process.
    unsigned int numCpus() { return execContexts.size(); }

    // record of blocked context
    struct WaitRec
    {
	Addr waitChan;
	ExecContext *waitingContext;

	WaitRec(Addr chan, ExecContext *ctx)
	    : waitChan(chan), waitingContext(ctx)
	{
	}
    };

    // list of all blocked contexts
    std::list<WaitRec> waitList;

    RegFile *init_regs;		// initial register contents

    Addr text_base;		// text (code) segment base
    unsigned text_size;		// text (code) size in bytes

    Addr data_base;		// initialized data segment base
    unsigned data_size;		// initialized data + bss size in bytes

    Addr brk_point;		// top of the data segment

    Addr stack_base;		// stack segment base (highest address)
    unsigned stack_size;	// initial stack size
    Addr stack_min;		// lowest address accessed on the stack

    // addr to use for next stack region (for multithreaded apps)
    Addr next_thread_stack_base;

    // Base of region for mmaps (when user doesn't specify an address).
    Addr mmap_start;
    Addr mmap_end;

    // Base of region for nxm data
    Addr nxm_start;
    Addr nxm_end;

    std::string prog_fname;	// file name
    Addr prog_entry;		// entry point (initial PC)

    Stats::Scalar<> num_syscalls;	// number of syscalls executed

    class FileParameters{
    public:

    	std::string path;
    	int hostFlags;
    	int mode;

    	FileParameters()
    	: hostFlags(0), mode(0) {}

    	FileParameters(std::string _path, int _hostFlags, int _mode)
    	: path(_path), hostFlags(_hostFlags), mode(_mode) {}

    };

    Checkpoint* currentCheckpoint;

  protected:
    // constructor
    Process(const std::string &nm,
	    int stdin_fd, 	// initial I/O descriptors
	    int stdout_fd,
	    int stderr_fd,
	    int _memSizeMB,
	    int _cpuID,
	    int _victimEntries);

    // post initialization startup
    virtual void startup();

  protected:
    FunctionalMemory *memory;

  private:
    // file descriptor remapping support
    static const int MAX_FD = 100000;	// max legal fd value
    int fd_map[MAX_FD+1];
    int cpuID;

    std::map<int, FileParameters> tgtFDFileParams;

    std::string generateFileStateName(const char* prefix, int tgt_fd);

    void cleanFileState(Checkpoint *cp, const std::string &section);

    void copyFile(std::string fromPath, std::string toPath);

  public:
    // static helper functions to generate file descriptors for constructor
    static int openInputFile(const std::string &filename);
    static int openOutputFile(const std::string &filename);

    // override of virtual SimObject method: register statistics
    virtual void regStats();

    // register an execution context for this process.
    // returns xc's cpu number (index into execContexts[])
    int registerExecContext(ExecContext *xc);


    void replaceExecContext(ExecContext *xc, int xcIndex);

    // map simulator fd sim_fd to target fd tgt_fd
    void dup_fd(int sim_fd, int tgt_fd);

    // generate new target fd for sim_fd
    int open_fd(int sim_fd, FileParameters params);

    bool close_fd(int tgt_fd);

    // look up simulator fd for given target fd
    int sim_fd(int tgt_fd);

    // is this a valid instruction fetch address?
    bool validInstAddr(Addr addr)
    {
	return (text_base <= addr &&
		addr < text_base + text_size &&
		!(addr & (sizeof(MachInst)-1)));
    }

    // is this a valid address? (used to filter data fetches)
    // note that we just assume stack size <= 16MB
    // this may be alpha-specific
    bool validDataAddr(Addr addr){

    	return ((data_base <= addr && addr < brk_point) ||
    			(next_thread_stack_base <= addr && addr < stack_base) ||
    			(text_base <= addr && addr < (text_base + text_size)) ||
    			(mmap_start <= addr && addr < mmap_end) ||
    			(nxm_start <= addr && addr < nxm_end));
    }

    virtual void syscall(ExecContext *xc) = 0;

    virtual FunctionalMemory *getMemory() { return memory; }

    virtual void serialize(std::ostream &os);

    virtual void unserialize(Checkpoint *cp, const std::string &section);

    void remap(Addr vaddr, int64_t size, Addr new_vaddr);

    void clearMemory(Addr fromAddr, Addr toAddr);
};

//
// "Live" process with system calls redirected to host system
//
class ObjectFile;
class LiveProcess : public Process
{
  protected:
    LiveProcess(const std::string &nm, ObjectFile *objFile,
		int stdin_fd, int stdout_fd, int stderr_fd,
		std::vector<std::string> &argv,
		std::vector<std::string> &envp,
		int _memSizeMB,
		int _cpuID,
		int _victimEntries);

  public:
    // this function is used to create the LiveProcess object, since
    // we can't tell which subclass of LiveProcess to use until we
    // open and look at the object file.
    static LiveProcess *create(const std::string &nm,
			       int stdin_fd, int stdout_fd, int stderr_fd,
			       std::string executable,
			       std::vector<std::string> &argv,
			       std::vector<std::string> &envp,
			       int _maxMemMB,
			       int _cpuID,
			       int _victimEntries);
};


#endif // !FULL_SYSTEM

#endif // __PROCESS_HH__
