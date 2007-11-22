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

#ifndef __MBUF_HH__
#define __MBUF_HH__

#include "sim/host.hh"
#include "targetarch/isa_traits.hh"

namespace tru64 {

struct m_hdr {
    Addr	mh_next;	// 0x00
    Addr	mh_nextpkt;	// 0x08
    Addr	mh_data;	// 0x10
    int32_t	mh_len;		// 0x18
    int32_t	mh_type;	// 0x1C
    int32_t	mh_flags;	// 0x20
    int32_t	mh_pad0;	// 0x24
    Addr	mh_foo[4];	// 0x28, 0x30, 0x38, 0x40
};

struct	pkthdr {
    int32_t	len;
    int32_t	protocolSum;
    Addr	rcvif;
};

struct m_ext {
    Addr	ext_buf;	// 0x00
    Addr	ext_free;	// 0x08
    uint32_t	ext_size;	// 0x10
    uint32_t	ext_pad0;	// 0x14
    Addr	ext_arg;	// 0x18
    struct	ext_refq {
	Addr	forw, back;	// 0x20, 0x28
    } ext_ref;
    Addr	uiomove_f;	// 0x30
    int32_t	protocolSum;	// 0x38
    int32_t	bytesSummed;	// 0x3C
    Addr	checksum;	// 0x40
};

struct mbuf {
    struct	m_hdr m_hdr;
    union {
	struct {
	    struct	pkthdr MH_pkthdr;
	    union {
		struct	m_ext MH_ext;
		char	MH_databuf[1];
	    } MH_dat;
	} MH;
	char	M_databuf[1];
    } M_dat;
};

#define m_attr          m_hdr.mh_attr
#define	m_next		m_hdr.mh_next
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

}

#endif // __MBUF_HH__
