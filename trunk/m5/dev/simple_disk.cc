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
 * Simple disk interface for the system console
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "base/misc.hh"
#include "base/trace.hh"
#include "dev/disk_image.hh"
#include "dev/simple_disk.hh"
#include "mem/functional/physical.hh"
#include "sim/builder.hh"

using namespace std;

SimpleDisk::SimpleDisk(const string &name, PhysicalMemory *pmem,
		       DiskImage *img)
    : SimObject(name), physmem(pmem), image(img)
{}

SimpleDisk::~SimpleDisk()
{}


void
SimpleDisk::read(Addr addr, baddr_t block, int count) const
{
    uint8_t *data = physmem->dma_addr(addr, count);
    if (!data)
	panic("dma out of range! read addr=%#x count=%d\n", addr, count);

    if (count & (SectorSize - 1))
	panic("Not reading a multiple of a sector (count = %d)", count);

    for (int i = 0, j = 0; i < count; i += SectorSize, j++)
	image->read(data + i, block + j);

    DPRINTF(SimpleDisk, "read  block=%#x len=%d\n", (uint64_t)block, count);
    DDUMP(SimpleDiskData, data, count);
}

void
SimpleDisk::write(Addr addr, baddr_t block, int count)
{
    panic("unimplemented!\n");

#if 0
    uint8_t *data = physmem->dma_addr(addr, count);
    if (!data)
	panic("dma out of range! write addr=%#x count=%d\n", addr, count);

    image->write(data, block, count);
#endif
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(SimpleDisk)

    SimObjectParam<PhysicalMemory *> physmem;
    SimObjectParam<DiskImage *> disk;

END_DECLARE_SIM_OBJECT_PARAMS(SimpleDisk)

BEGIN_INIT_SIM_OBJECT_PARAMS(SimpleDisk)

    INIT_PARAM(physmem, "Physical Memory"),
    INIT_PARAM(disk, "Disk Image")

END_INIT_SIM_OBJECT_PARAMS(SimpleDisk)

CREATE_SIM_OBJECT(SimpleDisk)
{
    return new SimpleDisk(getInstanceName(), physmem, disk);
}

REGISTER_SIM_OBJECT("SimpleDisk", SimpleDisk)
