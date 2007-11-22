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

#include <cstdlib>
#include <cmath>

#include "sim/param.hh"
#include "base/random.hh"

using namespace std;

class RandomContext : public ParamContext
{
  public:
    RandomContext(const string &_iniSection)
	: ::ParamContext(_iniSection) {}
    ~RandomContext() {}

    void checkParams();
};

RandomContext paramContext("random");

Param<unsigned>
seed(&paramContext, "seed", "seed to random number generator", 1);

void
RandomContext::checkParams()
{
    ::srandom(seed);
}

long
getLong()
{
    return random();
}

// idea for generating a double from erand48
double
getDouble()
{
    union {
	uint32_t _long[2];
	uint16_t _short[4];
    };

    _long[0] = random();
    _long[1] = random();

    return ldexp((double) _short[0], -48) +
	ldexp((double) _short[1], -32) +
	ldexp((double) _short[2], -16);
}
