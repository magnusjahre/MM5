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

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <unistd.h>

#include "sim/host.hh"
#include "base/misc.hh"
#include "base/socket.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////
//
//

ListenSocket::ListenSocket()
    : listening(false), fd(-1)
{}

ListenSocket::~ListenSocket()
{
    if (fd != -1)
	close(fd);
}

// Create a socket and configure it for listening
bool
ListenSocket::listen(int port, bool reuse)
{
    if (listening)
	panic("Socket already listening!");

    fd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	panic("Can't create socket:%s !", strerror(errno));

    if (reuse) {
	int i = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&i,
			 sizeof(i)) < 0)
	    panic("ListenSocket(listen): setsockopt() SO_REUSEADDR failed!");
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    sockaddr.sin_port = htons(port);
    int ret = ::bind(fd, (struct sockaddr *)&sockaddr, sizeof (sockaddr));
    if (ret != 0) {
	if (ret == -1 && errno != EADDRINUSE)
	    panic("ListenSocket(listen): bind() failed!");
	return false;
    }

    if (::listen(fd, 1) == -1)
	panic("ListenSocket(listen): listen() failed!");

    listening = true;

    return true;
}

// Open a connection.  Accept will block, so if you don't want it to,
// make sure a connection is ready before you call accept.
int
ListenSocket::accept(bool nodelay)
{
    struct sockaddr_in sockaddr;
    socklen_t slen = sizeof (sockaddr);
    int sfd = ::accept(fd, (struct sockaddr *)&sockaddr, &slen);
    if (sfd != -1 && nodelay) {
	int i = 1;
	::setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&i, sizeof(i));
    }

    return sfd;
}
