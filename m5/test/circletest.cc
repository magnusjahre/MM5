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



#include <fcntl.h>
#include <iostream.h>
#include <unistd.h>

#include "base/circlebuf.hh"

char *strings[] =
{ "This is the first test\n",
  "he went with his woman to the store\n",
  "the man with the bat hit the woman with the hat\n",
  "that that is is that that was\n",
  "sue sells sea shells by the sea shore\n",
  "go to the store and buy me some milk and bread\n",
  "the friendly flight attendants spoke soothingly to the frightened passengers in their native languages\n"
};

const int num_strings = sizeof(strings) / sizeof(char *);

int
main()
{
  CircleBuf buf(1024);

  for (int count = 0; count < 100; count++)
    buf.write(strings[count % num_strings]);
  buf.read(STDOUT_FILENO);
  write(STDOUT_FILENO, "<\n", 2);

  for (int count = 0; count < 100; count++)
    buf.write(strings[count % num_strings]);
  buf.read(STDOUT_FILENO, 100);
  write(STDOUT_FILENO, "<\n", 2);

  buf.flush();
  buf.write("asdfa asdf asd fasdf asdf\n");
  buf.write("");
  buf.write("");
  buf.write("");
  buf.write("");
  buf.write("");
  buf.write("");
  buf.read(STDOUT_FILENO);
  write(STDOUT_FILENO, "<\n", 2);
}
