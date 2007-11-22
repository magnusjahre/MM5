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

#ifndef __BASE_INET_HH__
#define __BASE_INET_HH__

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "base/range.hh"
#include "dev/etherpkt.hh"
#include "sim/host.hh"

#include "dnet/os.h"
#include "dnet/eth.h"
#include "dnet/ip.h"
#include "dnet/ip6.h"
#include "dnet/addr.h"
#include "dnet/arp.h"
#include "dnet/icmp.h"
#include "dnet/tcp.h"
#include "dnet/udp.h"
#include "dnet/intf.h"
#include "dnet/route.h"
#include "dnet/fw.h"
#include "dnet/blob.h"
#include "dnet/rand.h"

namespace Net {

/*
 * Ethernet Stuff
 */
struct EthAddr : protected eth_addr
{
  protected:
    void parse(const std::string &addr);

  public:
    EthAddr();
    EthAddr(const uint8_t ea[ETH_ADDR_LEN]);
    EthAddr(const eth_addr &ea);
    EthAddr(const std::string &addr);
    const EthAddr &operator=(const eth_addr &ea);
    const EthAddr &operator=(const std::string &addr);
    
    int size() const { return sizeof(eth_addr); }

    const uint8_t *bytes() const { return &data[0]; }
    uint8_t *bytes() { return &data[0]; }

    const uint8_t *addr() const { return &data[0]; }
    bool unicast() const { return data[0] == 0x00; }
    bool multicast() const { return data[0] == 0x01; }
    bool broadcast() const { return data[0] == 0xff; }
    std::string string() const;

    operator uint64_t() const
    {
	uint64_t reg = 0;
	reg |= ((uint64_t)data[0]) << 40;
	reg |= ((uint64_t)data[1]) << 32;
	reg |= ((uint64_t)data[2]) << 24;
	reg |= ((uint64_t)data[3]) << 16;
	reg |= ((uint64_t)data[4]) << 8;
	reg |= ((uint64_t)data[5]) << 0;
	return reg;
    }

};

std::ostream &operator<<(std::ostream &stream, const EthAddr &ea);
bool operator==(const EthAddr &left, const EthAddr &right);

struct EthHdr : public eth_hdr
{
    uint16_t type() const { return ntohs(eth_type); }
    const EthAddr &src() const { return *(EthAddr *)&eth_src; }
    const EthAddr &dst() const { return *(EthAddr *)&eth_dst; }

    int size() const { return sizeof(eth_hdr); }

    const uint8_t *bytes() const { return (const uint8_t *)this; }
    const uint8_t *payload() const { return bytes() + size(); }
    uint8_t *bytes() { return (uint8_t *)this; }
    uint8_t *payload() { return bytes() + size(); }
};

class EthPtr
{
  protected:
    friend class IpPtr;
    PacketPtr p;

  public:
    EthPtr() {}
    EthPtr(const PacketPtr &ptr) : p(ptr) { }

    EthHdr *operator->() { return (EthHdr *)p->data; }
    EthHdr &operator*() { return *(EthHdr *)p->data; }
    operator EthHdr *() { return (EthHdr *)p->data; }

    const EthHdr *operator->() const { return (const EthHdr *)p->data; }
    const EthHdr &operator*() const { return *(const EthHdr *)p->data; }
    operator const EthHdr *() const { return (const EthHdr *)p->data; }

    const EthPtr &operator=(const PacketPtr &ptr) { p = ptr; return *this; }

    const PacketPtr packet() const { return p; }
    PacketPtr packet() { return p; }
    bool operator!() const { return !p; }
    operator bool() const { return p; }
};

/*
 * IP Stuff
 */
struct IpOpt;
struct IpHdr : public ip_hdr
{
    uint8_t  version() const { return ip_v; }
    uint8_t  hlen() const { return ip_hl * 4; }
    uint8_t  tos() const { return ip_tos; }
    uint16_t len() const { return ntohs(ip_len); }
    uint16_t id() const { return ntohs(ip_id); }
    uint16_t frag_flags() const { return ntohs(ip_off) >> 13; }
    uint16_t frag_off() const { return ntohs(ip_off) & 0x1fff; }
    uint8_t  ttl() const { return ip_ttl; }
    uint8_t  proto() const { return ip_p; }
    uint16_t sum() const { return ip_sum; }
    uint32_t src() const { return ntohl(ip_src); }
    uint32_t dst() const { return ntohl(ip_dst); }

    void sum(uint16_t sum) { ip_sum = sum; }

    bool options(std::vector<const IpOpt *> &vec) const;

