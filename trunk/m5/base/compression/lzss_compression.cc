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
 * LZSSCompression definitions.
 */

#include <assert.h>

#include "base/compression/lzss_compression.hh"

#include "base/misc.hh" //for fatal

void
LZSSCompression::findSubString(uint8_t *src, int back, int size, uint16_t &L, 
			       uint16_t &P)
{
    int front = 0;
    int max_length = size - back;
    L = 0;
    P = back - 1;
    while (front < back) {
	while (src[front] != src[back] && front < back) ++front;
	if (front >= back) {
	    return;
	}
	int i = 1;
	while (src[front+i] == src[back+i] && i < max_length) ++i;
	if (i >= L) {
	    L = i;
	    P = front;
	}
	if (src[front+i] != src[back+i-1]) {
	    // can't find a longer substring until past this point.
	    front += i;
	} else {
	    ++front;
	}
    }    
}

int
LZSSCompression::emitByte(uint8_t *dest, uint8_t byte)
{
    if ((byte >> 5 & 0x7) == 0 || (byte >> 5 & 0x7) == 7) {
	// If the top 3 bits are the same, emit 00<6bits>
	dest[0] = byte & 0x3f;
	return 1;
    } else {
	// emit 01XXXXXX <8 bits>
	dest[0] = 0x40;
	dest[1] = byte;
	return 2;
    }
}

void
LZSSCompression::emitString(uint8_t *dest, uint16_t P, uint16_t L)
{
    // Emit 1<7P> <5P><3L> <8L>
    dest[0] = 1<<7 | (P >> 5 & 0x7f);
    dest[1] = ((P & 0x1f) << 3) | (L>>8 & 0x3);
    dest[2] = L & 0xFF;
}

int
LZSSCompression::compress(uint8_t *dest, uint8_t *src, int size)
{
    if (size > 4096) {
	fatal("Compression can only handle block sizes of 4096 bytes or less");
    }

    // Encode the first byte.
    int dest_index = emitByte(dest, src[0]);
    int i = 1;
    // A 11 bit field
    uint16_t L;
    // A 12 bit field
    uint16_t P = 0;

    while (i < size && dest_index < size) {
	L = 0;

	if (dest_index+3 >= size) {
	    dest_index = size;
	    continue;
	}

	if (i == size - 1) {
	    // Output the character
	    dest_index += emitByte(&dest[dest_index], src[i]);
	    ++i;
	    continue;
	}
	findSubString(src, i, size, L, P);
	if (L > 1) {
	    // Output the string reference
	    emitString(&dest[dest_index], P, L);
	    dest_index += 3;
	    i = i+L;
	} else {
	    // Output the character
	    dest_index += emitByte(&dest[dest_index], src[i]);
	    ++i;
	}
    }

    if (dest_index >= size) {
	// Have expansion instead of compression, just copy.
	memcpy(dest,src,size);
	return size;
    }
    return dest_index;
}

int
LZSSCompression::uncompress(uint8_t *dest, uint8_t *src, int size)
{
    int index = 0;
    int i = 0;
    while (i < size) {
	if (src[i] & 1<<7 ) {
	    // We have a string
	    // Extract P
	    int start = (src[i] & 0x3f)<<5 | ((src[i+1] >> 3) & 0x1f);
	    // Extract L
	    int len = (src[i+1] & 0x07)<<8 | src[i+2];
	    i += 3;
	    for (int j = start; j < start+len; ++j) {
		dest[index++] = dest[j];
	    }
	} else {
	    // We have a character
	    if (src[i] & 1<<6) {
		// Value is in the next byte
		dest[index++] = src[i+1];
		i += 2;
	    } else {
		// just extend the lower 6 bits
		dest[index++] = (src[i] & 0x3f) | ((src[i] & 1<<5) ? 0xC0 : 0);
		++i;
	    }
	}
    }
    return index;
}
