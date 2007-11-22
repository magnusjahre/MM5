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

#include <assert.h>

#include "base/inifile.hh"
#include "base/misc.hh"
#include "sim/builder.hh"
#include "sim/configfile.hh"
#include "sim/config_node.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"
#include "sim/root.hh"

using namespace std;

SimObjectBuilder::SimObjectBuilder(ConfigNode *_configNode)
    : ParamContext(_configNode->getPath(), NoAutoInit),
      configNode(_configNode)
{
}

SimObjectBuilder::~SimObjectBuilder()
{
}

///////////////////////////////////////////
//
// SimObjectBuilder member definitions
//
///////////////////////////////////////////

// override ParamContext::parseParams() to check params based on
// instance name first.  If not found, then check based on iniSection
// (as in default ParamContext implementation).
void
SimObjectBuilder::parseParams(IniFile &iniFile)
{
    iniFilePtr = &iniFile;	// set object member

    ParamList::iterator i;

    for (i = paramList->begin(); i != paramList->end(); ++i) {
	string string_value;
	if (iniFile.find(iniSection, (*i)->name, string_value))
	    (*i)->parse(string_value);
    }
}


void
SimObjectBuilder::printErrorProlog(ostream &os)
{
    ccprintf(os, "Error creating object '%s' of type '%s':\n",
	     iniSection, configNode->getType());
}


////////////////////////////////////////////////////////////////////////
//
// SimObjectClass member definitions
//
////////////////////////////////////////////////////////////////////////

// Map of class names to SimObjectBuilder creation functions.  Need to
// make this a pointer so we can force initialization on the first
// reference; otherwise, some SimObjectClass constructors may be invoked
// before the classMap constructor.
map<string,SimObjectClass::CreateFunc> *SimObjectClass::classMap = NULL;

// SimObjectClass constructor: add mapping to classMap
SimObjectClass::SimObjectClass(const string &className, CreateFunc createFunc)
{
    if (classMap == NULL)
	classMap = new map<string,SimObjectClass::CreateFunc>();

    if ((*classMap)[className])
	panic("Error: simulation object class '%s' redefined\n", className);

    // add className --> createFunc to class map
    (*classMap)[className] = createFunc;
}


//
//
SimObject *
SimObjectClass::createObject(IniFile &configDB, ConfigNode *configNode)
{
    const string &type = configNode->getType();

    // look up className to get appropriate createFunc
    if (classMap->find(type) == classMap->end())
	panic("Simulator object type '%s' not found.\n", type);


    CreateFunc createFunc = (*classMap)[type];

    // call createFunc with config hierarchy node to get object
    // builder instance (context with parameters for object creation)
    SimObjectBuilder *objectBuilder = (*createFunc)(configNode);

    assert(objectBuilder != NULL);

    // parse all parameters in context to generate parameter values
    objectBuilder->parseParams(configDB);

    // now create the actual simulation object
    SimObject *object = objectBuilder->create();

    assert(object != NULL);

    // echo object parameters to stats file (for documenting the
    // config used to generate the associated stats)
    ccprintf(*configStream, "[%s]\n", object->name());
    ccprintf(*configStream, "type=%s\n", type);
    objectBuilder->showParams(*configStream);
    ccprintf(*configStream, "\n");

    // done with the SimObjectBuilder now
    delete objectBuilder;

    return object;
}


//
// static method:
//
void
SimObjectClass::describeAllClasses(ostream &os)
{
    map<string,CreateFunc>::iterator iter;

    for (iter = classMap->begin(); iter != classMap->end(); ++iter) {
	const string &className = iter->first;
	CreateFunc createFunc = iter->second;

	os << "[" << className << "]\n";

	// create dummy object builder just to instantiate parameters
	SimObjectBuilder *objectBuilder = (*createFunc)(NULL);

	// now get the object builder to describe ite params
	objectBuilder->describeParams(os);

	os << endl;

	// done with the object builder now
	delete objectBuilder;
    }
}