    int size() const { return hlen(); }
    const uint8_t *bytes() const { return (const uint8_t *)this; }
    const uint8_t *payload() const { return bytes() + size(); }
    uint8_t *bytes() { return (uint8_t *)this; }
    uint8_t *payload() { return bytes() + size(); }
};

class IpPtr
{
  protected:
    friend class TcpPtr;
    friend class UdpPtr;
    PacketPtr p;

    const IpHdr *h() const
    { return (const IpHdr *)(p->data + sizeof(eth_hdr)); }
    IpHdr *h() { return (IpHdr *)(p->data + sizeof(eth_hdr)); }

    void set(const PacketPtr &ptr)
    {
	EthHdr *eth = (EthHdr *)ptr->data;
	if (eth->type() == ETH_TYPE_IP)
	    p = ptr;
	else
	    p = 0;
    }

  public:
    IpPtr() {}
    IpPtr(const PacketPtr &ptr) { set(ptr); }
    IpPtr(const EthPtr &ptr) { set(ptr.p); }
    IpPtr(const IpPtr &ptr) : p(ptr.p) { }

    IpHdr *operator->() { return h(); }
    IpHdr &operator*() { return *h(); }
    operator IpHdr *() { return h(); }

    const IpHdr *operator->() const { return h(); }
    const IpHdr &operator*() const { return *h(); }
    operator const IpHdr *() const { return h(); }

    const IpPtr &operator=(const PacketPtr &ptr) { set(ptr); return *this; }
    const IpPtr &operator=(const EthPtr &ptr) { set(ptr.p); return *this; }
    const IpPtr &operator=(const IpPtr &ptr) { p = ptr.p; return *this; }

    const PacketPtr packet() const { return p; }
    PacketPtr packet() { return p; }
    bool operator!() const { return !p; }
    operator bool() const { return p; }
    operator bool() { return p; }
};

uint16_t cksum(const IpPtr &ptr);

struct IpOpt : public ip_opt
{
    uint8_t type() const { return opt_type; }
    uint8_t typeNumber() const { return IP_OPT_NUMBER(opt_type); }
    uint8_t typeClass() const { return IP_OPT_CLASS(opt_type); }
    uint8_t typeCopied() const { return IP_OPT_COPIED(opt_type); }
    uint8_t len() const { return IP_OPT_TYPEONLY(type()) ? 1 : opt_len; }

    bool isNumber(int num) const { return typeNumber() == IP_OPT_NUMBER(num); }
    bool isClass(int cls) const { return typeClass() == IP_OPT_CLASS(cls); }
    bool isCopied(int cpy) const { return typeCopied() == IP_OPT_COPIED(cpy); }

    const uint8_t *data() const { return opt_data.data8; }
    void sec(ip_opt_data_sec &sec) const;
    void lsrr(ip_opt_data_rr &rr) const;
    void ssrr(ip_opt_data_rr &rr) const;
    void ts(ip_opt_data_ts &ts) const;
    uint16_t satid() const { return ntohs(opt_data.satid); }
    uint16_t mtup() const { return ntohs(opt_data.mtu); }
    uint16_t mtur() const { return ntohs(opt_data.mtu); }
    void tr(ip_opt_data_tr &tr) const;
    const uint32_t *addext() const { return &opt_data.addext[0]; }
    uint16_t rtralt() const { return ntohs(opt_data.rtralt); }
    void sdb(std::vector<uint32_t> &vec) const;
};

/*
 * TCP Stuff
 */
struct TcpOpt;
struct TcpHdr : public tcp_hdr
{
    uint16_t sport() const { return ntohs(th_sport); }
    uint16_t dport() const { return ntohs(th_dport); }
    uint32_t seq() const { return ntohl(th_seq); }
    uint32_t ack() const { return ntohl(th_ack); }
    uint8_t  off() const { return th_off; }
    uint8_t  flags() const { return th_flags & 0x3f; }
    uint16_t win() const { return ntohs(th_win); }
    uint16_t sum() const { return th_sum; }
    uint16_t urp() const { return ntohs(th_urp); }

    void sum(uint16_t sum) { th_sum = sum; }

    bool options(std::vector<const TcpOpt *> &vec) const;

    int size() const { return off(); }
    const uint8_t *bytes() const { return (const uint8_t *)this; }
    const uint8_t *payload() const { return bytes() + size(); }
    uint8_t *bytes() { return (uint8_t *)this; }
    uint8_t *payload() { return bytes() + size(); }
};

class TcpPtr
{
  protected:
    PacketPtr p;
    int off;

    const TcpHdr *h() const { return (const TcpHdr *)(p->data + off); }
    TcpHdr *h() { return (TcpHdr *)(p->data + off); }

