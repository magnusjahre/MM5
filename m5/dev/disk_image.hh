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

/** @file
 * Disk Image Interfaces
 */

#ifndef __DISK_IMAGE_HH__
#define __DISK_IMAGE_HH__

#include <fstream>

#include "base/hashmap.hh"
#include "sim/sim_object.hh"

#define SectorSize (512)

/**
 * Basic interface for accessing a disk image.
 */
class DiskImage : public SimObject
{
  protected:
    bool initialized;

  public:
    DiskImage(const std::string &name) : SimObject(name), initialized(false) {}
    virtual ~DiskImage() {}

    virtual off_t size() const = 0;

    virtual off_t read(uint8_t *data, off_t offset) const = 0;
    virtual off_t write(const uint8_t *data, off_t offset) = 0;
};

/**
 * Specialization for accessing a raw disk image
 */
class RawDiskImage : public DiskImage
{
  protected:
    mutable std::fstream stream;
    std::string file;
    bool readonly;
    mutable off_t disk_size;

  public:
    RawDiskImage(const std::string &name, const std::string &filename,
		 bool rd_only);
    ~RawDiskImage();

    void close();
    void open(const std::string &filename, bool rd_only = false);

    virtual off_t size() const;

    virtual off_t read(uint8_t *data, off_t offset) const;
    virtual off_t write(const uint8_t *data, off_t offset);
};

/**
 * Specialization for accessing a copy-on-write disk image layer.
 * A copy-on-write(COW) layer must be stacked on top of another disk
 * image layer this layer can be another CowDiskImage, or a
 * RawDiskImage.
 *
 * This object is designed to provide a mechanism for persistant
 * changes to a main disk image, or to provide a place for temporary
 * changes to the image to take place that later may be thrown away.
 */
class CowDiskImage : public DiskImage
{
  public:
    static const int VersionMajor;
    static const int VersionMinor;

  protected:
    struct Sector {
	uint8_t data[SectorSize];
    };
    typedef m5::hash_map<uint64_t, Sector *> SectorTable;

  protected:
    std::string filename;
    DiskImage *child;
    SectorTable *table;

  public:
    CowDiskImage(const std::string &name, DiskImage *kid, int hash_size);
    CowDiskImage(const std::string &name, DiskImage *kid, int hash_size,
		 const std::string &filename, bool read_only);
    ~CowDiskImage();

    void init(int hash_size);
    bool open(const std::string &file);
    void save();
    void save(const std::string &file);
    void writeback();
    void serialize(std::ostream &os);
    void unserialize(Checkpoint *cp, const std::string &section);

    virtual off_t size() const;

    virtual off_t read(uint8_t *data, off_t offset) const;
    virtual off_t write(const uint8_t *data, off_t offset);
};

#endif // __DISK_IMAGE_HH__
