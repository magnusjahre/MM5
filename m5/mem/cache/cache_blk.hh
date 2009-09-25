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

/** @file
 * Definitions of a simple cache block class.
 */

#ifndef __CACHE_BLK_HH__
#define __CACHE_BLK_HH__

#include "sim/root.hh"		// for Tick
#include "targetarch/isa_traits.hh"	// for Addr
#include "cpu/exec_context.hh"
#include "sim/serialize.hh"

/**
 * Cache block status bit assignments
 */
enum CacheBlkStatusBits {
	/** valid, readable */
	BlkValid =		0x01,
	/** write permission */
	BlkWritable =	0x02,
	/** dirty (modified) */
	BlkDirty =		0x04,
	/** compressed */
	BlkCompressed =	0x08,
	/** block was referenced */
	BlkReferenced =	0x10,
	/** block was a hardware prefetch yet unaccessed*/
	BlkHWPrefetched =	0x20
};

enum DirectoryState {
	DirInvalid = 1,
	DirUnOwned = 2,
	DirOwnedExGR = 3,
	DirOwnedNonExGR = 4,
	DirOwnedExDW = 5,
	DirOwnedNonExDW = 6,
	DirNoState = 7
};

/**
 * A Basic Cache block.
 * Contains the tag, status, and a pointer to data.
 */
class CacheBlk
{
public:
	/** The address space ID of this block. */
	int asid;
	/** Data block tag value. */
	Addr tag;
	/**
	 * Contains a copy of the data in this block for easy access. This is used
	 * for efficient execution when the data could be actually stored in
	 * another format (COW, compressed, sub-blocked, etc). In all cases the
	 * data stored here should be kept consistant with the actual data
	 * referenced by this block.
	 */
	uint8_t *data;
	/** the number of bytes stored in this block. */
	int size;

	/** block state: OR of CacheBlkStatusBit */
	typedef unsigned State;

	/** The current status of this block. @sa CacheBlockStatusBits */
	State status;

	/** Directory protocol state */
	DirectoryState dirState;
	int owner;
	bool* presentFlags;

	/** Shared cache owner info */
	int origRequestingCpuID;
	int prevOrigRequestingCpuID;

	/** Which curTick will this block be accessable */
	Tick whenReady;

	/** Save the exec context so that writebacks can use them. */
	ExecContext *xc;

	/**
	 * The set this block belongs to.
	 * @todo Move this into subclasses when we fix CacheTags to use them.
	 */
	int set;

	/** Number of references to this block since it was brought in. */
	int refCount;

	CacheBlk()
	: asid(-1), tag(0), data(0) ,size(0), status(0),
	dirState(DirNoState), owner(-1), presentFlags(NULL), origRequestingCpuID(-1),prevOrigRequestingCpuID(-1),
	whenReady(0), xc(0),
	set(-1), refCount(0)
	{}

	~CacheBlk(){
		if(presentFlags != NULL){
			delete presentFlags;
		}
	}

	/**
	 * Copy the state of the given block into this one.
	 * @param rhs The block to copy.
	 * @return a const reference to this block.
	 */
	const CacheBlk& operator=(const CacheBlk& rhs)
	{
		asid = rhs.asid;
		tag = rhs.tag;
		data = rhs.data;
		size = rhs.size;
		status = rhs.status;
		whenReady = rhs.whenReady;
		xc = rhs.xc;
		set = rhs.set;
		refCount = rhs.refCount;
		return *this;
	}

	/**
	 * Checks the write permissions of this block.
	 * @return True if the block is writable.
	 */
	bool isWritable() const
	{
		const int needed_bits = BlkWritable | BlkValid;
		return (status & needed_bits) == needed_bits;
	}

	/**
	 * Checks that a block is valid (readable).
	 * @return True if the block is valid.
	 */
	bool isValid() const
	{
		return (status & BlkValid) != 0;
	}

	/**
	 * Check to see if a block has been written.
	 * @return True if the block is dirty.
	 */
	bool isModified() const
	{
		return (status & BlkDirty) != 0;
	}

	/**
	 * Check to see if this block contains compressed data.
	 * @return True iF the block's data is compressed.
	 */
	bool isCompressed() const
	{
		return (status & BlkCompressed) != 0;
	}

