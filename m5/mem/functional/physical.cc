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

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include <cstdio>
#include <iostream>
#include <string>


#include "base/misc.hh"
#include "config/full_system.hh"
#if FULL_SYSTEM
#include "mem/functional/memory_control.hh"
#endif
#include "mem/functional/physical.hh"
#include "sim/host.hh"
#include "sim/builder.hh"
#include "targetarch/isa_traits.hh"

using namespace std;

PhysicalMemory::PhysicalMemory(const string &n, Range<Addr> range,
			       MemoryController *mmu, const std::string &fname)
    : FunctionalMemory(n), base_addr(range.start), pmem_size(range.size()),
      pmem_addr(NULL)
{
    if (pmem_size % TheISA::PageBytes != 0)
        panic("Memory Size not divisible by page size\n");

    mmu->add_child(this, range);

    int fd = -1;

    if (!fname.empty()) {
	fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
	    perror("open");
	    fatal("Could not open physical memory file: %s\n", fname);
	}
	ftruncate(fd, pmem_size);
    }

    int map_flags = (fd == -1) ? (MAP_ANON | MAP_PRIVATE) : MAP_SHARED;
    pmem_addr = (uint8_t *)mmap(NULL, pmem_size, PROT_READ | PROT_WRITE,
				map_flags, fd, 0);

    if (fd != -1)
	close(fd);

    if (pmem_addr == (void *)MAP_FAILED) {
	perror("mmap");
	fatal("Could not mmap!\n");
    }
}

PhysicalMemory::~PhysicalMemory()
{
    if (pmem_addr)
	munmap(pmem_addr, pmem_size);
}

//
// little helper for better prot_* error messages
//
void
PhysicalMemory::prot_access_error(Addr addr, int size, const string &func)
{
    panic("invalid physical memory access!\n"
          "%s: %s(addr=%#x, size=%d) out of range (max=%#x)\n",
	  name(), func, addr, size, pmem_size - 1);
}

void
PhysicalMemory::prot_read(Addr addr, uint8_t *p, int size)
{
    mem_addr_test(addr);
    if (addr + size >= pmem_size)
	prot_access_error(addr, size, "prot_read");

    memcpy(p, pmem_addr + addr - base_addr, size);
}

void
PhysicalMemory::prot_write(Addr addr, const uint8_t *p, int size)
{
    mem_addr_test(addr, p);
    if (addr + size >= pmem_size)
	prot_access_error(addr, size, "prot_write");

    memcpy(pmem_addr + addr - base_addr, p, size);
}

void
PhysicalMemory::prot_memset(Addr addr, uint8_t val, int size)
{
    mem_addr_test(addr, 0);
    if (addr + size >= pmem_size)
	prot_access_error(addr, size, "prot_memset");

    memset(pmem_addr + addr - base_addr, val, size);
}

Fault
PhysicalMemory::read(MemReqPtr &req, uint8_t *data)
{
    mem_addr_test(req->paddr);
    off_t offset = req->paddr - base_addr;
    if (offset + req->size >= pmem_size)
	return Machine_Check_Fault;

    memcpy(data, pmem_addr + offset, req->size);
    return No_Fault;
}

Fault
PhysicalMemory::write(MemReqPtr &req, const uint8_t *data)
{
    mem_addr_test(req->paddr, &data);
    off_t offset = req->paddr - base_addr;
    if (offset + req->size >= pmem_size)
	return Machine_Check_Fault;

    memcpy(pmem_addr + offset, data, req->size);
    return No_Fault;
}

Fault
PhysicalMemory::read(MemReqPtr &req, uint8_t &data)
{
    mem_block_test(req->paddr);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint8_t) > pmem_size)
	return Machine_Check_Fault;

    data = *(uint8_t *)(pmem_addr + offset);
    return No_Fault;
}

Fault
PhysicalMemory::read(MemReqPtr &req, uint16_t &data)
{
    mem_block_test(req->paddr);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint16_t) > pmem_size)
	return Machine_Check_Fault;

    data = *(uint16_t *)(pmem_addr + offset);
    return No_Fault;
}

Fault
PhysicalMemory::read(MemReqPtr &req, uint32_t &data)
{
    mem_block_test(req->paddr);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint32_t) > pmem_size)
	return Machine_Check_Fault;

    data = *(uint32_t *)(pmem_addr + offset);
    return No_Fault;
}

Fault
PhysicalMemory::read(MemReqPtr &req, uint64_t &data)
{
    mem_block_test(req->paddr);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint64_t) > pmem_size)
	return Machine_Check_Fault;

    data = *(uint64_t *)(pmem_addr + offset);
    return No_Fault;
}

Fault
PhysicalMemory::write(MemReqPtr &req, uint8_t data)
{
    mem_block_test(req->paddr, (uint8_t *)&data);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint8_t) > pmem_size)
	return Machine_Check_Fault;

    *(uint8_t *)(pmem_addr + offset) = data;
    return No_Fault;
}

Fault
PhysicalMemory::write(MemReqPtr &req, uint16_t data)
{
    mem_block_test(req->paddr, (uint8_t *)&data);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint16_t) > pmem_size)
	return Machine_Check_Fault;

    *(uint16_t *)(pmem_addr + offset) = data;
    return No_Fault;
}

