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


#include <iostream.h>

#include <string>
#include <vector>

#include "base/str.hh"

using namespace std;

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <number>\n";
    exit(1);
  }

  string s = argv[1];

#define OUTVAL(valtype, type) do { \
  valtype value; \
  cout << "TYPE = " #valtype "\n"; \
  if (to_number(s, value)) { \
    cout << "Number(" << s << ") = " << dec \
      << (unsigned long long)(unsigned type)value << "\n" \
      << "Number(" << s << ") = " << dec \
      << (signed long long)(signed type)value << "\n" \
      << "Number(" << s << ") = 0x" << hex \
      << (unsigned long long)(unsigned type)value << "\n" \
      << "Number(" << s << ") = 0" << oct \
      << (unsigned long long)(unsigned type)value << "\n\n"; \
  } else \
    cout << "Number(" << s << ") is invalid\n\n"; \
  } while (0)

  OUTVAL(signed long long, long long);
  OUTVAL(unsigned long long, long long);
  OUTVAL(signed long, long);
  OUTVAL(unsigned long, long);
  OUTVAL(signed int, int);
  OUTVAL(unsigned int, int);
  OUTVAL(signed short, short);
  OUTVAL(unsigned short, short);
  OUTVAL(signed char, char);
  OUTVAL(unsigned char, char);

  return 0;
}
