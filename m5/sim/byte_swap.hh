/*
 * Copyright (c) 2004
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


#ifndef __SIM_BYTE_SWAP_HH__
#define __SIM_BYTE_SWAP_HH__

#include "sim/host.hh"

static inline uint64_t
swp_byte64(uint64_t x)
{
    return  (uint64_t)((((uint64_t)(x) & 0xff) << 56) |                   
            ((uint64_t)(x) & 0xff00ULL) << 40 |                        
            ((uint64_t)(x) & 0xff0000ULL) << 24 |                      
            ((uint64_t)(x) & 0xff000000ULL) << 8 |                   
            ((uint64_t)(x) & 0xff00000000ULL) >> 8 |                  
            ((uint64_t)(x) & 0xff0000000000ULL) >> 24 |                
            ((uint64_t)(x) & 0xff000000000000ULL) >> 40 |              
            ((uint64_t)(x) & 0xff00000000000000ULL) >> 56) ;
}

static inline uint32_t
swp_byte32(uint32_t x)
{
    return  (uint32_t)(((uint32_t)(x) & 0xff) << 24 |                         
            ((uint32_t)(x) & 0xff00) << 8 | ((uint32_t)(x) & 0xff0000) >> 8 | 
            ((uint32_t)(x) & 0xff000000) >> 24);

}

static inline uint16_t
swp_byte16(uint16_t x)
{ 
    return (uint16_t)(((uint16_t)(x) & 0xff) << 8 |
		      ((uint16_t)(x) & 0xff00) >> 8); 
}

#endif // __SIM_BYTE_SWAP_HH__
