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

#ifndef __CPRINTF_FORMATS_HH__
#define __CPRINTF_FORMATS_HH__

struct Format
{
    bool alternate_form;
    bool flush_left;
    bool print_sign;
    bool blank_space;
    bool fill_zero;
    bool uppercase;
    enum { dec, hex, oct } base;
    enum { none, string, integer, character, floating } format;
    enum { best, fixed, scientific } float_format;
    int precision;
    int width;

    Format() { clear(); }

    void clear()
    {
	alternate_form = false;
	flush_left = false;
	print_sign = false;
	blank_space = false;
	fill_zero = false;
	uppercase = false;
	base = dec;
	format = none;
	precision = -1;
	width = 0;
    }
};

template <typename T>
inline void
_format_char(std::ostream &out, const T &data, Format &fmt)
{
    using namespace std;

    out << data;
}

template <typename T>
inline void
_format_integer(std::ostream &out, const T &data, Format &fmt)
{
    using namespace std;

    switch (fmt.base) {
      case Format::hex:
	out.setf(ios::hex, ios::basefield);
	break;

      case Format::oct:
	out.setf(ios::oct, ios::basefield);
	break;

      case Format::dec:
	out.setf(ios::dec, ios::basefield);
	break;
    }

    if (fmt.alternate_form) {
	if (!fmt.fill_zero)
	    out.setf(ios::showbase);
	else {
	    switch (fmt.base) {
	      case Format::hex:
		out << "0x";
		fmt.width -= 2;
		break;
	      case Format::oct:
		out << "0";
		fmt.width -= 1;
		break;
	      case Format::dec:
		break;
	    }
	}
    }

    if (fmt.fill_zero)
	out.fill('0');

    if (fmt.width > 0)
	out.width(fmt.width);

    if (fmt.flush_left && !fmt.fill_zero)
	out.setf(ios::left);

    if (fmt.print_sign)
	out.setf(ios::showpos);

    if (fmt.uppercase)
	out.setf(ios::uppercase);

    out << data;
}

template <typename T>
inline void
_format_float(std::ostream &out, const T &data, Format &fmt)
{
    using namespace std;

    switch (fmt.float_format) {
      case Format::scientific:
	if (fmt.precision != -1) {
	    if (fmt.width > 0)
		out.width(fmt.width);

	    if (fmt.precision == 0)
		fmt.precision = 1;
	    else
		out.setf(ios::scientific);

	    out.precision(fmt.precision);
	} else
	    if (fmt.width > 0)
		out.width(fmt.width);

	if (fmt.uppercase)
	    out.setf(ios::uppercase);
	break;

      case Format::fixed:
	if (fmt.precision != -1) {
	    if (fmt.width > 0)
		out.width(fmt.width);

	    out.setf(ios::fixed);
	    out.precision(fmt.precision);
	} else
	    if (fmt.width > 0)
		out.width(fmt.width);

	break;

      default:
	if (fmt.precision != -1)
	    out.precision(fmt.precision);

	if (fmt.width > 0)
	    out.width(fmt.width);

	break;
    }

    out << data;
}

template <typename T>
inline void
_format_string(std::ostream &out, const T &data, Format &fmt)
{
    using namespace std;

#if defined(__GNUC__) && (__GNUC__ < 3) || 1
    if (fmt.width > 0) {
	std::stringstream foo;
	foo << data;
	int flen = foo.str().size();

	if (fmt.width > flen) {
	    char *spaces = new char[fmt.width - flen + 1];
	    memset(spaces, ' ', fmt.width - flen);
	    spaces[fmt.width - flen] = 0;

	    if (fmt.flush_left)
		out << foo.str() << spaces;
	    else
		out << spaces << foo.str();

	    delete [] spaces;
	} else
	    out << data;
    } else
	out << data;
#else
    if (fmt.width > 0)
	out.width(fmt.width);
    if (fmt.flush_left)
	out.setf(ios::left);

    out << data;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//
//  The code below controls the actual usage of formats for various types
//

//
// character formats
//
template <typename T>
inline void
format_char(std::ostream &out, const T &data, Format &fmt)
{ out << "<bad arg type for char format>"; }

inline void
format_char(std::ostream &out, char data, Format &fmt)
{ _format_char(out, data, fmt); }

inline void
format_char(std::ostream &out, unsigned char data, Format &fmt)
{ _format_char(out, data, fmt); }

inline void
format_char(std::ostream &out, signed char data, Format &fmt)
{ _format_char(out, data, fmt); }

inline void
format_char(std::ostream &out, short data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, unsigned short data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, int data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, unsigned int data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, long data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, unsigned long data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, long long data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

inline void
format_char(std::ostream &out, unsigned long long data, Format &fmt)
{ _format_char(out, (char)data, fmt); }

//
// integer formats
//
template <typename T>
inline void
format_integer(std::ostream &out, const T &data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, char data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, unsigned char data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, signed char data, Format &fmt)
{ _format_integer(out, data, fmt); }
#if 0
inline void
format_integer(std::ostream &out, short data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, unsigned short data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, int data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, unsigned int data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, long data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, unsigned long data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, long long data, Format &fmt)
{ _format_integer(out, data, fmt); }
inline void
format_integer(std::ostream &out, unsigned long long data, Format &fmt)
{ _format_integer(out, data, fmt); }
#endif

//
// floating point formats
//
template <typename T>
inline void
format_float(std::ostream &out, const T &data, Format &fmt)
{ out << "<bad arg type for float format>"; }

inline void
format_float(std::ostream &out, float data, Format &fmt)
{ _format_float(out, data, fmt); }

inline void
format_float(std::ostream &out, double data, Format &fmt)
{ _format_float(out, data, fmt); }

//
// string formats
//
template <typename T>
inline void
format_string(std::ostream &out, const T &data, Format &fmt)
{ _format_string(out, data, fmt); }

inline void
format_string(std::ostream &out, const std::stringstream &data, Format &fmt)
{ _format_string(out, data.str(), fmt); }

#endif // __CPRINTF_FORMATS_HH__
