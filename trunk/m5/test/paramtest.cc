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



//
// This file is not part of the regular simulator.  It is solely for
// testing the parameter code.  Edit the Makefile to add param_test.cc
// to the sources list, then use configs/test.ini as the configuration
// file.
//
#include "sim/sim_object.hh"
#include "mem/cache/cache.hh"

class ParamTest : public SimObject
{
  public:
    ParamTest(string name)
	: SimObject(name)
    {
    }

    virtual ~ParamTest() {}
};

enum Enum1Type { Enum0 };
enum Enum2Type { Enum10 };

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ParamTest)

    Param<int> intparam;
    VectorParam<int> vecint;
    Param<string> stringparam;
    VectorParam<string> vecstring;
    Param<bool> boolparam;
    VectorParam<bool> vecbool;
    SimObjectParam<BaseMemory *> memobj;
    SimObjectVectorParam<BaseMemory *> vecmemobj;
    SimpleEnumParam<Enum1Type> enum1;
    MappedEnumParam<Enum2Type> enum2;
    SimpleEnumVectorParam<Enum1Type> vecenum1;
    MappedEnumVectorParam<Enum2Type> vecenum2;

END_DECLARE_SIM_OBJECT_PARAMS(ParamTest)

const char *enum1_strings[] =
{
    "zero", "one", "two", "three"
};

const EnumParamMap enum2_map[] =
{
    { "ten", 10 },
    { "twenty", 20 },
    { "thirty", 30 },
    { "forty", 40 }
};

BEGIN_INIT_SIM_OBJECT_PARAMS(ParamTest)

    INIT_PARAM(intparam, "intparam"),
    INIT_PARAM(vecint, "vecint"),
    INIT_PARAM(stringparam, "stringparam"),
    INIT_PARAM(vecstring, "vecstring"),
    INIT_PARAM(boolparam, "boolparam"),
    INIT_PARAM(vecbool, "vecbool"),
    INIT_PARAM(memobj, "memobj"),
    INIT_PARAM(vecmemobj, "vecmemobj"),
    INIT_ENUM_PARAM(enum1, "enum1", enum1_strings),
    INIT_ENUM_PARAM(enum2, "enum2", enum2_map),
    INIT_ENUM_PARAM(vecenum1, "vecenum1", enum1_strings),
    INIT_ENUM_PARAM(vecenum2, "vecenum2", enum2_map)

END_INIT_SIM_OBJECT_PARAMS(ParamTest)


CREATE_SIM_OBJECT(ParamTest)
{
    return new ParamTest(getInstanceName());
}

REGISTER_SIM_OBJECT("ParamTest", ParamTest)
