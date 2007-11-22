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

#ifndef __LZSS_COMPRESSION_HH__
#define __LZSS_COMPRESSION_HH__

/** @file
 * LZSSCompression declarations.
 */

#include "sim/host.hh" // for uint8_t

/**
 * Simple LZSS compression scheme.
 */
class LZSSCompression
{
    /**
     * Finds the longest substring for the given offset.
     * @param src The source block that we search for substrings.
     * @param back The larger offset.
     * @param size The size of the source block.
     * @param L The length of the largest substring.
     * @param P The starting offset of the largest substring.
     */
    void findSubString(uint8_t *src, int back, int size, uint16_t &L, 
		       uint16_t &P);

    /**
     * Emit an encoded byte to the compressed data array. If the 2 high
     * order bits can be signed extended, use 1 byte encoding, if not use 2
     * bytes.
     * @param dest The compressed data.
     * @param byte The byte to emit.
     * @return The number of bytes used to encode.
     */
    int emitByte(uint8_t *dest, uint8_t byte);

    /**
     * Emit a string reference to the compressed data array. A string reference
     * always uses 3 bytes. 1 flag bit, 12 bits for the starting position, and
     * 11 bits for the length of the string. This allows compression of 4096
     * byte blocks with string lengths of up to 2048 bytes.
     * @param dest The compressed data.
     * @param P The starting position in the uncompressed data.
     * @param L The length in bytes of the string.
     */
    void emitString(uint8_t *dest, uint16_t P, uint16_t L);

  public:
    /**
     * Compresses the source block and stores it in the destination block. If
     * the compressed block grows to larger than the source block, it aborts
     * and just performs a copy.
     * @param dest The destination block.
     * @param src The block to be compressed.
     * @param size The size of the source block.
     * @return The size of the compressed block.
     *
     * @pre Destination has enough storage to hold the compressed block.
     */
    int compress(uint8_t *dest, uint8_t *src, int size);

    /**
     * Unompresses the source block and stores it in the destination block.
     * @param dest The destination block.
     * @param src The block to be uncompressed.
     * @param size The size of the source block.
     * @return The size of the uncompressed block.
     *
     * @pre Destination has enough storage to hold the uncompressed block.
     */
    int uncompress(uint8_t *dest, uint8_t *src, int size);
};

#endif //__LZSS_COMPRESSION_HH__
