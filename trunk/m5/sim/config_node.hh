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

#ifndef __SIM_CONFIG_NODE_HH__
#define __SIM_CONFIG_NODE_HH__

#include "sim/configfile.hh"

class SimObject;
class IniFile;
class Checkpoint;

//
// A node within the configuration hierarchy.  Pulled definition out
// to separate file since it's only needed in a few of the places that
// need configfile.hh.
//
class ConfigHierarchy::Node
{
  private:
    friend class ConfigHierarchy;

    std::string nodeName;
    std::string nodePath;
    std::string nodeType;

    // backpointer to the config hierarchy this node belongs to
    ConfigHierarchy *myHierarchy;

    SimObject *simObject;

    Node *parent;
    std::vector<Node *> children;

    IniFile &getConfigDB() { return myHierarchy->configDB; }

    void buildSubtree();
    void parseChildren(const std::string &s);

  public:
    Node(const std::string &_name, Node *_parent, ConfigHierarchy *hierarchy);
    ~Node();

    const std::string &getName() const { return nodeName; }
    const std::string &getPath() const { return nodePath; }
    const std::string &getType() const { return nodeType; }

    void createSimObject();
    void createSimObjects();

    void unserialize(Checkpoint *cp, const std::string &section);

    bool find(const std::string &attr, std::string &value)
    {
	// look first under the full path, then under the class name
	return getConfigDB().find(nodePath, attr, value);
    }

    const Node *findChild(const std::string &name) const;

    const Node *resolveNode(const std::string &name, int level = 0) const;
    SimObject *resolveSimObject(const std::string &name) const;

    void addChild(const std::string &name);
};


#endif // __SIM_CONFIG_NODE_HH__
