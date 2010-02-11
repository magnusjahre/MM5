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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "base/cprintf.hh"
#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/str.hh"
#include "base/trace.hh"
#include "sim/builder.hh"
#include "sim/config_node.hh"
#include "sim/configfile.hh"
#include "sim/sim_object.hh"

using namespace std;

ConfigHierarchy::ConfigHierarchy(IniFile &f)
: configDB(f), root(NULL)
{
}

void
ConfigHierarchy::build()
{
	root = new Node("root", NULL, this);
	root->buildSubtree();
}

ConfigHierarchy::Node::Node(const std::string &_name, Node *_parent,
		ConfigHierarchy *hierarchy)
: nodeName(_name), myHierarchy(hierarchy), simObject(NULL), parent(_parent)
{
	if (parent && parent->nodePath != "root")
		nodePath = parent->nodePath + '.';
	nodePath += nodeName;
}

//
// build the subtree under current node (recursively)
//
void
ConfigHierarchy::Node::buildSubtree()
{
	DPRINTF(Config, "buildSubtree: %s\n", nodePath);

	// get 'children=' option
	string children_string;
	if (!find("children", children_string)) {
		DPRINTF(Config, "%s: no children found\n", nodePath);
		return;
	}

	// parse & create child ConfigHierarchy::Node objects
	parseChildren(children_string);

	// recursively build subtree under each child
	for (int i = 0; i < children.size(); i++)
		children[i]->buildSubtree();
}


//
// parse 'children=' option string and create corresponding Node
// objects as children of current node
//
// if a 'checkpoint=' is present, add a checkpoint node to the
// child list, and pass down the root name
void
ConfigHierarchy::Node::parseChildren(const string &str)
{
	string s = str;
	eat_white(s);
	vector<string> v;

	tokenize(v, s, ' ');
	vector<string>::iterator i = v.begin(), end = v.end();
	while (i != end) {
		addChild(*i);
		++i;
	}
}

void
ConfigHierarchy::Node::addChild(const string &childName)
{
	// check for duplicate names (not allowed as it makes future
	// references to the name ambiguous)
	if (findChild(childName))
		panic("Duplicate name '%s' in config node '%s'\n", childName);

	DPRINTF(Config, "addChild: adding child %s\n", childName);
	children.push_back(new Node(childName, this, myHierarchy));
}


const ConfigHierarchy::Node *
ConfigHierarchy::Node::findChild(const string &name) const
{
	for (int i = 0; i < children.size(); i++) {
		const Node *n = children[i];
		if (n->nodeName == name)
			return n;
	}

	return NULL;
}

const ConfigHierarchy::Node *
ConfigHierarchy::Node::resolveNode(const string &name, int level) const
{
	const ConfigNode *n;

	vector<string> v;

	tokenize(v, name, '.');

	if ((n = findChild(v[level])) != NULL) {

		// have a match at current level of path

		if (level == v.size() - 1){
			// this is the last level... we're done
			return n;
		} else {
			// look for match at next level
			return n->resolveNode(name, level + 1);
		}
	}

	if (level == 0 && parent != NULL) {
		// still looking for initial match: recurse up the tree if we can
		return parent->resolveNode(name, 0);
	}

	// nowhere else to look... give up
	return NULL;
}


SimObject *
ConfigHierarchy::Node::resolveSimObject(const string &name) const
{
	const ConfigNode *node = resolveNode(name);

	// check fro config node not found
	if (node == NULL)
		return NULL;

	// if config node exists, but corresponding SimObject has not yet
	// been created, force creation
	if (node->simObject == NULL) {
		ConfigNode *mnode = const_cast<ConfigNode *>(node);
		mnode->createSimObject();
	}

	return node->simObject;
}


void
ConfigHierarchy::createSimObjects()
{
	root->createSimObject();

	// do a depth-first post-order traversal, creating each hierarchy
	// node's corresponding simulation object as we go
	for (int i = 0; i < root->children.size(); i++)
		root->children[i]->createSimObjects();
}

void
ConfigHierarchy::Node::createSimObject()
{
	DPRINTFR(Config, "create SimObject: %s\n", getPath());

	//cout << "create SimObject: " << getPath() << "\n";

	// make this safe to call more than once by skipping if object
	// already created
	if (simObject != NULL)
		return;

	if (find("type", nodeType)) {
		simObject = SimObjectClass::createObject(getConfigDB(),  this);
		DPRINTFR(Config, "new SimObject: %s\n", getPath());

		if (!simObject)
			panic("Error creating object.\n");
	}
}


void
ConfigHierarchy::Node::createSimObjects()
{
	// do a depth-first post-order traversal, creating each hierarchy
	// node's corresponding simulation object as we go
	for (int i = 0; i < children.size(); i++)
		children[i]->createSimObjects();

	// now create this node's object
	createSimObject();
}


void
ConfigHierarchy::dump(ostream &stream) const
{
	dump(stream, root, 0);
}


void
ConfigHierarchy::dump(ostream &stream, const Node *n, int l) const
{
	if (!n) return;
	for (int i = 0; i < l; i++)
		stream << "  ";
	ccprintf(stream, "%s\n", n->nodeName);

	for (int i = 0; i < n->children.size(); i++)
		dump(stream, n->children[i], l + 1);
}

void
ConfigHierarchy::unserializeSimObjects()
{
	root->unserialize(NULL, "");

	// keep checkpoint in memory so that processes can be restarted
	//	Checkpoint* cpt = root->unserialize(NULL, "");
	//	delete cpt;
}

// Walks tree in a pre-order traversal and unserializes any
// SimObjects with a checkpoint provided
Checkpoint*
ConfigHierarchy::Node::unserialize(Checkpoint *parentCkpt,
		const std::string &section)
{
	Checkpoint *cp = parentCkpt; // use parent's checkpoint by default
	string cpSection = section;  // and parent-provided section name

	string checkpointName;

	if (find("checkpoint", checkpointName) && checkpointName != "") {
		DPRINTFR(Config, "Loading checkpoint dir '%s'\n",
				checkpointName);
		cp = new Checkpoint(checkpointName, section, this);
		Serializable::unserializeGlobals(cp);
		cpSection = "";
	}

	if (simObject && cp) {
		DPRINTFR(Config, "Unserializing '%s' from section '%s'\n",
				simObject->name(), cpSection);
		if(cp->sectionExists(cpSection))
			simObject->unserialize(cp, cpSection);
//		else
//			warn("Not unserializing '%s': no section found in checkpoint.\n",
//					cpSection);
	}

	for (int i = 0; i < children.size(); i++) {
		string childSection = cpSection;
		if (childSection != "")
			childSection += ".";
		childSection += children[i]->nodeName;
		children[i]->unserialize(cp, childSection);
	}

	return cp;
}
