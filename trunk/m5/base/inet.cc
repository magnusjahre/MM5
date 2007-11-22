/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#include <cstdio>
#include <sstream>
#include <string>

#include "base/cprintf.hh"
#include "sim/host.hh"
#include "base/inet.hh"

using namespace std;
namespace Net {

EthAddr::EthAddr()
{
    memset(data, 0, ETH_ADDR_LEN);
}

EthAddr::EthAddr(const uint8_t ea[ETH_ADDR_LEN])
{
    *data = *ea;
}

EthAddr::EthAddr(const eth_addr &ea)
{
    *data = *ea.data;
}

EthAddr::EthAddr(const std::string &addr)
{
    parse(addr);
}

const EthAddr &
EthAddr::operator=(const eth_addr &ea)
{
    *data = *ea.data;
    return *this;
}

const EthAddr &
EthAddr::operator=(const std::string &addr)
{
    parse(addr);
    return *this;
}

void
EthAddr::parse(const std::string &addr)
{
    // the hack below is to make sure that ETH_ADDR_LEN is 6 otherwise
    // the sscanf function won't work.
    int bytes[ETH_ADDR_LEN == 6 ? ETH_ADDR_LEN : -1];
    if (sscanf(addr.c_str(), "%x:%x:%x:%x:%x:%x", &bytes[0], &bytes[1],
	       &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != ETH_ADDR_LEN) {
	memset(data, 0xff, ETH_ADDR_LEN);
	return;
    }

    for (int i = 0; i < ETH_ADDR_LEN; ++i) {
	if (bytes[i] & ~0xff) {
	    memset(data, 0xff, ETH_ADDR_LEN);
	    return;
	}
	
	data[i] = bytes[i];
    }
}

string
EthAddr::string() const
{
    stringstream stream;
    stream << *this;
    return stream.str();
}

bool
operator==(const EthAddr &left, const EthAddr &right)
{
    return memcmp(left.bytes(), right.bytes(), ETH_ADDR_LEN);
}

ostream &
operator<<(ostream &stream, const EthAddr &ea)
{
    const uint8_t *a = ea.addr();
    ccprintf(stream, "%x:%x:%x:%x:%x:%x", a[0], a[1], a[2], a[3], a[4], a[5]);
    return stream;
}

uint16_t
cksum(const IpPtr &ptr)
{
    int sum = ip_cksum_add(ptr->bytes(), ptr->hlen(), 0);
    return ip_cksum_carry(sum);
}

uint16_t
__tu_cksum(const IpPtr &ip)
{
    int tcplen = ip->len() - ip->hlen();
    int sum = ip_cksum_add(ip->payload(), tcplen, 0);
    sum = ip_cksum_add(&ip->ip_src, 8, sum); // source and destination
    sum += htons(ip->ip_p + tcplen);
    return ip_cksum_carry(sum);
}

uint16_t
cksum(const TcpPtr &tcp)
{ return __tu_cksum(IpPtr(tcp.packet())); }

uint16_t
cksum(const UdpPtr &udp)
{ return __tu_cksum(IpPtr(udp.packet())); }

bool
IpHdr::options(vector<const IpOpt *> &vec) const
{
    vec.clear();

    const uint8_t *data = bytes() + sizeof(struct ip_hdr);
    int all = hlen() - sizeof(struct ip_hdr);
    while (all > 0) {
	const IpOpt *opt = (const IpOpt *)data;
	int len = opt->len();
	if (all < len)
	    return false;

	vec.push_back(opt);
	all -= len;
	data += len;
    }

    return true;
}

bool
TcpHdr::options(vector<const TcpOpt *> &vec) const
{
    vec.clear();

    const uint8_t *data = bytes() + sizeof(struct tcp_hdr);
    int all = off() - sizeof(struct tcp_hdr);
    while (all > 0) {
	const TcpOpt *opt = (const TcpOpt *)data;
	int len = opt->len();
	if (all < len)
	    return false;

	vec.push_back(opt);
	all -= len;
	data += len;
    }

    return true;
}

bool
TcpOpt::sack(vector<SackRange> &vec) const
{
    vec.clear();

    const uint8_t *data = bytes() + sizeof(struct tcp_hdr);
    int all = len() - offsetof(tcp_opt, opt_data.sack);
    while (all > 0) {
	const uint16_t *sack = (const uint16_t *)data;
	int len = sizeof(uint16_t) * 2;
	if (all < len) {
	    vec.clear();
	    return false;
	}

	vec.push_back(RangeIn(ntohs(sack[0]), ntohs(sack[1])));
	all -= len;
	data += len;
    }

    return false;
}

/* namespace Net */ }