	/**
	 * Check if this block has been referenced.
	 * @return True if the block has been referenced.
	 */
	bool isReferenced() const
	{
		return (status & BlkReferenced) != 0;
	}

	/**
	 * Check if this block was the result of a hardware prefetch, yet to
	 * be touched.
	 * @return True if the block was a hardware prefetch, unaccesed.
	 */
	bool isPrefetch() const
	{
		return (status & BlkHWPrefetched) != 0;
	}

	bool isOwnedExclusiveGR(){
		return (dirState == DirOwnedExGR);
	}

	bool isDirInvalid(){
		return (dirState == DirInvalid);
	}

	bool isOwnedNonExclusiveGR(){
		return (dirState == DirOwnedNonExGR);
	}

	void initPresentFlags(int cpu_count){
		presentFlags = new bool[cpu_count];
		for(int i=0;i<cpu_count;i++){
			presentFlags = false;
		}
	}

	void serialize(std::ofstream &outfile){
//		SERIALIZE_SCALAR(asid);
//		SERIALIZE_SCALAR(tag);
//		SERIALIZE_SCALAR(status);
//		SERIALIZE_SCALAR(origRequestingCpuID);
//		SERIALIZE_SCALAR(prevOrigRequestingCpuID);
//		SERIALIZE_SCALAR(set);

		Serializable::writeEntry(&asid, sizeof(int), outfile);
		Serializable::writeEntry(&tag, sizeof(Addr), outfile);
		Serializable::writeEntry(&status, sizeof(State), outfile);
		Serializable::writeEntry(&origRequestingCpuID, sizeof(int), outfile);
		Serializable::writeEntry(&prevOrigRequestingCpuID, sizeof(int), outfile);
		Serializable::writeEntry(&set, sizeof(int), outfile);

		//TODO: should we handle xc serialization?
	}

	void unserialize(std::ifstream &infile){

//		UNSERIALIZE_SCALAR(asid);
//		UNSERIALIZE_SCALAR(tag);
//		UNSERIALIZE_SCALAR(status);
//		UNSERIALIZE_SCALAR(origRequestingCpuID);
//		UNSERIALIZE_SCALAR(prevOrigRequestingCpuID);
//		UNSERIALIZE_SCALAR(set);

		asid = *((int*) Serializable::readEntry(sizeof(int), infile));
		tag = *((Addr*) Serializable::readEntry(sizeof(Addr), infile));
		status = *((State*) Serializable::readEntry(sizeof(State), infile));
		origRequestingCpuID = *((int*) Serializable::readEntry(sizeof(int), infile));
		prevOrigRequestingCpuID =  *((int*) Serializable::readEntry(sizeof(int), infile));
		set = *((int*) Serializable::readEntry(sizeof(int), infile));

		//TODO: should we handle xc unserialization
	}

};

/**
 * Output a CacheBlk to the given ostream.
 * @param out The stream for the output.
 * @param blk The cache block to print.
 *
 * @return The output stream.
 */
inline std::ostream &
operator<<(std::ostream &out, const CacheBlk &blk)
{
	out << std::hex << std::endl;
	out << "  Asid:           " << blk.asid << std::endl;
	out << "  Tag:            " << blk.tag << std::endl;
	out << "  Status:         " <<  blk.status << std::endl;
	out << "  Set:            " <<  blk.set << std::endl;
	out << "  Requesting CPU: " << blk.origRequestingCpuID << std::endl;
	out << "  Prev req CPU:   " << blk.prevOrigRequestingCpuID << std::endl;

	return(out << std::dec);
}

inline std::ostream &
operator<<(std::ostream &out, DirectoryState cur)
{
	switch(cur){
	case DirInvalid:
		out << "DirInvalid";
		break;
	case DirUnOwned:
		out << "UnOwned";
		break;
	case DirOwnedExGR:
		out << "Owned Exclusive GR";
		break;
	case DirOwnedNonExGR:
		out << "Owned NonExclusive GR";
		break;
	case DirOwnedExDW:
		out << "Owned Exclusive DW";
		break;
	case DirOwnedNonExDW:
		out << "Owned NonExclusive DW";
		break;
	case DirNoState:
		out << "No State";
		break;
	default:
		out << "Unknown State";
	}

	return out;
}

#endif //__CACHE_BLK_HH__