Fault
PhysicalMemory::write(MemReqPtr &req, uint32_t data)
{
    mem_block_test(req->paddr, (uint8_t *)&data);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint32_t) > pmem_size)
	return Machine_Check_Fault;

    *(uint32_t *)(pmem_addr + offset) = data;
    return No_Fault;
}

Fault
PhysicalMemory::write(MemReqPtr &req, uint64_t data)
{
    mem_block_test(req->paddr, (uint8_t *)&data);
    off_t offset = req->paddr - base_addr;
    if (offset + sizeof(uint64_t) > pmem_size)
	return Machine_Check_Fault;

    *(uint64_t *)(pmem_addr + offset) = data;
    return No_Fault;
}

void
PhysicalMemory::serialize(ostream &os)
{
    gzFile compressedMem;
    string filename = name() + ".physmem";

    SERIALIZE_SCALAR(pmem_size);
    SERIALIZE_SCALAR(filename);

    // write memory file
    string thefile = Checkpoint::dir() + "/" + filename.c_str();
    int fd = creat(thefile.c_str(), 0664);
    if (fd < 0) {
	perror("creat");
	fatal("Can't open physical memory checkpoint file '%s'\n", filename);
    }

    compressedMem = gzdopen(fd, "wb");
    if (compressedMem == NULL)
        fatal("Insufficient memory to allocate compression state for %s\n",
                filename);
    
    if (gzwrite(compressedMem, pmem_addr, pmem_size) != pmem_size) {
	fatal("Write failed on physical memory checkpoint file '%s'\n",
	      filename);
    }

    if (gzclose(compressedMem))
    	fatal("Close failed on physical memory checkpoint file '%s'\n",
	      filename);
}

void
PhysicalMemory::unserialize(Checkpoint *cp, const string &section)
{
    gzFile compressedMem;
    long *tempPage;
    long *pmem_current;
    uint64_t curSize;
    uint32_t bytesRead;
    const int chunkSize = 16384;
   

    // unmap file that was mmaped in the constructor
    munmap(pmem_addr, pmem_size);

    string filename;

    UNSERIALIZE_SCALAR(pmem_size);
    UNSERIALIZE_SCALAR(filename);
    
    filename = cp->cptDir + "/" + filename;

    // mmap memoryfile
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
	perror("open");
	fatal("Can't open physical memory checkpoint file '%s'", filename);
    }

    compressedMem = gzdopen(fd, "rb");
    if (compressedMem == NULL)
        fatal("Insufficient memory to allocate compression state for %s\n",
                filename);
    

    pmem_addr = (uint8_t *)mmap(NULL, pmem_size, PROT_READ | PROT_WRITE, 
                                MAP_ANON | MAP_PRIVATE, -1, 0);
    
    if (pmem_addr == (void *)MAP_FAILED) {
	perror("mmap");
	fatal("Could not mmap physical memory!\n");
    }
    
    curSize = 0;
    tempPage = (long*)malloc(chunkSize);
    if (tempPage == NULL)
        fatal("Unable to malloc memory to read file %s\n", filename);
       
    /* Only copy bytes that are non-zero, so we don't give the VM system hell */
    while (curSize < pmem_size) {
        bytesRead = gzread(compressedMem, tempPage, chunkSize);
        if (bytesRead != chunkSize && bytesRead != pmem_size - curSize)
            fatal("Read failed on physical memory checkpoint file '%s'"
                  " got %d bytes, expected %d or %d bytes\n",
                  filename, bytesRead, chunkSize, pmem_size-curSize);
      
        assert(bytesRead % sizeof(long) == 0); 

        for (int x = 0; x < bytesRead/sizeof(long); x++)
        {
             if (*(tempPage+x) != 0) {
                 pmem_current = (long*)(pmem_addr + curSize + x * sizeof(long));
                 *pmem_current = *(tempPage+x);
             }
        }
        curSize += bytesRead;
    }
        
    free(tempPage);

    if (gzclose(compressedMem))
    	fatal("Close failed on physical memory checkpoint file '%s'\n",
	      filename);

}

#if FULL_SYSTEM
BEGIN_DECLARE_SIM_OBJECT_PARAMS(PhysicalMemory)

    Param<string> file;
    SimObjectParam<MemoryController *> mmu;
    Param<Range<Addr> > range;

END_DECLARE_SIM_OBJECT_PARAMS(PhysicalMemory)

BEGIN_INIT_SIM_OBJECT_PARAMS(PhysicalMemory)

    INIT_PARAM_DFLT(file, "memory mapped file", ""),
    INIT_PARAM(mmu, "Memory Controller"),
    INIT_PARAM(range, "Device Address Range")

END_INIT_SIM_OBJECT_PARAMS(PhysicalMemory)

CREATE_SIM_OBJECT(PhysicalMemory)
{
    return new PhysicalMemory(getInstanceName(), range, mmu, file);
}

REGISTER_SIM_OBJECT("PhysicalMemory", PhysicalMemory)

#endif // FULL_SYSTEM
