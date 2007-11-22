/*
 * Copyright (c) 2004, 2005
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
#include <algorithm>

#include "base/trace.hh"
#include "targetarch/arguments.hh"

using namespace std;


void
Printk(AlphaArguments args)
{
    char *p = (char *)args++;

    ios::fmtflags saved_flags = DebugOut().flags();
    char old_fill = DebugOut().fill();
    int old_precision = DebugOut().precision();

    while (*p) {
	switch (*p) {
	  case '%': {
	      bool more = true;
	      bool islong = false;
	      bool leftjustify = false;
	      bool format = false;
	      bool zero = false;
	      int width = 0;
	      while (more && *++p) {
		  switch (*p) {
		    case 'l':
		    case 'L':
		      islong = true;
		      break;
		    case '-':
		      leftjustify = true;
		      break;
		    case '#':
		      format = true;
		      break;
		    case '0':
		      if (width)
			  width *= 10;
		      else
			  zero = true;
		      break;
		    default:
		      if (*p >= '1' && *p <= '9')
			  width = 10 * width + *p - '0';
		      else
			  more = false;
		      break;
		  }
	      }

	      bool hexnum = false;
	      bool octal = false;
	      bool sign = false;
	      switch (*p) {
		case 'X':
		case 'x':
		  hexnum = true;
		  break;
		case 'O':
		case 'o':
		  octal = true;
		  break;
		case 'D':
		case 'd':
		  sign = true;
		  break;
		case 'P':
		  format = true;
		case 'p':
		  hexnum = true;
		  break;
	      }

	      switch (*p) {
		case 'D':
		case 'd':
		case 'U':
		case 'u':
		case 'X':
		case 'x':
		case 'O':
		case 'o':
		case 'P':
		case 'p': {
		  if (hexnum)
		      DebugOut() << hex;

		  if (octal)
		      DebugOut() << oct;

		  if (format) {
		      if (!zero)
			  DebugOut().setf(ios::showbase);
		      else {
			  if (hexnum) {
			      DebugOut() << "0x";
			      width -= 2;
			  } else if (octal) {
			      DebugOut() << "0";
			      width -= 1;
			  }
		      }
		  }

		  if (zero)
		      DebugOut().fill('0');

		  if (width > 0)
		      DebugOut().width(width);

		  if (leftjustify && !zero)
		      DebugOut().setf(ios::left);

		  if (sign) {
		      if (islong)
			  DebugOut() << (int64_t)args;
		      else
			  DebugOut() << (int32_t)args;
		  } else {
		      if (islong)
			  DebugOut() << (uint64_t)args;
		      else
			  DebugOut() << (uint32_t)args;
		  }

		  if (zero)
		      DebugOut().fill(' ');

		  if (width > 0)
		      DebugOut().width(0);

		  DebugOut() << dec;

		  ++args;
		}
		  break;

		case 's': {
		    char *s = (char *)args;
		    if (!s)
			s = "<NULL>";

		    if (width > 0)
			DebugOut().width(width);
		    if (leftjustify)
			DebugOut().setf(ios::left);

		    DebugOut() << s;
		    ++args;
		}
		  break;
		case 'C':
		case 'c': {
		    uint64_t mask = (*p == 'C') ? 0xffL : 0x7fL;
		    uint64_t num;
		    int width;

		    if (islong) {
			num = (uint64_t)args;
			width = sizeof(uint64_t);
		    } else {
			num = (uint32_t)args;
			width = sizeof(uint32_t);
		    }

		    while (width-- > 0) {
			char c = (char)(num & mask);
			if (c)
			    DebugOut() << c;
			num >>= 8;
		    }

		    ++args;
		}
		  break;
		case 'b': {
		  uint64_t n = (uint64_t)args++;
		  char *s = (char *)args++;
		  DebugOut() << s << ": " << n;
		}
		  break;
		case 'n':
		case 'N': {
		    args += 2;
#if 0
		    uint64_t n = (uint64_t)args++;
		    struct reg_values *rv = (struct reg_values *)args++;
#endif
		}
		  break;
		case 'r':
		case 'R': {
		    args += 2;
#if 0
		    uint64_t n = (uint64_t)args++;
		    struct reg_desc *rd = (struct reg_desc *)args++;
#endif
		}
		  break;
		case '%':
		  DebugOut() << '%';
		  break;
	      }
	      ++p;
	  }
	    break;
	  case '\n':
	    DebugOut() << endl;
	    ++p;
	    break;
	  case '\r':
	    ++p;
	    if (*p != '\n')
		DebugOut() << endl;
	    break;

	  default: {
	      size_t len = strcspn(p, "%\n\r\0");
	      DebugOut().write(p, len);
	      p += len;
	  }
	}
    }

    DebugOut().flags(saved_flags);
    DebugOut().fill(old_fill);
    DebugOut().precision(old_precision);
}

