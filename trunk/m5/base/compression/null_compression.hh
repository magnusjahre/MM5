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

#ifndef __BASE_COMPRESSION_NULL_COMPRESSION_HH__
#define __BASE_COMPRESSION_NULL_COMPRESSION_HH__

/**
 * @file
 * This file defines a doNothing compression algorithm.
 */

#include "base/misc.hh" // for fatal()
#include "sim/host.hh"


/**
 * A dummy compression class to use when no data compression is desired.
 */
class NullCompression
{
  public:
    /**
     * Uncompress the data, causes a fatal since no data should be compressed.
     * @param dest The output buffer.
     * @param src  The compressed data.
     * @param size The number of bytes in src.
     *
     * @retval The size of the uncompressed data.
     */
    static int uncompress(uint8_t * dest, uint8_t *src, int size)
    {
	fatal("Can't uncompress data");
    }

    /**
     * Compress the data, just returns the source data.
     * @param dest The output buffer.
     * @param src  The data to be compressed.
     * @param size The number of bytes in src.
     *
     * @retval The size of the compressed data.
     */

    static int compress(uint8_t *dest, uint8_t *src, int size)
    {
	memcpy(dest,src,size);
	return size;
    }
};

#endif //__BASE_COMPRESSION_NULL_COMPRESSION_HH__
