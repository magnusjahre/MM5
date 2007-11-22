/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#ifndef __STR_HH__
#define __STR_HH__

#include <sstream>
#include <string>
#include <vector>

#include <ctype.h>

template<class> class Hash;
template<>
class Hash<std::string> {
public:
  unsigned operator()(const std::string &s) {
      std::string::const_iterator i = s.begin();
      std::string::const_iterator end = s.end();
      unsigned hash = 5381;

      while (i < end)
	  hash = ((hash << 5) + hash) + *i++;

      return hash;
  }
};

inline void
eat_lead_white(std::string &s)
{
    std::string::size_type off = s.find_first_not_of(' ');
    if (off != std::string::npos) {
	std::string::iterator begin = s.begin();
	s.erase(begin, begin + off);
    }
}

inline void
eat_end_white(std::string &s)
{
    std::string::size_type off = s.find_last_not_of(' ');
    if (off != std::string::npos)
	s.erase(s.begin() + off + 1, s.end());
}

inline void
eat_white(std::string &s)
{
    eat_lead_white(s);
    eat_end_white(s);
}

inline std::string
to_lower(const std::string &s)
{
    std::string lower;
    int len = s.size();

    lower.reserve(len);

    for (int i = 0; i < len; ++i)
	lower.push_back(tolower(s[i]));

    return lower;
}

// Split the string s into lhs and rhs on the first occurence of the
// character c.
bool
split_first(const std::string &s, std::string &lhs, std::string &rhs, char c);

// Split the string s into lhs and rhs on the last occurence of the
// character c.
bool
split_last(const std::string &s, std::string &lhs, std::string &rhs, char c);

// Tokenize the string <s> splitting on the character <token>, and
// place the result in the string vector <vector>.  If <ign> is true,
// then empty result strings (due to trailing tokens, or consecutive
// tokens) are skipped.
void
tokenize(std::vector<std::string> &vector, const std::string &s,
	 char token, bool ign = true);

template <class T> bool
to_number(const std::string &value, T &retval);

template <class T>
inline std::string
to_string(const T &value)
{
    std::stringstream str;
    str << value;
    return str.str();
}

// Put quotes around string arg if it contains spaces.
inline std::string
quote(const std::string &s)
{
    std::string ret;
    bool quote = s.find(' ') != std::string::npos;

    if (quote)
	ret = '"';

    ret += s;

    if (quote)
	ret += '"';

    return ret;
}

#endif //__STR_HH__