    void set(const PacketPtr &ptr, int offset) { p = ptr; off = offset; }
    void set(const IpPtr &ptr)
    {
	if (ptr->proto() == IP_PROTO_TCP)
	    set(ptr.p, sizeof(eth_hdr) + ptr->hlen());
	else
	    set(0, 0);
    }

  public:
    TcpPtr() {}
    TcpPtr(const IpPtr &ptr) { set(ptr); }
    TcpPtr(const TcpPtr &ptr) : p(ptr.p), off(ptr.off) {}

    TcpHdr *operator->() { return h(); }
    TcpHdr &operator*() { return *h(); }
    operator TcpHdr *() { return h(); }

    const TcpHdr *operator->() const { return h(); }
    const TcpHdr &operator*() const { return *h(); }
    operator const TcpHdr *() const { return h(); }

    const TcpPtr &operator=(const IpPtr &i) { set(i); return *this; }
    const TcpPtr &operator=(const TcpPtr &t) { set(t.p, t.off); return *this; }

    const PacketPtr packet() const { return p; }
    PacketPtr packet() { return p; }
    bool operator!() const { return !p; }
    operator bool() const { return p; }
    operator bool() { return p; }
};

uint16_t cksum(const TcpPtr &ptr);

typedef Range<uint16_t> SackRange;

struct TcpOpt : public tcp_opt
{
    uint8_t type() const { return opt_type; }
    uint8_t len() const { return TCP_OPT_TYPEONLY(type()) ? 1 : opt_len; }

    bool isopt(int opt) const { return type() == opt; }

    const uint8_t *data() const { return opt_data.data8; }

    uint16_t mss() const { return ntohs(opt_data.mss); }
    uint8_t wscale() const { return opt_data.wscale; }
    bool sack(std::vector<SackRange> &vec) const;
    uint32_t echo() const { return ntohl(opt_data.echo); }
    uint32_t tsval() const { return ntohl(opt_data.timestamp[0]); }
    uint32_t tsecr() const { return ntohl(opt_data.timestamp[1]); }
    uint32_t cc() const { return ntohl(opt_data.cc); }
    uint8_t cksum() const{ return opt_data.cksum; }
    const uint8_t *md5() const { return opt_data.md5; }

    int size() const { return len(); }
    const uint8_t *bytes() const { return (const uint8_t *)this; }
    const uint8_t *payload() const { return bytes() + size(); }
    uint8_t *bytes() { return (uint8_t *)this; }
    uint8_t *payload() { return bytes() + size(); }
};

/*
 * UDP Stuff
 */
struct UdpHdr : public udp_hdr
{
    uint16_t sport() const { return ntohs(uh_sport); }
    uint16_t dport() const { return ntohs(uh_dport); }
    uint16_t len() const { return ntohs(uh_ulen); }
    uint16_t sum() const { return uh_sum; }

    void sum(uint16_t sum) { uh_sum = sum; }

    int size() const { return sizeof(udp_hdr); }
    const uint8_t *bytes() const { return (const uint8_t *)this; }
    const uint8_t *payload() const { return bytes() + size(); }
    uint8_t *bytes() { return (uint8_t *)this; }
    uint8_t *payload() { return bytes() + size(); }
};

class UdpPtr
{
  protected:
    PacketPtr p;
    int off;

    const UdpHdr *h() const { return (const UdpHdr *)(p->data + off); }
    UdpHdr *h() { return (UdpHdr *)(p->data + off); }

    void set(const PacketPtr &ptr, int offset) { p = ptr; off = offset; }
    void set(const IpPtr &ptr)
    {
	if (ptr->proto() == IP_PROTO_UDP)
	    set(ptr.p, sizeof(eth_hdr) + ptr->hlen());
	else
	    set(0, 0);
    }

  public:
    UdpPtr() {}
    UdpPtr(const IpPtr &ptr) { set(ptr); }
    UdpPtr(const UdpPtr &ptr) : p(ptr.p), off(ptr.off) {}

    UdpHdr *operator->() { return h(); }
    UdpHdr &operator*() { return *h(); }
    operator UdpHdr *() { return h(); }

    const UdpHdr *operator->() const { return h(); }
    const UdpHdr &operator*() const { return *h(); }
    operator const UdpHdr *() const { return h(); }

    const UdpPtr &operator=(const IpPtr &i) { set(i); return *this; }
    const UdpPtr &operator=(const UdpPtr &t) { set(t.p, t.off); return *this; }

    const PacketPtr packet() const { return p; }
    PacketPtr packet() { return p; }
    bool operator!() const { return !p; }
    operator bool() const { return p; }
    operator bool() { return p; }
};

uint16_t cksum(const UdpPtr &ptr);

/* namespace Net */ }
 
#endif // __BASE_INET_HH__
