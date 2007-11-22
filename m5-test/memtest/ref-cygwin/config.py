# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

import sys

string_data = {}

def AddModule(path, name, type, filename, data):
    fullpath = path
    if name != '__init__':
        fullpath += [name]
    fullpath = '.'.join(fullpath)
    string_data[fullpath] = name,type,data,path,filename

class StringImporter:
    def find_module(self, fullname, path=None):
        if string_data.has_key(fullname):
            name,type,data,path,filename = string_data[fullname]
            if type == 'py':
                return self
        return None

    def load_module(self, fullname):
        import imp
        mod = imp.new_module(fullname)
        sys.modules[fullname] = mod
        name,type,data,path,filename = string_data[fullname]

        mod.__file__ = "<embed: %s>" % filename
        mod.__loader__ = self
        if name == '__init__':
            mod.__path__ = path

        if type == 'py':
            code = compile(data, mod.__file__, "exec")
            exec code in mod.__dict__
        else:
            raise AttributeError, "File of the wrong type!"

        return mod

sys.meta_path.append(StringImporter())
AddModule([], 'jobfile', 'py', 'm5/util/pbs/jobfile.py', '''\
import sys

class ternary(object):
    def __new__(cls, *args):
        if len(args) > 1:
            raise TypeError, \\
                  '%s() takes at most 1 argument (%d given)' % \\
                  (cls.__name__, len(args))

        if args:
            if not isinstance(args[0], (bool, ternary)):
                raise TypeError, \\
                      '%s() argument must be True, False, or Any' % \\
                      cls.__name__
            return args[0]
        return super(ternary, cls).__new__(cls)

    def __bool__(self):
        return True

    def __neg__(self):
        return self

    def __eq__(self, other):
        return True
    
    def __ne__(self, other):
        return False

    def __str__(self):
        return 'Any'

    def __repr__(self):
        return 'Any'

Any = ternary()
    
class Flags(dict):
    def __init__(self, *args, **kwargs):
        super(Flags, self).__init__()
        self.update(*args, **kwargs)

    def __getattr__(self, attr):
        return self[attr]

    def __setattr__(self, attr, value):
        self[attr] = value

    def __setitem__(self, item, value):
        return super(Flags, self).__setitem__(item, ternary(value))

    def __getitem__(self, item):
        if item not in self:
            return False
        return super(Flags, self).__getitem__(item)

    def update(self, *args, **kwargs):
        for arg in args:
            if isinstance(arg, Flags):
                super(Flags, self).update(arg)
            elif isinstance(arg, dict):
                for key,val in kwargs.iteritems():
                    self[key] = val
            else:
                raise AttributeError, \\
                      'flags not of type %s or %s, but %s' % \\
                      (Flags, dict, type(arg))

        for key,val in kwargs.iteritems():
            self[key] = val

    def match(self, *args, **kwargs):
        match = Flags(*args, **kwargs)

        for key,value in match.iteritems():
            if self[key] != value:
                return False

        return True

def crossproduct(items):
    if not isinstance(items, (list, tuple)):
        raise AttributeError, 'crossproduct works only on sequences'

    if not items:
        yield None
        return

    current = items[0]
    remainder = items[1:]

    if not hasattr(current, '__iter__'):
        current = [ current ]
    
    for item in current:
        for rem in crossproduct(remainder):
            data = [ item ]
            if rem:
                data += rem
            yield data

def flatten(items):
    if not isinstance(items, (list, tuple)):
        yield items
        return

    for item in items:
        for flat in flatten(item):
            yield flat

class Data(object):
    def __init__(self, name, desc, **kwargs):
        self.name = name
        self.desc = desc
        self.system = None
        self.flags = Flags()
        self.env = {}
        for k,v in kwargs.iteritems():
            setattr(self, k, v)

    def update(self, obj):
        if not isinstance(obj, Data):
            raise AttributeError, "can only update from Data object"

        self.env.update(obj.env)
        self.flags.update(obj.flags)
        if obj.system:
            if self.system and self.system != obj.system:
                raise AttributeError, \\
                      "conflicting values for system: '%s'/'%s'" % \\
                      (self.system, obj.system)
            self.system = obj.system

    def printinfo(self):
        if self.name:
            print 'name: %s' % self.name
        if self.desc:
            print 'desc: %s' % self.desc
        if self.system:
            print 'system: %s' % self.system

    def printverbose(self):
        print 'flags:'
        keys = self.flags.keys()
        keys.sort()
        for key in keys:
            print '    %s = %s' % (key, self.flags[key])
        print 'env:'
        keys = self.env.keys()
        keys.sort()
        for key in keys:
            print '    %s = %s' % (key, self.env[key])
        print

    def __str__(self):
        return self.name

class Job(Data):
    def __init__(self, options):
        super(Job, self).__init__('', '')
        self.setoptions(options)

        self.checkpoint = False
        opts = []
        for opt in options:
            cpt = opt.group.checkpoint
            if not cpt:
                self.checkpoint = True
                continue
            if isinstance(cpt, Option):
                opt = cpt.clone(suboptions=False)
            else:
                opt = opt.clone(suboptions=False)

            opts.append(opt)

        if not opts:
            self.checkpoint = False

        if self.checkpoint:
            self.checkpoint = Job(opts)

    def clone(self):
        return Job(self.options)

    def __getattribute__(self, attr):
        if attr == 'name':
            names = [ ]
            for opt in self.options:
                if opt.name:
                    names.append(opt.name)
            return ':'.join(names)

        if attr == 'desc':
            descs = [ ]
            for opt in self.options:
                if opt.desc:
                    descs.append(opt.desc)
            return ', '.join(descs)

        return super(Job, self).__getattribute__(attr)
            
    def setoptions(self, options):
        config = options[0].config
        for opt in options:
            if opt.config != config:
                raise AttributeError, \\
                      "All options are not from the same Configuration"
            
        self.config = config
        self.groups = [ opt.group for opt in options ]
        self.options = options

        self.update(self.config)
        for group in self.groups:
            self.update(group)
                
        for option in self.options:
            self.update(option)
            if option._suboption:
                self.update(option._suboption)

    def printinfo(self):
        super(Job, self).printinfo()
        if self.checkpoint:
            print 'checkpoint: %s' % self.checkpoint.name
        print 'config: %s' % self.config.name
        print 'groups: %s' % [ g.name for g in self.groups ]
        print 'options: %s' % [ o.name for o in self.options ]
        super(Job, self).printverbose()

class SubOption(Data):
    def __init__(self, name, desc, **kwargs):
        super(SubOption, self).__init__(name, desc, **kwargs)
        self.number = None

class Option(Data):
    def __init__(self, name, desc, **kwargs):
        super(Option, self).__init__(name, desc, **kwargs)
        self._suboptions = []
        self._suboption = None
        self.number = None

    def __getattribute__(self, attr):
        if attr == 'name':
            name = self.__dict__[attr]
            if self._suboption is not None:
                name = '%s:%s' % (name, self._suboption.name)
            return name

        if attr == 'desc':
            desc = self.__dict__[attr]
            if self._suboption is not None:
                desc = '%s, %s' % (desc, self._suboption.desc)
            return desc

        return super(Option, self).__getattribute__(attr)

    def suboption(self, name, desc, **kwargs):
        subo = SubOption(name, desc, **kwargs)
        subo.config = self.config
        subo.group = self.group
        subo.option = self
        subo.number = len(self._suboptions)
        self._suboptions.append(subo)
        return subo

    def clone(self, suboptions=True):
        option = Option(self.__dict__['name'], self.__dict__['desc'])
        option.update(self)
        option.group = self.group
        option.config = self.config
        option.number = self.number
        if suboptions:
            option._suboptions.extend(self._suboptions)
            option._suboption = self._suboption
        return option

    def subopts(self):
        if not self._suboptions:
            return [ self ]

        subopts = []
        for subo in self._suboptions:
            option = self.clone()
            option._suboption = subo
            subopts.append(option)

        return subopts

    def printinfo(self):
        super(Option, self).printinfo()
        print 'config: %s' % self.config.name
        super(Option, self).printverbose()

class Group(Data): 
    def __init__(self, name, desc, **kwargs):
        super(Group, self).__init__(name, desc, **kwargs)
        self._options = []
        self.checkpoint = False
        self.number = None

    def option(self, name, desc, **kwargs):
        opt = Option(name, desc, **kwargs)
        opt.config = self.config
        opt.group = self
        opt.number = len(self._options)
        self._options.append(opt)
        return opt

    def options(self):
        return self._options

    def subopts(self):
        subopts = []
        for opt in self._options:
            for subo in opt.subopts():
                subopts.append(subo)
        return subopts

    def printinfo(self):
        super(Group, self).printinfo()
        print 'config: %s' % self.config.name
        print 'options: %s' % [ o.name for o in self._options ]
        super(Group, self).printverbose()

class Configuration(Data):
    def __init__(self, name, desc, **kwargs):
        super(Configuration, self).__init__(name, desc, **kwargs)
        self._groups = []

    def group(self, name, desc, **kwargs):
        grp = Group(name, desc, **kwargs)
        grp.config = self
        grp.number = len(self._groups)
        self._groups.append(grp)
        return grp

    def groups(self, flags=Flags(), sign=True):
        if not flags:
            return self._groups

        return [ grp for grp in self._groups if sign ^ grp.flags.match(flags) ]

    def checkchildren(self, kids):
        for kid in kids:
            if kid.config != self:
                raise AttributeError, "child from the wrong configuration"

    def sortgroups(self, groups):
        groups = [ (grp.number, grp) for grp in groups ]
        groups.sort()
        return [ grp[1] for grp in groups ]
    
    def options(self, groups = None, checkpoint = False):
        if groups is None:
            groups = self._groups
        self.checkchildren(groups)
        groups = self.sortgroups(groups)
        if checkpoint:
            groups = [ grp for grp in groups if grp.checkpoint ]
            optgroups = [ g.options() for g in groups ]
        else:
            optgroups = [ g.subopts() for g in groups ]
        for options in crossproduct(optgroups):
            for opt in options:
                cpt = opt.group.checkpoint
                if not isinstance(cpt, bool) and cpt != opt:
                    if checkpoint:
                        break
                    else:
                        yield options
            else:
                if checkpoint:
                    yield options

    def checkpoints(self, groups = None):
        for options in self.options(groups, True):
            yield Job(options)

    def jobs(self, groups = None):
        for options in self.options(groups, False):
            yield Job(options)

    def alljobs(self, groups = None):
        for options in self.options(groups, True):
            yield Job(options)
        for options in self.options(groups, False):
            yield Job(options)

    def find(self, jobname):
        for job in self.alljobs():
            if job.name == jobname:
                return job
        else:
            raise AttributeError, "job '%s' not found" % jobname

    def job(self, options):
        self.checkchildren(options)
        options = [ (opt.group.number, opt) for opt in options ]
        options.sort()
        options = [ opt[1] for opt in options ]
        job = Job(options)
        return job

    def printinfo(self):
        super(Configuration, self).printinfo()
        print 'groups: %s' % [ g.name for g in self._grouips ]
        super(Configuration, self).printverbose()

def JobFile(jobfile):
    from os.path import expanduser, isfile, join as joinpath
    filename = expanduser(jobfile)

    # Can't find filename in the current path, search sys.path
    if not isfile(filename):
        for path in sys.path:
            testname = joinpath(path, filename)
            if isfile(testname):
                filename = testname
                break
        else:
            raise AttributeError, \\
                  "Could not find file '%s'" % jobfile

    data = {}
    execfile(filename, data)
    if 'conf' not in data:
        raise ImportError, 'cannot import name conf from %s' % jobfile
    conf = data['conf']
    import jobfile
    if not isinstance(conf, Configuration):
        raise AttributeError, \\
              'conf in jobfile: %s (%s) is not type %s' % \\
              (jobfile, type(conf), Configuration)
    return conf

if __name__ == '__main__':
    from jobfile import *
    import sys

    usage = 'Usage: %s [-b] [-c] [-v] <jobfile>' % sys.argv[0]

    try:
        import getopt
        opts, args = getopt.getopt(sys.argv[1:], '-bcv')
    except getopt.GetoptError:
        sys.exit(usage)

    if len(args) != 1:
        raise AttributeError, usage

    both = False
    checkpoint = False
    verbose = False
    for opt,arg in opts:
        if opt == '-b':
            both = True
            checkpoint = True
        if opt == '-c':
            checkpoint = True
        if opt == '-v':
            verbose = True

    jobfile = args[0]
    conf = JobFile(jobfile)

    if both:
        gen = conf.alljobs()
    elif checkpoint:
        gen = conf.checkpoints()
    else:
        gen = conf.jobs()
        
    for job in gen:
        if not verbose:
            cpt = ''
            if job.checkpoint:
                cpt = job.checkpoint.name
            print job.name, cpt
        else:
            job.printinfo()
''')

AddModule(['m5'], '__init__', 'py', 'm5/python/m5/__init__.py', '''\
# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

import sys, os

# define this here so we can use it right away if necessary
def panic(string):
    print >>sys.stderr, 'panic:', string
    sys.exit(1)

def m5execfile(f, global_dict):
    # copy current sys.path
    oldpath = sys.path[:]
    # push file's directory onto front of path
    sys.path.insert(0, os.path.abspath(os.path.dirname(f)))
    execfile(f, global_dict)
    # restore original path
    sys.path = oldpath

# Prepend given directory to system module search path.
def AddToPath(path):
    # if it's a relative path and we know what directory the current
    # python script is in, make the path relative to that directory.
    if not os.path.isabs(path) and sys.path[0]:
        path = os.path.join(sys.path[0], path)
    path = os.path.realpath(path)
    # sys.path[0] should always refer to the current script's directory,
    # so place the new dir right after that.
    sys.path.insert(1, path)

# find the m5 compile options: must be specified as a dict in
# __main__.m5_build_env.
import __main__
if not hasattr(__main__, 'm5_build_env'):
    panic("__main__ must define m5_build_env")

# make a SmartDict out of the build options for our local use
import smartdict
build_env = smartdict.SmartDict()
build_env.update(__main__.m5_build_env)

# make a SmartDict out of the OS environment too
env = smartdict.SmartDict()
env.update(os.environ)

# import the main m5 config code
from config import *

# import the built-in object definitions
from objects import *

''')

AddModule(['m5'], 'config', 'py', 'm5/python/m5/config.py', '''\
# Copyright (c) 2004, 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

from __future__ import generators
import os, re, sys, types, inspect

import m5
panic = m5.panic
from convert import *
from multidict import multidict

noDot = False
try:
    import pydot
except:
    noDot = True

class Singleton(type):
    def __call__(cls, *args, **kwargs):
        if hasattr(cls, '_instance'):
            return cls._instance

        cls._instance = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instance

#####################################################################
#
# M5 Python Configuration Utility
#
# The basic idea is to write simple Python programs that build Python
# objects corresponding to M5 SimObjects for the desired simulation
# configuration.  For now, the Python emits a .ini file that can be
# parsed by M5.  In the future, some tighter integration between M5
# and the Python interpreter may allow bypassing the .ini file.
#
# Each SimObject class in M5 is represented by a Python class with the
# same name.  The Python inheritance tree mirrors the M5 C++ tree
# (e.g., SimpleCPU derives from BaseCPU in both cases, and all
# SimObjects inherit from a single SimObject base class).  To specify
# an instance of an M5 SimObject in a configuration, the user simply
# instantiates the corresponding Python object.  The parameters for
# that SimObject are given by assigning to attributes of the Python
# object, either using keyword assignment in the constructor or in
# separate assignment statements.  For example:
#
# cache = BaseCache(size='64KB')
# cache.hit_latency = 3
# cache.assoc = 8
#
# The magic lies in the mapping of the Python attributes for SimObject
# classes to the actual SimObject parameter specifications.  This
# allows parameter validity checking in the Python code.  Continuing
# the example above, the statements "cache.blurfl=3" or
# "cache.assoc='hello'" would both result in runtime errors in Python,
# since the BaseCache object has no 'blurfl' parameter and the 'assoc'
# parameter requires an integer, respectively.  This magic is done
# primarily by overriding the special __setattr__ method that controls
# assignment to object attributes.
#
# Once a set of Python objects have been instantiated in a hierarchy,
# calling 'instantiate(obj)' (where obj is the root of the hierarchy)
# will generate a .ini file.  See simple-4cpu.py for an example
# (corresponding to m5-test/simple-4cpu.ini).
#
#####################################################################

#####################################################################
#
# ConfigNode/SimObject classes
#
# The Python class hierarchy rooted by ConfigNode (which is the base
# class of SimObject, which in turn is the base class of all other M5
# SimObject classes) has special attribute behavior.  In general, an
# object in this hierarchy has three categories of attribute-like
# things:
#
# 1. Regular Python methods and variables.  These must start with an
# underscore to be treated normally.
#
# 2. SimObject parameters.  These values are stored as normal Python
# attributes, but all assignments to these attributes are checked
# against the pre-defined set of parameters stored in the class's
# _params dictionary.  Assignments to attributes that do not
# correspond to predefined parameters, or that are not of the correct
# type, incur runtime errors.
#
# 3. Hierarchy children.  The child nodes of a ConfigNode are stored
# in the node's _children dictionary, but can be accessed using the
# Python attribute dot-notation (just as they are printed out by the
# simulator).  Children cannot be created using attribute assigment;
# they must be added by specifying the parent node in the child's
# constructor or using the '+=' operator.

# The SimObject parameters are the most complex, for a few reasons.
# First, both parameter descriptions and parameter values are
# inherited.  Thus parameter description lookup must go up the
# inheritance chain like normal attribute lookup, but this behavior
# must be explicitly coded since the lookup occurs in each class's
# _params attribute.  Second, because parameter values can be set
# on SimObject classes (to implement default values), the parameter
# checking behavior must be enforced on class attribute assignments as
# well as instance attribute assignments.  Finally, because we allow
# class specialization via inheritance (e.g., see the L1Cache class in
# the simple-4cpu.py example), we must do parameter checking even on
# class instantiation.  To provide all these features, we use a
# metaclass to define most of the SimObject parameter behavior for
# this class hierarchy.
#
#####################################################################

def isSimObject(value):
    return isinstance(value, SimObject) 

def isSimObjSequence(value):
    if not isinstance(value, (list, tuple)):
        return False

    for val in value:
        if not isNullPointer(val) and not isSimObject(val):
            return False

    return True

def isNullPointer(value):
    return isinstance(value, NullSimObject)

# The metaclass for ConfigNode (and thus for everything that derives
# from ConfigNode, including SimObject).  This class controls how new
# classes that derive from ConfigNode are instantiated, and provides
# inherited class behavior (just like a class controls how instances
# of that class are instantiated, and provides inherited instance
# behavior).
class MetaSimObject(type):
    # Attributes that can be set only at initialization time
    init_keywords = { 'abstract' : types.BooleanType,
                      'type' : types.StringType }
    # Attributes that can be set any time
    keywords = { 'check' : types.FunctionType,
                 'children' : types.ListType }
    
    # __new__ is called before __init__, and is where the statements
    # in the body of the class definition get loaded into the class's
    # __dict__.  We intercept this to filter out parameter assignments
    # and only allow "private" attributes to be passed to the base
    # __new__ (starting with underscore).
    def __new__(mcls, name, bases, dict):
        # Copy "private" attributes (including special methods such as __new__)
        # to the official dict.  Everything else goes in _init_dict to be
        # filtered in __init__.
        cls_dict = {}
        for key,val in dict.items():
            if key.startswith('_'):
                cls_dict[key] = val
                del dict[key]
        cls_dict['_init_dict'] = dict
        return super(MetaSimObject, mcls).__new__(mcls, name, bases, cls_dict)
        
    # initialization
    def __init__(cls, name, bases, dict):
        super(MetaSimObject, cls).__init__(name, bases, dict)

        # initialize required attributes
        cls._params = multidict()
        cls._values = multidict()
        cls._anon_subclass_counter = 0

        # We don't support multiple inheritance.  If you want to, you
        # must fix multidict to deal with it properly.
        if len(bases) > 1:
            raise TypeError, "SimObjects do not support multiple inheritance"

        base = bases[0]

        if isinstance(base, MetaSimObject):
            cls._params.parent = base._params
            cls._values.parent = base._values

            # If your parent has a value in it that's a config node, clone
            # it.  Do this now so if we update any of the values'
            # attributes we are updating the clone and not the original.
            for key,val in base._values.iteritems():

                # don't clone if (1) we're about to overwrite it with
                # a local setting or (2) we've already cloned a copy
                # from an earlier (more derived) base
                if cls._init_dict.has_key(key) or cls._values.has_key(key):
                    continue

                if isSimObject(val):
                    cls._values[key] = val()
                elif isSimObjSequence(val) and len(val):
                    cls._values[key] = [ v() for v in val ]

        # now process remaining _init_dict items
        for key,val in cls._init_dict.items():
            if isinstance(val, (types.FunctionType, types.TypeType)):
                type.__setattr__(cls, key, val)

            # param descriptions
            elif isinstance(val, ParamDesc):
                cls._new_param(key, val)

            # init-time-only keywords
            elif cls.init_keywords.has_key(key):
                cls._set_keyword(key, val, cls.init_keywords[key])

            # default: use normal path (ends up in __setattr__)
            else:
                setattr(cls, key, val)

    def _set_keyword(cls, keyword, val, kwtype):
        if not isinstance(val, kwtype):
            raise TypeError, 'keyword %s has bad type %s (expecting %s)' % \\
                  (keyword, type(val), kwtype)
        if isinstance(val, types.FunctionType):
            val = classmethod(val)
        type.__setattr__(cls, keyword, val)

    def _new_param(cls, name, value):
        cls._params[name] = value
        if hasattr(value, 'default'):
            setattr(cls, name, value.default)

    # Set attribute (called on foo.attr = value when foo is an
    # instance of class cls).
    def __setattr__(cls, attr, value):
        # normal processing for private attributes
        if attr.startswith('_'):
            type.__setattr__(cls, attr, value)
            return

        if cls.keywords.has_key(attr):
            cls._set_keyword(attr, value, cls.keywords[attr])
            return

        # must be SimObject param
        param = cls._params.get(attr, None)
        if param:
            # It's ok: set attribute by delegating to 'object' class.
            try:
                cls._values[attr] = param.convert(value)
            except Exception, e:
                msg = "%s\\nError setting param %s.%s to %s\\n" % \\
                      (e, cls.__name__, attr, value)
                e.args = (msg, )
                raise
        # I would love to get rid of this
        elif isSimObject(value) or isSimObjSequence(value):
           cls._values[attr] = value
        else:
            raise AttributeError, \\
                  "Class %s has no parameter %s" % (cls.__name__, attr)

    def __getattr__(cls, attr):
        if cls._values.has_key(attr):
            return cls._values[attr]

        raise AttributeError, \\
              "object '%s' has no attribute '%s'" % (cls.__name__, attr)

# The ConfigNode class is the root of the special hierarchy.  Most of
# the code in this class deals with the configuration hierarchy itself
# (parent/child node relationships).
class SimObject(object):
    # Specify metaclass.  Any class inheriting from SimObject will
    # get this metaclass.
    __metaclass__ = MetaSimObject

    def __init__(self, _value_parent = None, **kwargs):
        self._children = {}
        if _value_parent and type(_value_parent) != type(self):
            # this was called as a type conversion rather than a clone
            raise TypeError, "Cannot convert %s to %s" % \\
                  (_value_parent.__class__.__name__, self.__class__.__name__)
        if not _value_parent:
            _value_parent = self.__class__
        # clone values
        self._values = multidict(_value_parent._values)
        for key,val in _value_parent._values.iteritems():
            if isSimObject(val):
                setattr(self, key, val())
            elif isSimObjSequence(val) and len(val):
                setattr(self, key, [ v() for v in val ])
        # apply attribute assignments from keyword args, if any
        for key,val in kwargs.iteritems():
            setattr(self, key, val)

    def __call__(self, **kwargs):
        return self.__class__(_value_parent = self, **kwargs)

    def __getattr__(self, attr):
        if self._values.has_key(attr):
            return self._values[attr]

        raise AttributeError, "object '%s' has no attribute '%s'" \\
              % (self.__class__.__name__, attr)

    # Set attribute (called on foo.attr = value when foo is an
    # instance of class cls).
    def __setattr__(self, attr, value):
        # normal processing for private attributes
        if attr.startswith('_'):
            object.__setattr__(self, attr, value)
            return

        # must be SimObject param
        param = self._params.get(attr, None)
        if param:
            # It's ok: set attribute by delegating to 'object' class.
            try:
                value = param.convert(value)
            except Exception, e:
                msg = "%s\\nError setting param %s.%s to %s\\n" % \\
                      (e, self.__class__.__name__, attr, value)
                e.args = (msg, )
                raise
        # I would love to get rid of this
        elif isSimObject(value) or isSimObjSequence(value):
            pass
        else:
            raise AttributeError, "Class %s has no parameter %s" \\
                  % (self.__class__.__name__, attr)

        # clear out old child with this name, if any
        self.clear_child(attr)

        if isSimObject(value):
            value.set_path(self, attr)
        elif isSimObjSequence(value):
            value = SimObjVector(value)
            [v.set_path(self, "%s%d" % (attr, i)) for i,v in enumerate(value)]

        self._values[attr] = value

    # this hack allows tacking a '[0]' onto parameters that may or may
    # not be vectors, and always getting the first element (e.g. cpus)
    def __getitem__(self, key):
        if key == 0:
            return self
        raise TypeError, "Non-zero index '%s' to SimObject" % key

    # clear out children with given name, even if it's a vector
    def clear_child(self, name):
        if not self._children.has_key(name):
            return
        child = self._children[name]
        if isinstance(child, SimObjVector):
            for i in xrange(len(child)):
                del self._children["s%d" % (name, i)]
        del self._children[name]

    def add_child(self, name, value):
        self._children[name] = value

    def set_path(self, parent, name):
        if not hasattr(self, '_parent'):
            self._parent = parent
            self._name = name
            parent.add_child(name, self)

    def path(self):
        if not hasattr(self, '_parent'):
            return 'root'
        ppath = self._parent.path()
        if ppath == 'root':
            return self._name
        return ppath + "." + self._name

    def __str__(self):
        return self.path()

    def ini_str(self):
        return self.path()

    def find_any(self, ptype):
        if isinstance(self, ptype):
            return self, True

        found_obj = None
        for child in self._children.itervalues():
            if isinstance(child, ptype):
                if found_obj != None and child != found_obj:
                    raise AttributeError, \\
                          'parent.any matched more than one: %s %s' % \\
                          (found_obj.path, child.path)
                found_obj = child
        # search param space
        for pname,pdesc in self._params.iteritems():
            if issubclass(pdesc.ptype, ptype):
                match_obj = self._values[pname]
                if found_obj != None and found_obj != match_obj:
                    raise AttributeError, \\
                          'parent.any matched more than one: %s' % obj.path
                found_obj = match_obj
        return found_obj, found_obj != None

    def unproxy(self, base):
        return self

    def print_ini(self):
        print '[' + self.path() + ']'	# .ini section header

        if hasattr(self, 'type') and not isinstance(self, ParamContext):
            print 'type=%s' % self.type

        child_names = self._children.keys()
        child_names.sort()
        np_child_names = [c for c in child_names \\
                          if not isinstance(self._children[c], ParamContext)]
        if len(np_child_names):
            print 'children=%s' % ' '.join(np_child_names)

        param_names = self._params.keys()
        param_names.sort()
        for param in param_names:
            value = self._values.get(param, None)
            if value != None:
                if isproxy(value):
                    try:
                        value = value.unproxy(self)
                    except:
                        print >> sys.stderr, \\
                              "Error in unproxying param '%s' of %s" % \\
                              (param, self.path())
                        raise
                    setattr(self, param, value)
                print '%s=%s' % (param, self._values[param].ini_str())

        print	# blank line between objects

        for child in child_names:
            self._children[child].print_ini()

    # generate output file for 'dot' to display as a pretty graph.
    # this code is currently broken.
    def outputDot(self, dot):
        label = "{%s|" % self.path
        if isSimObject(self.realtype):
            label +=  '%s|' % self.type

        if self.children:
            # instantiate children in same order they were added for
            # backward compatibility (else we can end up with cpu1
            # before cpu0).
            for c in self.children:
                dot.add_edge(pydot.Edge(self.path,c.path, style="bold"))
          
        simobjs = [] 
        for param in self.params:
            try:
                if param.value is None:
                    raise AttributeError, 'Parameter with no value'

                value = param.value
                string = param.string(value)
            except Exception, e:
                msg = 'exception in %s:%s\\n%s' % (self.name, param.name, e)
                e.args = (msg, )
                raise

            if isSimObject(param.ptype) and string != "Null":
                simobjs.append(string)
            else:
                label += '%s = %s\\\\n' % (param.name, string)
                
        for so in simobjs:
            label += "|<%s> %s" % (so, so)
            dot.add_edge(pydot.Edge("%s:%s" % (self.path, so), so,
                                    tailport="w"))
        label += '}'
        dot.add_node(pydot.Node(self.path,shape="Mrecord",label=label))
        
        # recursively dump out children
        for c in self.children:
            c.outputDot(dot)

class ParamContext(SimObject):
    pass

#####################################################################
#
# Proxy object support.
#
#####################################################################

class BaseProxy(object):
    def __init__(self, search_self, search_up):
        self._search_self = search_self
        self._search_up = search_up
        self._multiplier = None

    def __setattr__(self, attr, value):
        if not attr.startswith('_'):
            raise AttributeError, 'cannot set attribute on proxy object'
        super(BaseProxy, self).__setattr__(attr, value)

    # support multiplying proxies by constants
    def __mul__(self, other):
        if not isinstance(other, (int, float)):
            raise TypeError, "Proxy multiplier must be integer"
        if self._multiplier == None:
            self._multiplier = other
        else:
            # support chained multipliers
            self._multiplier *= other
        return self

    __rmul__ = __mul__

    def _mulcheck(self, result):
        if self._multiplier == None:
            return result
        return result * self._multiplier

    def unproxy(self, base):
        obj = base
        done = False

        if self._search_self:
            result, done = self.find(obj)

        if self._search_up:
            while not done:
                try: obj = obj._parent
                except: break

                result, done = self.find(obj)

        if not done:
            raise AttributeError, "Can't resolve proxy '%s' from '%s'" % \\
                  (self.path(), base.path())

        if isinstance(result, BaseProxy):
            if result == self:
                raise RuntimeError, "Cycle in unproxy"
            result = result.unproxy(obj)

        return self._mulcheck(result)

    def getindex(obj, index):
        if index == None:
            return obj
        try:
            obj = obj[index]
        except TypeError:
            if index != 0:
                raise
            # if index is 0 and item is not subscriptable, just
            # use item itself (so cpu[0] works on uniprocessors)
        return obj
    getindex = staticmethod(getindex)

    def set_param_desc(self, pdesc):
        self._pdesc = pdesc

class AttrProxy(BaseProxy):
    def __init__(self, search_self, search_up, attr):
        super(AttrProxy, self).__init__(search_self, search_up)
        self._attr = attr
        self._modifiers = []
        
    def __getattr__(self, attr):
        # python uses __bases__ internally for inheritance
        if attr.startswith('_'):
            return super(AttrProxy, self).__getattr__(self, attr)
        if hasattr(self, '_pdesc'):
            raise AttributeError, "Attribute reference on bound proxy"
        self._modifiers.append(attr)
        return self

    # support indexing on proxies (e.g., Self.cpu[0])
    def __getitem__(self, key):
        if not isinstance(key, int):
            raise TypeError, "Proxy object requires integer index"
        self._modifiers.append(key)
        return self

    def find(self, obj):
        try:
            val = getattr(obj, self._attr)
        except:
            return None, False
        while isproxy(val):
            val = val.unproxy(obj)
        for m in self._modifiers:
            if isinstance(m, str):
                val = getattr(val, m)
            elif isinstance(m, int):
                val = val[m]
            else:
                assert("Item must be string or integer")
            while isproxy(val):
                val = val.unproxy(obj)
        return val, True

    def path(self):
        p = self._attr
        for m in self._modifiers:
            if isinstance(m, str):
                p += '.%s' % m
            elif isinstance(m, int):
                p += '[%d]' % m
            else:
                assert("Item must be string or integer")
        return p

class AnyProxy(BaseProxy):
    def find(self, obj):
        return obj.find_any(self._pdesc.ptype)

    def path(self):
        return 'any'

def isproxy(obj):
    if isinstance(obj, (BaseProxy, EthernetAddr)):
        return True
    elif isinstance(obj, (list, tuple)):
        for v in obj:
            if isproxy(v):
                return True
    return False

class ProxyFactory(object):
    def __init__(self, search_self, search_up):
        self.search_self = search_self
        self.search_up = search_up

    def __getattr__(self, attr):
        if attr == 'any':
            return AnyProxy(self.search_self, self.search_up)
        else:
            return AttrProxy(self.search_self, self.search_up, attr)

# global objects for handling proxies
Parent = ProxyFactory(search_self = False, search_up = True)
Self = ProxyFactory(search_self = True, search_up = False)

#####################################################################
#
# Parameter description classes
#
# The _params dictionary in each class maps parameter names to
# either a Param or a VectorParam object.  These objects contain the
# parameter description string, the parameter type, and the default
# value (loaded from the PARAM section of the .odesc files).  The
# _convert() method on these objects is used to force whatever value
# is assigned to the parameter to the appropriate type.
#
# Note that the default values are loaded into the class's attribute
# space when the parameter dictionary is initialized (in
# MetaConfigNode._setparams()); after that point they aren't used.
#
#####################################################################

# Dummy base class to identify types that are legitimate for SimObject
# parameters.
class ParamValue(object):

    # default for printing to .ini file is regular string conversion.
    # will be overridden in some cases
    def ini_str(self):
        return str(self)

    # allows us to blithely call unproxy() on things without checking
    # if they're really proxies or not
    def unproxy(self, base):
        return self

# Regular parameter description.
class ParamDesc(object):
    def __init__(self, ptype_str, ptype, *args, **kwargs):
        self.ptype_str = ptype_str
        # remember ptype only if it is provided
        if ptype != None:
            self.ptype = ptype

        if args:
            if len(args) == 1:
                self.desc = args[0]
            elif len(args) == 2:
                self.default = args[0]
                self.desc = args[1]
            else:
                raise TypeError, 'too many arguments'

        if kwargs.has_key('desc'):
            assert(not hasattr(self, 'desc'))
            self.desc = kwargs['desc']
            del kwargs['desc']

        if kwargs.has_key('default'):
            assert(not hasattr(self, 'default'))
            self.default = kwargs['default']
            del kwargs['default']

        if kwargs:
            raise TypeError, 'extra unknown kwargs %s' % kwargs

        if not hasattr(self, 'desc'):
            raise TypeError, 'desc attribute missing'

    def __getattr__(self, attr):
        if attr == 'ptype':
            try:
                ptype = eval(self.ptype_str, m5.__dict__)
                if not isinstance(ptype, type):
                    panic("Param qualifier is not a type: %s" % self.ptype)
                self.ptype = ptype
                return ptype
            except NameError:
                pass
        raise AttributeError, "'%s' object has no attribute '%s'" % \\
              (type(self).__name__, attr)

    def convert(self, value):
        if isinstance(value, BaseProxy):
            value.set_param_desc(self)
            return value
        if not hasattr(self, 'ptype') and isNullPointer(value):
            # deferred evaluation of SimObject; continue to defer if
            # we're just assigning a null pointer
            return value
        if isinstance(value, self.ptype):
            return value
        if isNullPointer(value) and issubclass(self.ptype, SimObject):
            return value
        return self.ptype(value)

# Vector-valued parameter description.  Just like ParamDesc, except
# that the value is a vector (list) of the specified type instead of a
# single value.

class VectorParamValue(list):
    def ini_str(self):
        return ' '.join([str(v) for v in self])

    def unproxy(self, base):
        return [v.unproxy(base) for v in self]

class SimObjVector(VectorParamValue):
    def print_ini(self):
        for v in self:
            v.print_ini()

class VectorParamDesc(ParamDesc):
    # Convert assigned value to appropriate type.  If the RHS is not a
    # list or tuple, it generates a single-element list.
    def convert(self, value):
        if isinstance(value, (list, tuple)):
            # list: coerce each element into new list
            tmp_list = [ ParamDesc.convert(self, v) for v in value ]
            if isSimObjSequence(tmp_list):
                return SimObjVector(tmp_list)
            else:
                return VectorParamValue(tmp_list)
        else:
            # singleton: leave it be (could coerce to a single-element
            # list here, but for some historical reason we don't...
            return ParamDesc.convert(self, value)


class ParamFactory(object):
    def __init__(self, param_desc_class, ptype_str = None):
        self.param_desc_class = param_desc_class
        self.ptype_str = ptype_str

    def __getattr__(self, attr):
        if self.ptype_str:
            attr = self.ptype_str + '.' + attr
        return ParamFactory(self.param_desc_class, attr)

    # E.g., Param.Int(5, "number of widgets")
    def __call__(self, *args, **kwargs):
        caller_frame = inspect.stack()[1][0]
        ptype = None
        try:
            ptype = eval(self.ptype_str,
                         caller_frame.f_globals, caller_frame.f_locals)
            if not isinstance(ptype, type):
                raise TypeError, \\
                      "Param qualifier is not a type: %s" % ptype
        except NameError:
            # if name isn't defined yet, assume it's a SimObject, and
            # try to resolve it later
            pass
        return self.param_desc_class(self.ptype_str, ptype, *args, **kwargs)

Param = ParamFactory(ParamDesc)
VectorParam = ParamFactory(VectorParamDesc)

#####################################################################
#
# Parameter Types
#
# Though native Python types could be used to specify parameter types
# (the 'ptype' field of the Param and VectorParam classes), it's more
# flexible to define our own set of types.  This gives us more control
# over how Python expressions are converted to values (via the
# __init__() constructor) and how these values are printed out (via
# the __str__() conversion method).  Eventually we'll need these types
# to correspond to distinct C++ types as well.
#
#####################################################################

class Range(ParamValue):
    type = int # default; can be overridden in subclasses
    def __init__(self, *args, **kwargs):

        def handle_kwargs(self, kwargs):
            if 'end' in kwargs:
                self.second = self.type(kwargs.pop('end'))
            elif 'size' in kwargs:
                self.second = self.first + self.type(kwargs.pop('size')) - 1
            else:
                raise TypeError, "Either end or size must be specified"

        if len(args) == 0:
            self.first = self.type(kwargs.pop('start'))
            handle_kwargs(self, kwargs)

        elif len(args) == 1:
            if kwargs:
                self.first = self.type(args[0])
                handle_kwargs(self, kwargs)
            elif isinstance(args[0], Range):
                self.first = self.type(args[0].first)
                self.second = self.type(args[0].second)
            else:
                self.first = self.type(0)
                self.second = self.type(args[0]) - 1

        elif len(args) == 2:
            self.first = self.type(args[0])
            self.second = self.type(args[1])
        else:
            raise TypeError, "Too many arguments specified"

        if kwargs:
            raise TypeError, "too many keywords: %s" % kwargs.keys()

    def __str__(self):
        return '%s:%s' % (self.first, self.second)

# Metaclass for bounds-checked integer parameters.  See CheckedInt.
class CheckedIntType(type):
    def __init__(cls, name, bases, dict):
        super(CheckedIntType, cls).__init__(name, bases, dict)

        # CheckedInt is an abstract base class, so we actually don't
        # want to do any processing on it... the rest of this code is
        # just for classes that derive from CheckedInt.
        if name == 'CheckedInt':
            return

        if not (hasattr(cls, 'min') and hasattr(cls, 'max')):
            if not (hasattr(cls, 'size') and hasattr(cls, 'unsigned')):
                panic("CheckedInt subclass %s must define either\\n" \\
                      "    'min' and 'max' or 'size' and 'unsigned'\\n" \\
                      % name);
            if cls.unsigned:
                cls.min = 0
                cls.max = 2 ** cls.size - 1
            else:
                cls.min = -(2 ** (cls.size - 1))
                cls.max = (2 ** (cls.size - 1)) - 1

# Abstract superclass for bounds-checked integer parameters.  This
# class is subclassed to generate parameter classes with specific
# bounds.  Initialization of the min and max bounds is done in the
# metaclass CheckedIntType.__init__.
class CheckedInt(long,ParamValue):
    __metaclass__ = CheckedIntType

    def __new__(cls, value):
        if isinstance(value, str):
            value = toInteger(value)

        self = long.__new__(cls, value)

        if not cls.min <= self <= cls.max:
            raise TypeError, 'Integer param out of bounds %d < %d < %d' % \\
                  (cls.min, self, cls.max)
        return self

class Int(CheckedInt):      size = 32; unsigned = False
class Unsigned(CheckedInt): size = 32; unsigned = True

class Int8(CheckedInt):     size =  8; unsigned = False
class UInt8(CheckedInt):    size =  8; unsigned = True
class Int16(CheckedInt):    size = 16; unsigned = False
class UInt16(CheckedInt):   size = 16; unsigned = True
class Int32(CheckedInt):    size = 32; unsigned = False
class UInt32(CheckedInt):   size = 32; unsigned = True
class Int64(CheckedInt):    size = 64; unsigned = False
class UInt64(CheckedInt):   size = 64; unsigned = True

class Counter(CheckedInt):  size = 64; unsigned = True
class Tick(CheckedInt):     size = 64; unsigned = True
class TcpPort(CheckedInt):  size = 16; unsigned = True
class UdpPort(CheckedInt):  size = 16; unsigned = True

class Percent(CheckedInt):  min = 0; max = 100

class Float(ParamValue, float):
    pass

class MemorySize(CheckedInt):
    size = 64
    unsigned = True
    def __new__(cls, value):
        return super(MemorySize, cls).__new__(cls, toMemorySize(value))


class Addr(CheckedInt):
    size = 64
    unsigned = True
    def __new__(cls, value):
        try:
            value = long(toMemorySize(value))
        except TypeError:
            value = long(value)
        return super(Addr, cls).__new__(cls, value)

class AddrRange(Range):
    type = Addr

# String-valued parameter.  Just mixin the ParamValue class
# with the built-in str class.
class String(ParamValue,str):
    pass

# Boolean parameter type.  Python doesn't let you subclass bool, since
# it doesn't want to let you create multiple instances of True and
# False.  Thus this is a little more complicated than String.
class Bool(ParamValue):
    def __init__(self, value):
        try:
            self.value = toBool(value)
        except TypeError:
            self.value = bool(value)

    def __str__(self):
        return str(self.value)

    def ini_str(self):
        if self.value:
            return 'true'
        return 'false'

def IncEthernetAddr(addr, val = 1):
    bytes = map(lambda x: int(x, 16), addr.split(':'))
    bytes[5] += val
    for i in (5, 4, 3, 2, 1):
        val,rem = divmod(bytes[i], 256)
        bytes[i] = rem
        if val == 0:
            break
        bytes[i - 1] += val
    assert(bytes[0] <= 255)
    return ':'.join(map(lambda x: '%02x' % x, bytes))

class NextEthernetAddr(object):
    addr = "00:90:00:00:00:01"

    def __init__(self, inc = 1):
        self.value = NextEthernetAddr.addr
        NextEthernetAddr.addr = IncEthernetAddr(NextEthernetAddr.addr, inc)

class EthernetAddr(ParamValue):
    def __init__(self, value):
        if value == NextEthernetAddr:
            self.value = value
            return

        if not isinstance(value, str):
            raise TypeError, "expected an ethernet address and didn't get one"

        bytes = value.split(':')
        if len(bytes) != 6:
            raise TypeError, 'invalid ethernet address %s' % value

        for byte in bytes:
            if not 0 <= int(byte) <= 256:
                raise TypeError, 'invalid ethernet address %s' % value

        self.value = value

    def unproxy(self, base):
        if self.value == NextEthernetAddr:
            self.addr = self.value().value
        return self

    def __str__(self):
        if self.value == NextEthernetAddr:
            return self.addr
        else:
            return self.value

# Special class for NULL pointers.  Note the special check in
# make_param_value() above that lets these be assigned where a
# SimObject is required.
# only one copy of a particular node
class NullSimObject(object):
    __metaclass__ = Singleton

    def __call__(cls):
        return cls

    def _instantiate(self, parent = None, path = ''):
        pass
    
    def ini_str(self):
        return 'Null'

    def unproxy(self, base):
        return self
    
    def set_path(self, parent, name):
        pass 
    def __str__(self):
        return 'Null'

# The only instance you'll ever need...
Null = NULL = NullSimObject()

# Enumerated types are a little more complex.  The user specifies the
# type as Enum(foo) where foo is either a list or dictionary of
# alternatives (typically strings, but not necessarily so).  (In the
# long run, the integer value of the parameter will be the list index
# or the corresponding dictionary value.  For now, since we only check
# that the alternative is valid and then spit it into a .ini file,
# there's not much point in using the dictionary.)

# What Enum() must do is generate a new type encapsulating the
# provided list/dictionary so that specific values of the parameter
# can be instances of that type.  We define two hidden internal
# classes (_ListEnum and _DictEnum) to serve as base classes, then
# derive the new type from the appropriate base class on the fly.


# Metaclass for Enum types
class MetaEnum(type):
    def __init__(cls, name, bases, init_dict):
        if init_dict.has_key('map'):
            if not isinstance(cls.map, dict):
                raise TypeError, "Enum-derived class attribute 'map' " \\
                      "must be of type dict"
            # build list of value strings from map
            cls.vals = cls.map.keys()
            cls.vals.sort()
        elif init_dict.has_key('vals'):
            if not isinstance(cls.vals, list):
                raise TypeError, "Enum-derived class attribute 'vals' " \\
                      "must be of type list"
            # build string->value map from vals sequence
            cls.map = {}
            for idx,val in enumerate(cls.vals):
                cls.map[val] = idx
        else:
            raise TypeError, "Enum-derived class must define "\\
                  "attribute 'map' or 'vals'"

        super(MetaEnum, cls).__init__(name, bases, init_dict)

    def cpp_declare(cls):
        s = 'enum %s {\\n    ' % cls.__name__
        s += ',\\n    '.join(['%s = %d' % (v,cls.map[v]) for v in cls.vals])
        s += '\\n};\\n'
        return s

# Base class for enum types.
class Enum(ParamValue):
    __metaclass__ = MetaEnum
    vals = []

    def __init__(self, value):
        if value not in self.map:
            raise TypeError, "Enum param got bad value '%s' (not in %s)" \\
                  % (value, self.vals)
        self.value = value

    def __str__(self):
        return self.value

ticks_per_sec = None

# how big does a rounding error need to be before we warn about it?
frequency_tolerance = 0.001  # 0.1%

# convert a floting-point # of ticks to integer, and warn if rounding
# discards too much precision
def tick_check(float_ticks):
    if float_ticks == 0:
        return 0
    int_ticks = int(round(float_ticks))
    err = (float_ticks - int_ticks) / float_ticks
    if err > frequency_tolerance:
        print >> sys.stderr, "Warning: rounding error > tolerance"
        print >> sys.stderr, "    %f rounded to %d" % (float_ticks, int_ticks)
        #raise ValueError
    return int_ticks

# superclass for "numeric" parameter values, to emulate math
# operations in a type-safe way.  e.g., a Latency times an int returns
# a new Latency object.
class NumericParamValue(ParamValue):
    def __str__(self):
        return str(self.value)

    def __float__(self):
        return float(self.value)

    def __mul__(self, other):
        newobj = self.__class__(self)
        newobj.value *= other
        return newobj

    __rmul__ = __mul__

    def __div__(self, other):
        newobj = self.__class__(self)
        newobj.value /= other
        return newobj


def getLatency(value):
    if isinstance(value, Latency) or isinstance(value, Clock):
        return value.value
    elif isinstance(value, Frequency) or isinstance(value, RootClock):
        return 1 / value.value
    elif isinstance(value, str):
        try:
            return toLatency(value)
        except ValueError:
            try:
                return 1 / toFrequency(value)
            except ValueError:
                pass # fall through
    raise ValueError, "Invalid Frequency/Latency value '%s'" % value


class Latency(NumericParamValue):
    def __init__(self, value):
        self.value = getLatency(value)

    def __getattr__(self, attr):
        if attr in ('latency', 'period'):
            return self
        if attr == 'frequency':
            return Frequency(self)
        raise AttributeError, "Latency object has no attribute '%s'" % attr

    # convert latency to ticks
    def ini_str(self):
        return str(tick_check(self.value * ticks_per_sec))

class Frequency(NumericParamValue):
    def __init__(self, value):
        self.value = 1 / getLatency(value)

    def __getattr__(self, attr):
        if attr == 'frequency':
            return self
        if attr in ('latency', 'period'):
            return Latency(self)
        raise AttributeError, "Frequency object has no attribute '%s'" % attr

    # convert frequency to ticks per period
    def ini_str(self):
        return self.period.ini_str()

# Just like Frequency, except ini_str() is absolute # of ticks per sec (Hz).
# We can't inherit from Frequency because we don't want it to be directly
# assignable to a regular Frequency parameter.
class RootClock(ParamValue):
    def __init__(self, value):
        self.value = 1 / getLatency(value)

    def __getattr__(self, attr):
        if attr == 'frequency':
            return Frequency(self)
        if attr in ('latency', 'period'):
            return Latency(self)
        raise AttributeError, "Frequency object has no attribute '%s'" % attr

    def ini_str(self):
        return str(tick_check(self.value))

# A generic frequency and/or Latency value.  Value is stored as a latency,
# but to avoid ambiguity this object does not support numeric ops (* or /).
# An explicit conversion to a Latency or Frequency must be made first.
class Clock(ParamValue):
    def __init__(self, value):
        self.value = getLatency(value)

    def __getattr__(self, attr):
        if attr == 'frequency':
            return Frequency(self)
        if attr in ('latency', 'period'):
            return Latency(self)
        raise AttributeError, "Frequency object has no attribute '%s'" % attr

    def ini_str(self):
        return self.period.ini_str()

class NetworkBandwidth(float,ParamValue):
    def __new__(cls, value):
        val = toNetworkBandwidth(value) / 8.0
        return super(cls, NetworkBandwidth).__new__(cls, val)

    def __str__(self):
        return str(self.val)

    def ini_str(self):
        return '%f' % (ticks_per_sec / float(self))

class MemoryBandwidth(float,ParamValue):
    def __new__(self, value):
        val = toMemoryBandwidth(value)
        return super(cls, MemoryBandwidth).__new__(cls, val)

    def __str__(self):
        return str(self.val)

    def ini_str(self):
        return '%f' % (ticks_per_sec / float(self))

#
# "Constants"... handy aliases for various values.
#

# Some memory range specifications use this as a default upper bound.
MaxAddr = Addr.max
MaxTick = Tick.max
AllMemory = AddrRange(0, MaxAddr)

#####################################################################

# The final hook to generate .ini files.  Called from configuration
# script once config is built.
def instantiate(root):
    global ticks_per_sec
    ticks_per_sec = float(root.clock.frequency)
    root.print_ini()
    noDot = True # temporary until we fix dot
    if not noDot:
       dot = pydot.Dot()
       instance.outputDot(dot)
       dot.orientation = "portrait"
       dot.size = "8.5,11"
       dot.ranksep="equally"
       dot.rank="samerank"
       dot.write("config.dot")
       dot.write_ps("config.ps")

# __all__ defines the list of symbols that get exported when
# 'from config import *' is invoked.  Try to keep this reasonably
# short to avoid polluting other namespaces.
__all__ = ['SimObject', 'ParamContext', 'Param', 'VectorParam',
           'Parent', 'Self',
           'Enum', 'Bool', 'String', 'Float',
           'Int', 'Unsigned', 'Int8', 'UInt8', 'Int16', 'UInt16',
           'Int32', 'UInt32', 'Int64', 'UInt64',
           'Counter', 'Addr', 'Tick', 'Percent',
           'TcpPort', 'UdpPort', 'EthernetAddr',
           'MemorySize', 'Latency', 'Frequency', 'RootClock', 'Clock',
           'NetworkBandwidth', 'MemoryBandwidth',
           'Range', 'AddrRange', 'MaxAddr', 'MaxTick', 'AllMemory',
           'Null', 'NULL',
           'NextEthernetAddr', 'instantiate']

''')

AddModule(['m5'], 'convert', 'py', 'm5/python/m5/convert.py', '''\
# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

# metric prefixes
exa  = 1.0e18
peta = 1.0e15
tera = 1.0e12
giga = 1.0e9
mega = 1.0e6
kilo = 1.0e3

milli = 1.0e-3
micro = 1.0e-6
nano  = 1.0e-9
pico  = 1.0e-12
femto = 1.0e-15
atto  = 1.0e-18

# power of 2 prefixes
kibi = 1024
mebi = kibi * 1024
gibi = mebi * 1024
tebi = gibi * 1024
pebi = tebi * 1024
exbi = pebi * 1024

# memory size configuration stuff
def toFloat(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('Ei'):
        return float(value[:-2]) * exbi
    elif value.endswith('Pi'):
        return float(value[:-2]) * pebi
    elif value.endswith('Ti'):
        return float(value[:-2]) * tebi
    elif value.endswith('Gi'):
        return float(value[:-2]) * gibi
    elif value.endswith('Mi'):
        return float(value[:-2]) * mebi
    elif value.endswith('ki'):
        return float(value[:-2]) * kibi
    elif value.endswith('E'):
        return float(value[:-1]) * exa
    elif value.endswith('P'):
        return float(value[:-1]) * peta
    elif value.endswith('T'):
        return float(value[:-1]) * tera
    elif value.endswith('G'):
        return float(value[:-1]) * giga
    elif value.endswith('M'):
        return float(value[:-1]) * mega
    elif value.endswith('k'):
        return float(value[:-1]) * kilo
    elif value.endswith('m'):
        return float(value[:-1]) * milli
    elif value.endswith('u'):
        return float(value[:-1]) * micro
    elif value.endswith('n'):
        return float(value[:-1]) * nano
    elif value.endswith('p'):
        return float(value[:-1]) * pico
    elif value.endswith('f'):
        return float(value[:-1]) * femto
    else:
        return float(value)

def toLong(value):
    value = toFloat(value)
    result = int(value)
    if value != result:
        raise ValueError, "cannot convert '%s' to long" % value

    return result

def toInteger(value):
    value = toFloat(value)
    result = int(value)
    if value != result:
        raise ValueError, "cannot convert '%s' to integer" % value

    return result

_bool_dict = {
    'true' : True,   't' : True,  'yes' : True, 'y' : True,  '1' : True,
    'false' : False, 'f' : False, 'no' : False, 'n' : False, '0' : False
    }

def toBool(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    value = value.lower()
    result = _bool_dict.get(value, None)
    if result == None:
        raise ValueError, "cannot convert '%s' to bool" % value
    return result

def toFrequency(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('THz'):
        return float(value[:-3]) * tera
    elif value.endswith('GHz'):
        return float(value[:-3]) * giga
    elif value.endswith('MHz'):
        return float(value[:-3]) * mega
    elif value.endswith('kHz'):
        return float(value[:-3]) * kilo
    elif value.endswith('Hz'):
        return float(value[:-2])

    raise ValueError, "cannot convert '%s' to frequency" % value

def toLatency(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('ps'):
        return float(value[:-2]) * pico
    elif value.endswith('ns'):
        return float(value[:-2]) * nano
    elif value.endswith('us'):
        return float(value[:-2]) * micro
    elif value.endswith('ms'):
        return float(value[:-2]) * milli
    elif value.endswith('s'):
        return float(value[:-1])

    raise ValueError, "cannot convert '%s' to latency" % value

def toClockPeriod(value):
    """result is a clock period"""

    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    try:
        val = toFrequency(value)
        if val != 0:
            val = 1 / val
        return val
    except ValueError:
        pass

    try:
        val = toLatency(value)
        return val
    except ValueError:
        pass

    raise ValueError, "cannot convert '%s' to clock period" % value


def toNetworkBandwidth(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('Tbps'):
        return float(value[:-4]) * tera
    elif value.endswith('Gbps'):
        return float(value[:-4]) * giga
    elif value.endswith('Mbps'):
        return float(value[:-4]) * mega
    elif value.endswith('kbps'):
        return float(value[:-4]) * kilo
    elif value.endswith('bps'):
        return float(value[:-3])
    else:
        return float(value)

    raise ValueError, "cannot convert '%s' to network bandwidth" % value

def toMemoryBandwidth(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('PB/s'):
        return float(value[:-4]) * pebi
    elif value.endswith('TB/s'):
        return float(value[:-4]) * tebi
    elif value.endswith('GB/s'):
        return float(value[:-4]) * gibi
    elif value.endswith('MB/s'):
        return float(value[:-4]) * mebi
    elif value.endswith('kB/s'):
        return float(value[:-4]) * kibi
    elif value.endswith('B/s'):
        return float(value[:-3])

    raise ValueError, "cannot convert '%s' to memory bandwidth" % value

def toMemorySize(value):
    if not isinstance(value, str):
        raise TypeError, "wrong type '%s' should be str" % type(value)

    if value.endswith('PB'):
        return float(value[:-2]) * pebi
    elif value.endswith('TB'):
        return float(value[:-2]) * tebi
    elif value.endswith('GB'):
        return float(value[:-2]) * gibi
    elif value.endswith('MB'):
        return float(value[:-2]) * mebi
    elif value.endswith('kB'):
        return float(value[:-2]) * kibi
    elif value.endswith('B'):
        return float(value[:-1])

    raise ValueError, "cannot convert '%s' to memory size" % value
''')

AddModule(['m5'], 'multidict', 'py', 'm5/python/m5/multidict.py', '''\
# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

__all__ = [ 'multidict' ]

class multidict(object):
    __nodefault = object()
    def __init__(self, parent = {}, **kwargs):
        self.dict = dict(**kwargs)
        self.parent = parent
        self.deleted = {}

    def __str__(self):
        return str(dict(self.items()))
   
    def __repr__(self):
        return `dict(self.items())`

    def __contains__(self, key):
        return self.dict.has_key(key) or self.parent.has_key(key)

    def __delitem__(self, key):
        try:
            del self.dict[key]
        except KeyError, e:
            if key in self.parent:
                self.deleted[key] = True
            else:
                raise KeyError, e

    def __setitem__(self, key, value):
        self.deleted.pop(key, False)
        self.dict[key] = value

    def __getitem__(self, key):
        try:
            return self.dict[key]
        except KeyError, e:
            if not self.deleted.get(key, False) and key in self.parent:
                return self.parent[key]
            else:
                raise KeyError, e

    def __len__(self):
        return len(self.dict) + len(self.parent)

    def next(self):
        for key,value in self.dict.items():
            yield key,value

        if self.parent:
            for key,value in self.parent.next():
                if key not in self.dict and key not in self.deleted:
                    yield key,value  

    def has_key(self, key):
        return key in self

    def iteritems(self):
        for item in self.next():
            yield item

    def items(self):
        return [ item for item in self.next() ]

    def iterkeys(self):
        for key,value in self.next():
            yield key

    def keys(self):
        return [ key for key,value in self.next() ]

    def itervalues(self):
        for key,value in self.next():
            yield value

    def values(self):
        return [ value for key,value in self.next() ]
    
    def get(self, key, default=__nodefault):
        try:
            return self[key]
        except KeyError, e:
            if default != self.__nodefault:
                return default
            else:
                raise KeyError, e
    
    def setdefault(self, key, default):
        try:
            return self[key]
        except KeyError:
            self.deleted.pop(key, False)
            self.dict[key] = default
            return default

    def _dump(self):
        print 'multidict dump'
        node = self
        while isinstance(node, multidict):
            print '    ', node.dict
            node = node.parent

    def _dumpkey(self, key):
        values = []
        node = self
        while isinstance(node, multidict):
            if key in node.dict:
                values.append(node.dict[key])
            node = node.parent
        print key, values

if __name__ == '__main__':
    test1 = multidict()
    test2 = multidict(test1)
    test3 = multidict(test2)
    test4 = multidict(test3)

    test1['a'] = 'test1_a'
    test1['b'] = 'test1_b'
    test1['c'] = 'test1_c'
    test1['d'] = 'test1_d'
    test1['e'] = 'test1_e'

    test2['a'] = 'test2_a'
    del test2['b']
    test2['c'] = 'test2_c'
    del test1['a']

    test2.setdefault('f', multidict)

    print 'test1>', test1.items()
    print 'test2>', test2.items()
    #print test1['a']
    print test1['b']
    print test1['c']
    print test1['d']
    print test1['e']
    
    print test2['a']
    #print test2['b']
    print test2['c']
    print test2['d']
    print test2['e']
    
    for key in test2.iterkeys():
        print key

    test2.get('g', 'foo')
    #test2.get('b')
    test2.get('b', 'bar')
    test2.setdefault('b', 'blah')
    print test1
    print test2
    print `test2`

    print len(test2)

    test3['a'] = [ 0, 1, 2, 3 ]

    print test4
''')

AddModule(['m5', 'objects'], 'AlphaConsole', 'py', 'm5/python/m5/objects/AlphaConsole.py', '''\
from m5 import *
from Device import PioDevice

class AlphaConsole(PioDevice):
    type = 'AlphaConsole'
    cpu = Param.BaseCPU(Parent.any, "Processor")
    disk = Param.SimpleDisk("Simple Disk")
    sim_console = Param.SimConsole(Parent.any, "The Simulator Console")
    system = Param.System(Parent.any, "system object")
''')

AddModule(['m5', 'objects'], 'AlphaFullCPU', 'py', 'm5/python/m5/objects/AlphaFullCPU.py', '''\
from m5 import *
from BaseCPU import BaseCPU

class DerivAlphaFullCPU(BaseCPU):
    type = 'DerivAlphaFullCPU'

    numThreads = Param.Unsigned("number of HW thread contexts")

    if not build_env['FULL_SYSTEM']:
        mem = Param.FunctionalMemory(NULL, "memory")    

    decodeToFetchDelay = Param.Unsigned("Decode to fetch delay")
    renameToFetchDelay = Param.Unsigned("Rename to fetch delay")
    iewToFetchDelay = Param.Unsigned("Issue/Execute/Writeback to fetch "
               "delay")
    commitToFetchDelay = Param.Unsigned("Commit to fetch delay")
    fetchWidth = Param.Unsigned("Fetch width")
    
    renameToDecodeDelay = Param.Unsigned("Rename to decode delay")
    iewToDecodeDelay = Param.Unsigned("Issue/Execute/Writeback to decode "
               "delay")
    commitToDecodeDelay = Param.Unsigned("Commit to decode delay")
    fetchToDecodeDelay = Param.Unsigned("Fetch to decode delay")
    decodeWidth = Param.Unsigned("Decode width")
    
    iewToRenameDelay = Param.Unsigned("Issue/Execute/Writeback to rename "
               "delay")
    commitToRenameDelay = Param.Unsigned("Commit to rename delay")
    decodeToRenameDelay = Param.Unsigned("Decode to rename delay")
    renameWidth = Param.Unsigned("Rename width")
    
    commitToIEWDelay = Param.Unsigned("Commit to "
               "Issue/Execute/Writeback delay")
    renameToIEWDelay = Param.Unsigned("Rename to "
               "Issue/Execute/Writeback delay")
    issueToExecuteDelay = Param.Unsigned("Issue to execute delay (internal "
              "to the IEW stage)")
    issueWidth = Param.Unsigned("Issue width")
    executeWidth = Param.Unsigned("Execute width")
    executeIntWidth = Param.Unsigned("Integer execute width")
    executeFloatWidth = Param.Unsigned("Floating point execute width")
    executeBranchWidth = Param.Unsigned("Branch execute width")
    executeMemoryWidth = Param.Unsigned("Memory execute width")
    
    iewToCommitDelay = Param.Unsigned("Issue/Execute/Writeback to commit "
               "delay")
    renameToROBDelay = Param.Unsigned("Rename to reorder buffer delay")
    commitWidth = Param.Unsigned("Commit width")
    squashWidth = Param.Unsigned("Squash width")

    local_predictor_size = Param.Unsigned("Size of local predictor")
    local_ctr_bits = Param.Unsigned("Bits per counter")
    local_history_table_size = Param.Unsigned("Size of local history table")
    local_history_bits = Param.Unsigned("Bits for the local history")
    global_predictor_size = Param.Unsigned("Size of global predictor")
    global_ctr_bits = Param.Unsigned("Bits per counter")
    global_history_bits = Param.Unsigned("Bits of history")
    choice_predictor_size = Param.Unsigned("Size of choice predictor")
    choice_ctr_bits = Param.Unsigned("Bits of choice counters")

    BTBEntries = Param.Unsigned("Number of BTB entries")
    BTBTagSize = Param.Unsigned("Size of the BTB tags, in bits")

    RASSize = Param.Unsigned("RAS size")

    LQEntries = Param.Unsigned("Number of load queue entries")
    SQEntries = Param.Unsigned("Number of store queue entries")
    LFSTSize = Param.Unsigned("Last fetched store table size")
    SSITSize = Param.Unsigned("Store set ID table size")

    numPhysIntRegs = Param.Unsigned("Number of physical integer registers")
    numPhysFloatRegs = Param.Unsigned("Number of physical floating point "
               "registers")
    numIQEntries = Param.Unsigned("Number of instruction queue entries")
    numROBEntries = Param.Unsigned("Number of reorder buffer entries")

    instShiftAmt = Param.Unsigned("Number of bits to shift instructions by")

    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")
''')

AddModule(['m5', 'objects'], 'AlphaTLB', 'py', 'm5/python/m5/objects/AlphaTLB.py', '''\
from m5 import *
class AlphaTLB(SimObject):
    type = 'AlphaTLB'
    abstract = True
    size = Param.Int("TLB size")

class AlphaDTB(AlphaTLB):
    type = 'AlphaDTB'
    size = 64

class AlphaITB(AlphaTLB):
    type = 'AlphaITB'
    size = 48
''')

AddModule(['m5', 'objects'], 'BadDevice', 'py', 'm5/python/m5/objects/BadDevice.py', '''\
from m5 import *
from Device import PioDevice

class BadDevice(PioDevice):
    type = 'BadDevice'
    devicename = Param.String("Name of device to error on")
''')

AddModule(['m5', 'objects'], 'BaseCPU', 'py', 'm5/python/m5/objects/BaseCPU.py', '''\
from m5 import *
class BaseCPU(SimObject):
    type = 'BaseCPU'
    abstract = True
    icache = Param.BaseMem(NULL, "L1 instruction cache object")
    dcache = Param.BaseMem(NULL, "L1 data cache object")

    if build_env['FULL_SYSTEM']:
        dtb = Param.AlphaDTB("Data TLB")
        itb = Param.AlphaITB("Instruction TLB")
        mem = Param.FunctionalMemory("memory")
        system = Param.System(Parent.any, "system object")
        cpu_id = Param.Int(-1, "CPU identifier")
    else:
        workload = VectorParam.Process("processes to run")

    max_insts_all_threads = Param.Counter(0,
        "terminate when all threads have reached this inst count")
    max_insts_any_thread = Param.Counter(0,
        "terminate when any thread reaches this inst count")
    max_loads_all_threads = Param.Counter(0,
        "terminate when all threads have reached this load count")
    max_loads_any_thread = Param.Counter(0,
        "terminate when any thread reaches this load count")

    defer_registration = Param.Bool(False,
        "defer registration with system (for sampling)")

    clock = Param.Clock(Parent.clock, "clock speed")
''')

AddModule(['m5', 'objects'], 'BaseCache', 'py', 'm5/python/m5/objects/BaseCache.py', '''\
from m5 import *
from BaseMem import BaseMem

class Prefetch(Enum): vals = ['none', 'tagged', 'stride', 'ghb']

class BaseCache(BaseMem):
    type = 'BaseCache'
    adaptive_compression = Param.Bool(False,
        "Use an adaptive compression scheme")
    assoc = Param.Int("associativity")
    block_size = Param.Int("block size in bytes")
    compressed_bus = Param.Bool(False,
        "This cache connects to a compressed memory")
    compression_latency = Param.Latency('0ns',
        "Latency in cycles of compression algorithm")
    do_copy = Param.Bool(False, "perform fast copies in the cache")
    hash_delay = Param.Int(1, "time in cycles of hash access")
    in_bus = Param.Bus(NULL, "incoming bus object")
    lifo = Param.Bool(False, 
	"whether this NIC partition should use LIFO repl. policy")
    max_miss_count = Param.Counter(0,
        "number of misses to handle before calling exit")
    mem_trace = Param.MemTraceWriter(NULL,
                                     "memory trace writer to record accesses")
    mshrs = Param.Int("number of MSHRs (max outstanding requests)")
    out_bus = Param.Bus("outgoing bus object")
    prioritizeRequests = Param.Bool(False,
        "always service demand misses first")
    protocol = Param.CoherenceProtocol(NULL, "coherence protocol to use")
    repl = Param.Repl(NULL, "replacement policy")
    size = Param.MemorySize("capacity in bytes")
    split = Param.Bool(False, "whether or not this cache is split")
    split_size = Param.Int(0, 
	"How many ways of the cache belong to CPU/LRU partition")
    store_compressed = Param.Bool(False,
        "Store compressed data in the cache")
    subblock_size = Param.Int(0,
        "Size of subblock in IIC used for compression")
    tgts_per_mshr = Param.Int("max number of accesses per MSHR")
    trace_addr = Param.Addr(0, "address to trace")
    two_queue = Param.Bool(False, 
	"whether the lifo should have two queue replacement")
    write_buffers = Param.Int(8, "number of write buffers")
    prefetch_miss = Param.Bool(False,
         "wheter you are using the hardware prefetcher from Miss stream")
    prefetch_access = Param.Bool(False,
         "wheter you are using the hardware prefetcher from Access stream")
    prefetcher_size = Param.Int(100, 
         "Number of entries in the harware prefetch queue")
    prefetch_past_page = Param.Bool(False,
         "Allow prefetches to cross virtual page boundaries")
    prefetch_serial_squash = Param.Bool(False,
         "Squash prefetches with a later time on a subsequent miss")
    prefetch_degree = Param.Int(1,
         "Degree of the prefetch depth")
    prefetch_latency = Param.Tick(10,
         "Latency of the prefetcher")
    prefetch_policy = Param.Prefetch('none',
         "Type of prefetcher to use")
    prefetch_cache_check_push = Param.Bool(True,
         "Check if in cash on push or pop of prefetch queue")
    prefetch_use_cpu_id = Param.Bool(True,
         "Use the CPU ID to seperate calculations of prefetches")
    prefetch_data_accesses_only = Param.Bool(False,
         "Only prefetch on data not on instruction accesses")
''')

AddModule(['m5', 'objects'], 'BaseHier', 'py', 'm5/python/m5/objects/BaseHier.py', '''\
from m5 import *
class BaseHier(SimObject):
    type = 'BaseHier'
    abstract = True
    hier = Param.HierParams(Parent.any, "Hierarchy global variables")
''')

AddModule(['m5', 'objects'], 'BaseMem', 'py', 'm5/python/m5/objects/BaseMem.py', '''\
from m5 import *
from BaseHier import BaseHier

class BaseMem(BaseHier):
    type = 'BaseMem'
    abstract = True
    addr_range = VectorParam.AddrRange(AllMemory, "The address range in bytes")
    latency = Param.Latency('0ns', "latency")
''')

AddModule(['m5', 'objects'], 'BaseMemory', 'py', 'm5/python/m5/objects/BaseMemory.py', '''\
from m5 import *
from BaseMem import BaseMem

class BaseMemory(BaseMem):
    type = 'BaseMemory'
    abstract = True
    compressed = Param.Bool(False, "This memory stores compressed data.")
    do_writes = Param.Bool(False, "update memory")
    in_bus = Param.Bus(NULL, "incoming bus object")
    snarf_updates = Param.Bool(True,
        "update memory on cache-to-cache transfers")
    uncacheable_latency = Param.Latency('0ns', "uncacheable latency")
    if build_env['FULL_SYSTEM']:
        func_mem = Param.FunctionalMemory(Parent.physmem,
                               "corresponding functional memory object")
''')

AddModule(['m5', 'objects'], 'BranchPred', 'py', 'm5/python/m5/objects/BranchPred.py', '''\
from m5 import *
class BranchPred(SimObject):
    type = 'BranchPred'

    class PredictorType(Enum): vals = ['resetting', 'saturating']
    class PredictorClass(Enum): vals = ['hybrid', 'global', 'local']

    btb_assoc = Param.Int("BTB associativity")
    btb_size = Param.Int("number of entries in BTB")
    choice_index_bits = Param.Int(0, "choice predictor index bits")
    choice_xor = Param.Bool(False, "XOR choice hist w/PC (False: concatenate)")
    conf_pred_ctr_bits = Param.Int(0, "confidence predictor counter bits")
    conf_pred_ctr_thresh = Param.Int(0, "confidence predictor threshold")
    conf_pred_ctr_type = Param.PredictorType('saturating',
                                             "confidence predictor type")
    conf_pred_enable = Param.Bool(False, "enable confidence predictor")
    conf_pred_index_bits = Param.Int(0, "confidence predictor index bits")
    conf_pred_xor = Param.Bool(False, "XOR confidence predictor bits")
    global_hist_bits = Param.Int(0, "global predictor history reg bits")
    global_index_bits = Param.Int(0, "global predictor index bits")
    global_xor = Param.Bool(False, "XOR global hist w/PC (False: concatenate)")
    local_hist_bits = Param.Int(0, "local predictor history reg bits")
    local_hist_regs = Param.Int(0, "num. local predictor history regs")
    local_index_bits = Param.Int(0, "local predictor index bits")
    local_xor = Param.Bool(False, "XOR local hist w/PC (False: concatenate)")
    pred_class = Param.PredictorClass("predictor class")
    ras_size = Param.Int("return address stack size")
''')

AddModule(['m5', 'objects'], 'Bus', 'py', 'm5/python/m5/objects/Bus.py', '''\
from m5 import *
from BaseHier import BaseHier

class Bus(BaseHier):
    type = 'Bus'
    clock = Param.Clock("bus frequency")
    width = Param.Int("bus width in bytes")
''')

AddModule(['m5', 'objects'], 'BusBridge', 'py', 'm5/python/m5/objects/BusBridge.py', '''\
from m5 import *
from BaseHier import BaseHier

class BusBridge(BaseHier):
    type = 'BusBridge'
    max_buffer = Param.Int(8, "The number of requests to buffer")
    in_bus = Param.Bus("The bus to forward from")
    out_bus = Param.Bus("The bus to forward to")
    latency = Param.Latency('0ns', "The latency of this bridge")
    ack_writes = Param.Bool(False, "Should this bridge ack writes")
    ack_delay = Param.Latency('0ns', "The latency till the bridge acks a write")
''')

AddModule(['m5', 'objects'], 'CoherenceProtocol', 'py', 'm5/python/m5/objects/CoherenceProtocol.py', '''\
from m5 import *
class Coherence(Enum): vals = ['uni', 'msi', 'mesi', 'mosi', 'moesi']

class CoherenceProtocol(SimObject):
    type = 'CoherenceProtocol'
    do_upgrades = Param.Bool(True, "use upgrade transactions?")
    protocol = Param.Coherence("name of coherence protocol")
''')

AddModule(['m5', 'objects'], 'Debug', 'py', 'm5/python/m5/objects/Debug.py', '''\
from m5 import *
class Debug(ParamContext):
    type = 'Debug'
    break_cycles = VectorParam.Tick("cycle(s) to create breakpoints")
''')

AddModule(['m5', 'objects'], 'DerivOoOCPU', 'py', 'm5/python/m5/objects/DerivOoOCPU.py', '''\
from m5 import *
from BaseCPU import BaseCPU

class DerivOoOCPU(BaseCPU):
    type = 'DerivOoOCPU'
    width = Param.Int(1, "CPU width")
    issueWidth = Param.Int(1, "CPU issue width")
    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")

    def check(self):
        has_workload = self._hasvalue('workload')
        has_dtb = self._hasvalue('dtb')
        has_itb = self._hasvalue('itb')
        has_mem = self._hasvalue('mem')
        has_system = self._hasvalue('system')

        if has_workload:
            self.dtb.disable = True
            self.itb.disable = True
            self.mem.disable = True
            self.system.disable = True
            self.mult.disable = True

        if has_dtb or has_itb or has_mem or has_system:
            self.workload.disable = True
''')

AddModule(['m5', 'objects'], 'Device', 'py', 'm5/python/m5/objects/Device.py', '''\
from m5 import *
from FunctionalMemory import FunctionalMemory

# This device exists only because there are some devices that I don't
# want to have a Platform parameter because it would cause a cycle in
# the C++ that cannot be easily solved.
#
# The real solution to this problem is to pass the ParamXXX structure
# to the constructor, but with the express condition that SimObject
# parameter values are not to be available at construction time.  If
# some further configuration must be done, it must be done during the
# initialization phase at which point all SimObject pointers will be
# valid.
class FooPioDevice(FunctionalMemory):
    type = 'PioDevice'
    abstract = True
    addr = Param.Addr("Device Address")
    mmu = Param.MemoryController(Parent.any, "Memory Controller")
    io_bus = Param.Bus(NULL, "The IO Bus to attach to")
    pio_latency = Param.Tick(1, "Programmed IO latency in bus cycles")

class FooDmaDevice(FooPioDevice):
    type = 'DmaDevice'
    abstract = True

class PioDevice(FooPioDevice):
    type = 'PioDevice'
    abstract = True
    platform = Param.Platform(Parent.any, "Platform")

class DmaDevice(PioDevice):
    type = 'DmaDevice'
    abstract = True

''')

AddModule(['m5', 'objects'], 'DiskImage', 'py', 'm5/python/m5/objects/DiskImage.py', '''\
from m5 import *
class DiskImage(SimObject):
    type = 'DiskImage'
    abstract = True
    image_file = Param.String("disk image file")
    read_only = Param.Bool(False, "read only image")

class RawDiskImage(DiskImage):
    type = 'RawDiskImage'

class CowDiskImage(DiskImage):
    type = 'CowDiskImage'
    child = Param.DiskImage("child image")
    table_size = Param.Int(65536, "initial table size")
    image_file = ''
''')

AddModule(['m5', 'objects'], 'DmaEngine', 'py', 'm5/python/m5/objects/DmaEngine.py', '''\
from m5 import *
class DmaEngine(SimObject):
    type = 'DmaEngine'
    bus = Param.Bus(NULL, "bus that we're attached to")
    channels = Param.Int(36, "number of dma channels")
    physmem = Param.PhysicalMemory(Parent.any, "physical memory")
    no_allocate = Param.Bool(True, "Should we allocate cache on read")
''')

AddModule(['m5', 'objects'], 'Ethernet', 'py', 'm5/python/m5/objects/Ethernet.py', '''\
from m5 import *
from Device import DmaDevice
from Pci import PciDevice

class EtherInt(SimObject):
    type = 'EtherInt'
    abstract = True
    peer = Param.EtherInt(NULL, "peer interface")

class EtherLink(SimObject):
    type = 'EtherLink'
    int1 = Param.EtherInt("interface 1")
    int2 = Param.EtherInt("interface 2")
    delay = Param.Latency('0us', "packet transmit delay")
    speed = Param.NetworkBandwidth('1Gbps', "link speed")
    dump = Param.EtherDump(NULL, "dump object")

class EtherBus(SimObject):
    type = 'EtherBus'
    loopback = Param.Bool(True, "send packet back to the sending interface")
    dump = Param.EtherDump(NULL, "dump object")
    speed = Param.NetworkBandwidth('100Mbps', "bus speed in bits per second")

class EtherTap(EtherInt):
    type = 'EtherTap'
    bufsz = Param.Int(10000, "tap buffer size")
    dump = Param.EtherDump(NULL, "dump object")
    port = Param.UInt16(3500, "tap port")

class EtherDump(SimObject):
    type = 'EtherDump'
    file = Param.String("dump file")
    maxlen = Param.Int(96, "max portion of packet data to dump")

if build_env['ALPHA_TLASER']:

    class EtherDev(DmaDevice):
        type = 'EtherDev'
        hardware_address = Param.EthernetAddr(NextEthernetAddr,
            "Ethernet Hardware Address")

        dma_data_free = Param.Bool(False, "DMA of Data is free")
        dma_desc_free = Param.Bool(False, "DMA of Descriptors is free")
        dma_read_delay = Param.Latency('0us', "fixed delay for dma reads")
        dma_read_factor = Param.Latency('0us', "multiplier for dma reads")
        dma_write_delay = Param.Latency('0us', "fixed delay for dma writes")
        dma_write_factor = Param.Latency('0us', "multiplier for dma writes")
        dma_no_allocate = Param.Bool(True, "Should we allocate cache on read")

        rx_filter = Param.Bool(True, "Enable Receive Filter")
        rx_delay = Param.Latency('1us', "Receive Delay")
        tx_delay = Param.Latency('1us', "Transmit Delay")

        intr_delay = Param.Latency('0us', "Interrupt Delay")
        payload_bus = Param.Bus(NULL, "The IO Bus to attach to for payload")
        physmem = Param.PhysicalMemory(Parent.any, "Physical Memory")
        tlaser = Param.Turbolaser(Parent.any, "Turbolaser")

    class EtherDevInt(EtherInt):
        type = 'EtherDevInt'
        device = Param.EtherDev("Ethernet device of this interface")

class NSGigE(PciDevice):
    type = 'NSGigE'
    hardware_address = Param.EthernetAddr(NextEthernetAddr,
        "Ethernet Hardware Address")

    clock = Param.Clock('0ns', "State machine processor frequency")

    dma_data_free = Param.Bool(False, "DMA of Data is free")
    dma_desc_free = Param.Bool(False, "DMA of Descriptors is free")
    dma_read_delay = Param.Latency('0us', "fixed delay for dma reads")
    dma_read_factor = Param.Latency('0us', "multiplier for dma reads")
    dma_write_delay = Param.Latency('0us', "fixed delay for dma writes")
    dma_write_factor = Param.Latency('0us', "multiplier for dma writes")
    dma_no_allocate = Param.Bool(True, "Should we allocate cache on read")


    rx_filter = Param.Bool(True, "Enable Receive Filter")
    rx_delay = Param.Latency('1us', "Receive Delay")
    tx_delay = Param.Latency('1us', "Transmit Delay")

    rx_fifo_size = Param.MemorySize('128kB', "max size in bytes of rxFifo")
    tx_fifo_size = Param.MemorySize('128kB', "max size in bytes of txFifo")

    m5reg = Param.UInt32(0, "Register for m5 usage")

    intr_delay = Param.Latency('0us', "Interrupt Delay in microseconds")
    payload_bus = Param.Bus(NULL, "The IO Bus to attach to for payload")
    physmem = Param.PhysicalMemory(Parent.any, "Physical Memory")

class NSGigEInt(EtherInt):
    type = 'NSGigEInt'
    device = Param.NSGigE("Ethernet device of this interface")

class Sinic(PciDevice):
    type = 'Sinic'
    hardware_address = Param.EthernetAddr(NextEthernetAddr,
        "Ethernet Hardware Address")

    clock = Param.Clock('100MHz', "State machine processor frequency")

    dma_read_delay = Param.Latency('0us', "fixed delay for dma reads")
    dma_read_factor = Param.Latency('0us', "multiplier for dma reads")
    dma_write_delay = Param.Latency('0us', "fixed delay for dma writes")
    dma_write_factor = Param.Latency('0us', "multiplier for dma writes")

    rx_filter = Param.Bool(True, "Enable Receive Filter")
    rx_delay = Param.Latency('1us', "Receive Delay")
    tx_delay = Param.Latency('1us', "Transmit Delay")

    rx_max_copy = Param.MemorySize('16kB', "rx max copy")
    tx_max_copy = Param.MemorySize('16kB', "tx max copy")
    rx_fifo_size = Param.MemorySize('64kB', "max size of rx fifo")
    tx_fifo_size = Param.MemorySize('64kB', "max size of tx fifo")
    rx_fifo_threshold = Param.MemorySize('48kB', "rx fifo high threshold")
    tx_fifo_threshold = Param.MemorySize('16kB', "tx fifo low threshold")

    intr_delay = Param.Latency('0us', "Interrupt Delay in microseconds")
    payload_bus = Param.Bus(NULL, "The IO Bus to attach to for payload")
    physmem = Param.PhysicalMemory(Parent.any, "Physical Memory")

class SinicInt(EtherInt):
    type = 'SinicInt'
    device = Param.Sinic("Ethernet device of this interface")
''')

AddModule(['m5', 'objects'], 'ExeTrace', 'py', 'm5/python/m5/objects/ExeTrace.py', '''\
from m5 import *
class ExecutionTrace(ParamContext):
    type = 'ExecutionTrace'
    speculative = Param.Bool(False, "capture speculative instructions")
    print_cycle = Param.Bool(True, "print cycle number")
    print_opclass = Param.Bool(True, "print op class")
    print_thread = Param.Bool(True, "print thread number")
    print_effaddr = Param.Bool(True, "print effective address")
    print_data = Param.Bool(True, "print result data")
    print_iregs = Param.Bool(False, "print all integer regs")
    print_fetchseq = Param.Bool(False, "print fetch sequence number")
    print_cpseq = Param.Bool(False, "print correct-path sequence number")
''')

AddModule(['m5', 'objects'], 'FastCPU', 'py', 'm5/python/m5/objects/FastCPU.py', '''\
from m5 import *
from BaseCPU import BaseCPU

class FastCPU(BaseCPU):
    type = 'FastCPU'
''')

AddModule(['m5', 'objects'], 'FetchTrace', 'py', 'm5/python/m5/objects/FetchTrace.py', '''\
from m5 import *
class FetchTrace(ParamContext):
    type = 'FetchTrace'
    trace = VectorParam.String("dump trace of fetch activity")
''')

AddModule(['m5', 'objects'], 'FreebsdSystem', 'py', 'm5/python/m5/objects/FreebsdSystem.py', '''\
from m5 import *
from System import System

class FreebsdSystem(System):
    type = 'FreebsdSystem'
    system_type = 34
    system_rev = 1 << 10
''')

AddModule(['m5', 'objects'], 'FullCPU', 'py', 'm5/python/m5/objects/FullCPU.py', '''\
from m5 import *
from BaseCPU import BaseCPU

class OpType(Enum):
    vals = ['(null)', 'IntAlu', 'IntMult', 'IntDiv', 'FloatAdd',
            'FloatCmp', 'FloatCvt', 'FloatMult', 'FloatDiv', 'FloatSqrt',
            'MemRead', 'MemWrite', 'IprAccess', 'InstPrefetch']

class OpDesc(SimObject):
    type = 'OpDesc'
    issueLat = Param.Int(1, "cycles until another can be issued")
    opClass = Param.OpType("type of operation")
    opLat = Param.Int(1, "cycles until result is available")

class FUDesc(SimObject):
    type = 'FUDesc'
    count = Param.Int("number of these FU's available")
    opList = VectorParam.OpDesc("operation classes for this FU type")

class FuncUnitPool(SimObject):
    type = 'FuncUnitPool'
    FUList = VectorParam.FUDesc("list of FU's for this pool")

class BaseIQ(SimObject):
    type = 'BaseIQ'
    abstract = True
    caps = VectorParam.Int("IQ caps")

class StandardIQ(BaseIQ):
    type = 'StandardIQ'
    prioritized_issue = Param.Bool(False, "thread priorities in issue")
    size = Param.Int("number of entries")

class SegmentedIQ(BaseIQ):
    type = 'SegmentedIQ'
    en_thread_priority = Param.Bool("enable thread priority")
    max_chain_depth = Param.Int("max chain depth")
    num_segments = Param.Int("number of IQ segments")
    segment_size = Param.Int("segment size")
    segment_thresh = Param.Int("segment delta threshold")
    use_bypassing = Param.Bool(True, "enable bypass at dispatch")
    use_pipelined_prom = Param.Bool(True, "enable pipelined chain wires")
    use_pushdown = Param.Bool(True, "enable instruction pushdown")

class SeznecIQ(BaseIQ):
    type = 'SeznecIQ'
    issue_buf_size = Param.Int("number of issue buffer entries")
    line_size = Param.Int("number of insts per prescheduling line")
    num_lines = Param.Int("number of prescheduling lines")
    use_hm_predictor = Param.Bool("use hit/miss predictor")

class FullCPU(BaseCPU):
    type = 'FullCPU'
    branch_pred = Param.BranchPred("branch predictor object")
    class ChainPolicy(Enum):
        vals = ['OneToOne', 'Static', 'StaticStall', 'Dynamic']
    chain_wire_policy = Param.ChainPolicy('OneToOne',
                                          "chain-wire assignment policy")
    class CommitModel(Enum): vals = ['smt', 'perthread', 'sscalar', 'rr']
    commit_model = Param.CommitModel('smt',"commit model")
    commit_width = Param.Int(Self.width,
			     "instruction commit BW (insts/cycle)")
    decode_to_dispatch = Param.Int("decode to dispatch latency (cycles)")
    decode_width = Param.Int(Self.width,
			     "instruction decode BW (insts/cycle)")
    class MemDisambig(Enum): vals = ['conservative', 'normal', 'oracle']
    disambig_mode = Param.MemDisambig('normal',
        "memory address disambiguation mode")
    class DispatchPolicy(Enum): vals = ['mod_n', 'perqueue', 'dependence']
    dispatch_policy = Param.DispatchPolicy('mod_n',
        "method for selecting destination IQ")
    dispatch_to_issue = Param.Int(1,
        "minimum dispatch to issue latency (cycles)")
    fault_handler_delay = Param.Int(5,
	"latency from commit of faulting inst to fetch of handler")
    fetch_branches = Param.Int("stop fetching after 'n'-th branch")
    class FetchPolicy(Enum):
        vals = ['RR', 'IC', 'ICRC', 'Ideal', 'Conf', 'Redundant',
                'RedIC', 'RedSlack', 'Rand']
    fetch_policy = Param.FetchPolicy('IC', "SMT fetch policy")
    fetch_pri_enable = Param.Bool(False, "use thread priorities in fetch")
    fetch_width = Param.Int(Self.width, "instruction fetch BW (insts/cycle)")
    fupools = VectorParam.FuncUnitPool("list of FU pools")
    icount_bias = VectorParam.Int([], "per-thread static icount bias")
    ifq_size = Param.Int("instruction fetch queue size (in insts)")
    inorder_issue = Param.Bool(False, "issue instruction inorder")
    iq = VectorParam.BaseIQ("instruction queue object")
    iq_comm_latency = Param.Int(1,
        "writeback communication latency (cycles) for multiple IQ's")
    issue_bandwidth = VectorParam.Int([], "maximum per-thread issue rate")
    issue_width = Param.Int(Self.width, "instruction issue B/W (insts/cycle)")
    lines_to_fetch = Param.Int(999, "instruction fetch BW (lines/cycle)")
    loose_mod_n_policy = Param.Bool(True,
        "loosen the Mod-N dispatch policy")
    lsq_size = Param.Int("load/store queue size")
    max_chains = Param.Int(64, "maximum number of dependence chains")
    max_wires = Param.Int(64, "maximum number of dependence chain wires")
    mispred_recover = Param.Int("branch misprediction recovery latency")
    mt_frontend = Param.Bool(True, "use the multi-threaded IFQ and FTDQ")
    num_icache_ports = Param.Int("number of icache ports")
    num_threads = Param.Int(0,
        "number of HW thread contexts (0 = # of processes)")
    pc_sample_interval = Param.Latency('0ns', "PC sample interval")
    prioritized_commit = Param.Bool(False,
        "use thread priorities in commit")
    prioritized_issue = Param.Bool(False, "issue HP thread first")
    ptrace = Param.PipeTrace(NULL, "pipeline tracing object")
    rob_caps = VectorParam.Int([], "maximum per-thread ROB occupancy")
    rob_size = Param.Int("reorder buffer size")
    storebuffer_size = Param.Int("store buffer size")
    class PrefetchPolicy(Enum):
        vals = ['enable', 'disable', 'squash']
    sw_prefetch_policy = Param.PrefetchPolicy('enable',
        "software prefetch policy")
    thread_weights = VectorParam.Int([], "issue priority weights")
    use_hm_predictor = Param.Bool(False, "enable hit/miss predictor")
    use_lat_predictor = Param.Bool(False, "enable latency predictor")
    use_lr_predictor = Param.Bool(True, "enable left/right predictor")
    width = Param.Int(4, "default machine width")
''')

AddModule(['m5', 'objects'], 'FunctionalMemory', 'py', 'm5/python/m5/objects/FunctionalMemory.py', '''\
from m5 import *

class FunctionalMemory(SimObject):
    type = 'FunctionalMemory'
    abstract = True
''')

AddModule(['m5', 'objects'], 'HierParams', 'py', 'm5/python/m5/objects/HierParams.py', '''\
from m5 import *
class HierParams(SimObject):
    type = 'HierParams'
    do_data = Param.Bool("Store data in this hierarchy")
    do_events = Param.Bool("Simulate timing in this hierarchy")
''')

AddModule(['m5', 'objects'], 'Ide', 'py', 'm5/python/m5/objects/Ide.py', '''\
from m5 import *
from Pci import PciDevice

class IdeID(Enum): vals = ['master', 'slave']

class IdeDisk(SimObject):
    type = 'IdeDisk'
    delay = Param.Latency('1us', "Fixed disk delay in microseconds")
    driveID = Param.IdeID('master', "Drive ID")
    image = Param.DiskImage("Disk image")
    physmem = Param.PhysicalMemory(Parent.any, "Physical memory")

class IdeController(PciDevice):
    type = 'IdeController'
    disks = VectorParam.IdeDisk("IDE disks attached to this controller")
''')

AddModule(['m5', 'objects'], 'IntervalStats', 'py', 'm5/python/m5/objects/IntervalStats.py', '''\
from m5 import *
class IntervalStats(ParamContext):
    type = 'IntervalStats'
    file = Param.String("output file for interval statistics")
    exit_when_done = Param.Bool("exit after last cycle interval")
    enable_stat_triggers = Param.Bool("stats dump instruction triggers")
    cycle = Param.Counter("dump statistics every 'n' cycles")
    range = Param.Range(Range(0,0), "cycle range to dump")
    stats = VectorParam.String("statistics to dump")
    if False:
        inst = Param.Counter("dump statistics every 'n' instructions");
''')

AddModule(['m5', 'objects'], 'IntrControl', 'py', 'm5/python/m5/objects/IntrControl.py', '''\
from m5 import *
class IntrControl(SimObject):
    type = 'IntrControl'
    cpu = Param.BaseCPU(Parent.any, "the cpu")
''')

AddModule(['m5', 'objects'], 'LinuxSystem', 'py', 'm5/python/m5/objects/LinuxSystem.py', '''\
from m5 import *
from System import System

class LinuxSystem(System):
    type = 'LinuxSystem'
    system_type = 34
    system_rev = 1 << 10
''')

AddModule(['m5', 'objects'], 'MainMemory', 'py', 'm5/python/m5/objects/MainMemory.py', '''\
from m5 import *
from FunctionalMemory import FunctionalMemory

class MainMemory(FunctionalMemory):
    type = 'MainMemory'
    do_data = Param.Bool(False, "dummy param")
''')

AddModule(['m5', 'objects'], 'MemTest', 'py', 'm5/python/m5/objects/MemTest.py', '''\
from m5 import *
class MemTest(SimObject):
    type = 'MemTest'
    cache = Param.BaseCache("L1 cache")
    check_mem = Param.FunctionalMemory("check memory")
    main_mem = Param.FunctionalMemory("hierarchical memory")
    max_loads = Param.Counter("number of loads to execute")
    memory_size = Param.Int(65536, "memory size")
    percent_copies = Param.Percent(0, "target copy percentage")
    percent_dest_unaligned = Param.Percent(50,
        "percent of copy dest address that are unaligned")
    percent_reads = Param.Percent(65, "target read percentage")
    percent_source_unaligned = Param.Percent(50,
        "percent of copy source address that are unaligned")
    percent_uncacheable = Param.Percent(10,
        "target uncacheable percentage")
    progress_interval = Param.Counter(1000000,
        "progress report interval (in accesses)")
    trace_addr = Param.Addr(0, "address to trace")
''')

AddModule(['m5', 'objects'], 'MemoryController', 'py', 'm5/python/m5/objects/MemoryController.py', '''\
from m5 import *
from FunctionalMemory import FunctionalMemory

class MemoryController(FunctionalMemory):
    type = 'MemoryController'
    capacity = Param.Int(64, "Maximum Number of children")
''')

AddModule(['m5', 'objects'], 'MemoryTrace', 'py', 'm5/python/m5/objects/MemoryTrace.py', '''\
from m5 import *
class MemoryTrace(ParamContext):
    type = 'MemoryTrace'
    trace = Param.String("dump memory traffic <filename>")
    thread = Param.Int(0, "which thread to trace")
    spec = Param.Bool(False, "trace misspeculated execution")

class MemTraceReader(SimObject):
    type = 'MemTraceReader'
    abstract = True
    filename = Param.String("trace file")

class M5Reader(MemTraceReader):
    type = 'M5Reader'

class IBMReader(MemTraceReader):
    type = 'IBMReader'

class ITXReader(MemTraceReader):
    type = 'ITXReader'

class MemTraceWriter(SimObject):
    type = 'MemTraceReader'
    abstract = True
    filename = Param.String("trace file")

class M5Writer(MemTraceWriter):
    type = 'M5Writer'

class ITXWriter(MemTraceWriter):
    type = 'ITXWriter'

class TraceCPU(SimObject):
    type = 'TraceCPU'
    data_trace = Param.MemTraceReader(NULL, "data trace")
    dcache = Param.BaseMem(NULL, "data cache")
    icache = Param.BaseMem(NULL, "instruction cache")

class OptCPU(SimObject):
    type = 'OptCPU'
    data_trace = Param.MemTraceReader(NULL, "data trace")
    assoc = Param.Int("associativity")
    block_size = Param.Int("block size in bytes")
    size = Param.MemorySize("capacity in bytes")
''')

AddModule(['m5', 'objects'], 'Pci', 'py', 'm5/python/m5/objects/Pci.py', '''\
from m5 import *
from Device import FooPioDevice, DmaDevice

class PciConfigData(SimObject):
    type = 'PciConfigData'
    VendorID = Param.UInt16("Vendor ID")
    DeviceID = Param.UInt16("Device ID")
    Command = Param.UInt16(0, "Command")
    Status = Param.UInt16(0, "Status")
    Revision = Param.UInt8(0, "Device")
    ProgIF = Param.UInt8(0, "Programming Interface")
    SubClassCode = Param.UInt8(0, "Sub-Class Code")
    ClassCode = Param.UInt8(0, "Class Code")
    CacheLineSize = Param.UInt8(0, "System Cacheline Size")
    LatencyTimer = Param.UInt8(0, "PCI Latency Timer")
    HeaderType = Param.UInt8(0, "PCI Header Type")
    BIST = Param.UInt8(0, "Built In Self Test")

    BAR0 = Param.UInt32(0x00, "Base Address Register 0")
    BAR1 = Param.UInt32(0x00, "Base Address Register 1")
    BAR2 = Param.UInt32(0x00, "Base Address Register 2")
    BAR3 = Param.UInt32(0x00, "Base Address Register 3")
    BAR4 = Param.UInt32(0x00, "Base Address Register 4")
    BAR5 = Param.UInt32(0x00, "Base Address Register 5")
    BAR0Size = Param.UInt32(0, "Base Address Register 0 Size")
    BAR1Size = Param.UInt32(0, "Base Address Register 1 Size")
    BAR2Size = Param.UInt32(0, "Base Address Register 2 Size")
    BAR3Size = Param.UInt32(0, "Base Address Register 3 Size")
    BAR4Size = Param.UInt32(0, "Base Address Register 4 Size")
    BAR5Size = Param.UInt32(0, "Base Address Register 5 Size")

    CardbusCIS = Param.UInt32(0x00, "Cardbus Card Information Structure")
    SubsystemID = Param.UInt16(0x00, "Subsystem ID")
    SubsystemVendorID = Param.UInt16(0x00, "Subsystem Vendor ID")
    ExpansionROM = Param.UInt32(0x00, "Expansion ROM Base Address")
    InterruptLine = Param.UInt8(0x00, "Interrupt Line")
    InterruptPin = Param.UInt8(0x00, "Interrupt Pin")
    MaximumLatency = Param.UInt8(0x00, "Maximum Latency")
    MinimumGrant = Param.UInt8(0x00, "Minimum Grant")

class PciConfigAll(FooPioDevice):
    type = 'PciConfigAll'

class PciDevice(DmaDevice):
    type = 'PciDevice'
    abstract = True
    addr = 0xffffffffL
    pci_bus = Param.Int("PCI bus")
    pci_dev = Param.Int("PCI device number")
    pci_func = Param.Int("PCI function code")
    configdata = Param.PciConfigData(Parent.any, "PCI Config data")
    configspace = Param.PciConfigAll(Parent.any, "PCI Configspace")

class PciFake(PciDevice):
    type = 'PciFake'
''')

AddModule(['m5', 'objects'], 'PhysicalMemory', 'py', 'm5/python/m5/objects/PhysicalMemory.py', '''\
from m5 import *
from FunctionalMemory import FunctionalMemory

class PhysicalMemory(FunctionalMemory):
    type = 'PhysicalMemory'
    range = Param.AddrRange("Device Address")
    file = Param.String('', "memory mapped file")
    mmu = Param.MemoryController(Parent.any, "Memory Controller")
''')

AddModule(['m5', 'objects'], 'PipeTrace', 'py', 'm5/python/m5/objects/PipeTrace.py', '''\
from m5 import *
class PipeTrace(SimObject):
    type = 'PipeTrace'
    exit_when_done = Param.Bool(False,
        "terminate simulation when done collecting ptrace")
    file = Param.String('', "output file name")
    range = Param.String('', "range of cycles to trace")
    statistics = VectorParam.String("stats to include in pipe-trace")
''')

AddModule(['m5', 'objects'], 'Platform', 'py', 'm5/python/m5/objects/Platform.py', '''\
from m5 import *
class Platform(SimObject):
    type = 'Platform'
    abstract = True
    intrctrl = Param.IntrControl(Parent.any, "interrupt controller")
''')

AddModule(['m5', 'objects'], 'Process', 'py', 'm5/python/m5/objects/Process.py', '''\
from m5 import *
class Process(SimObject):
    type = 'Process'
    abstract = True
    output = Param.String('cout', 'filename for stdout/stderr')

class LiveProcess(Process):
    type = 'LiveProcess'
    executable = Param.String('', "executable (overrides cmd[0] if set)")
    cmd = VectorParam.String("command line (executable plus arguments)")
    env = VectorParam.String('', "environment settings")
    input = Param.String('cin', "filename for stdin")

class EioProcess(Process):
    type = 'EioProcess'
    chkpt = Param.String('', "EIO checkpoint file name (optional)")
    file = Param.String("EIO trace file name")
''')

AddModule(['m5', 'objects'], 'PseudoInst', 'py', 'm5/python/m5/objects/PseudoInst.py', '''\
from m5 import *
class PseudoInst(ParamContext):
    type = 'PseudoInst'
    quiesce = Param.Bool(True, "enable quiesce instructions")
    statistics = Param.Bool(True, "enable statistics pseudo instructions")
    checkpoint = Param.Bool(True, "enable checkpoint pseudo instructions")
''')

AddModule(['m5', 'objects'], 'Random', 'py', 'm5/python/m5/objects/Random.py', '''\
from m5 import *
class Random(ParamContext):
    type = 'Random'
    seed = Param.UInt32(1, "seed to random number generator");

''')

AddModule(['m5', 'objects'], 'Repl', 'py', 'm5/python/m5/objects/Repl.py', '''\
from m5 import *
class Repl(SimObject):
    type = 'Repl'
    abstract = True

class GenRepl(Repl):
    type = 'GenRepl'
    fresh_res = Param.Int("associativity")
    num_pools = Param.Int("capacity in bytes")
    pool_res = Param.Int("block size in bytes")
''')

AddModule(['m5', 'objects'], 'Root', 'py', 'm5/python/m5/objects/Root.py', '''\
from m5 import *
from HierParams import HierParams
from Serialize import Serialize
from Statistics import Statistics
from Trace import Trace

class Root(SimObject):
    type = 'Root'
    clock = Param.RootClock('200MHz', "tick frequency")
    max_tick = Param.Tick('0', "maximum simulation ticks (0 = infinite)")
    progress_interval = Param.Tick('0',
        "print a progress message every n ticks (0 = never)")
    output_file = Param.String('cout', "file to dump simulator output to")
    checkpoint = Param.String('', "checkpoint file to load")
#    hier = Param.HierParams(HierParams(do_data = False, do_events = True),
#                            "shared memory hierarchy parameters")
#    stats = Param.Statistics(Statistics(), "statistics object")
#    trace = Param.Trace(Trace(), "trace object")
#    serialize = Param.Serialize(Serialize(), "checkpoint generation options")
    hier = HierParams(do_data = False, do_events = True)
    stats = Statistics()
    trace = Trace()
    serialize = Serialize()
''')

AddModule(['m5', 'objects'], 'Sampler', 'py', 'm5/python/m5/objects/Sampler.py', '''\
from m5 import *
class Sampler(SimObject):
    type = 'Sampler'
    phase0_cpus = VectorParam.BaseCPU("vector of actual CPUs to run in phase 0")
    phase1_cpus = VectorParam.BaseCPU("vector of actual CPUs to run in phase 1")
    periods = VectorParam.Tick("vector of per-cpu sample periods")
''')

AddModule(['m5', 'objects'], 'Scsi', 'py', 'm5/python/m5/objects/Scsi.py', '''\
from m5 import *
from Device import DmaDevice

class ScsiController(DmaDevice):
    type = 'ScsiController'
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")
    disks = VectorParam.ScsiDisk("Disks attached to this controller")
    engine = Param.DmaEngine(Parent.any, "DMA Engine")

class ScsiDevice(SimObject):
    type = 'ScsiDevice'
    abstract = True
    target = Param.Int("target scsi ID")

class ScsiNone(ScsiDevice):
    type = 'ScsiNone'

class ScsiDisk(ScsiDevice):
    type = 'ScsiDisk'
    delay = Param.Latency('0us', "fixed disk delay")
    image = Param.DiskImage("disk image")
    sector_size = Param.Int(512, "disk sector size")
    serial = Param.String("disk serial number")
    use_interface = Param.Bool(True, "use dma interface")
''')

AddModule(['m5', 'objects'], 'Serialize', 'py', 'm5/python/m5/objects/Serialize.py', '''\
from m5 import *
class Serialize(ParamContext):
    type = 'Serialize'
    dir = Param.String('cpt.%012d', "dir to stick checkpoint in")
    cycle = Param.Tick(0, "cycle to serialize")
    period = Param.Tick(0, "period to repeat serializations")
    count = Param.Int(10, "maximum number of checkpoints to drop")
''')

AddModule(['m5', 'objects'], 'SimConsole', 'py', 'm5/python/m5/objects/SimConsole.py', '''\
from m5 import *
class ConsoleListener(SimObject):
    type = 'ConsoleListener'
    port = Param.TcpPort(3456, "listen port")

class SimConsole(SimObject):
    type = 'SimConsole'
    append_name = Param.Bool(True, "append name() to filename")
    intr_control = Param.IntrControl(Parent.any, "interrupt controller")
    listener = Param.ConsoleListener("console listener")
    number = Param.Int(0, "console number")
    output = Param.String('console', "file to dump output to")
''')

AddModule(['m5', 'objects'], 'SimpleCPU', 'py', 'm5/python/m5/objects/SimpleCPU.py', '''\
from m5 import *
from BaseCPU import BaseCPU

class SimpleCPU(BaseCPU):
    type = 'SimpleCPU'
    width = Param.Int(1, "CPU width")
    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")
''')

AddModule(['m5', 'objects'], 'SimpleDisk', 'py', 'm5/python/m5/objects/SimpleDisk.py', '''\
from m5 import *
class SimpleDisk(SimObject):
    type = 'SimpleDisk'
    disk = Param.DiskImage("Disk Image")
    physmem = Param.PhysicalMemory(Parent.any, "Physical Memory")
''')

AddModule(['m5', 'objects'], 'Statistics', 'py', 'm5/python/m5/objects/Statistics.py', '''\
from m5 import *
class Statistics(ParamContext):
    type = 'Statistics'
    descriptions = Param.Bool(True, "display statistics descriptions")
    project_name = Param.String('test',
        "project name for statistics comparison")
    simulation_name = Param.String('test',
        "simulation name for statistics comparison")
    simulation_sample = Param.String('0', "sample for stats aggregation")
    text_file = Param.String('m5stats.txt', "file to dump stats to")
    text_compat = Param.Bool(True, "simplescalar stats compatibility")
    mysql_db = Param.String('', "mysql database to put data into")
    mysql_user = Param.String('', "username for mysql")
    mysql_password = Param.String('', "password for mysql user")
    mysql_host = Param.String('', "host for mysql")
    #events_start = Param.Tick(MaxTick, "cycle to start tracking events")
    dump_reset = Param.Bool(False, "when dumping stats, reset afterwards")
    dump_cycle = Param.Tick(0, "cycle on which to dump stats")
    dump_period = Param.Tick(0, "period with which to dump stats")
    ignore_events = VectorParam.String([], "name strings to ignore")

''')

AddModule(['m5', 'objects'], 'System', 'py', 'm5/python/m5/objects/System.py', '''\
from m5 import *
class System(SimObject):
    type = 'System'
    boot_cpu_frequency = Param.Frequency(Self.cpu[0].clock.frequency, 
                                         "boot processor frequency")
    memctrl = Param.MemoryController(Parent.any, "memory controller")
    physmem = Param.PhysicalMemory(Parent.any, "phsyical memory")
    kernel = Param.String("file that contains the kernel code")
    console = Param.String("file that contains the console code")
    pal = Param.String("file that contains palcode")
    readfile = Param.String("", "file to read startup script from")
    init_param = Param.UInt64(0, "numerical value to pass into simulator")
    boot_osflags = Param.String("a", "boot flags to pass to the kernel")
    system_type = Param.UInt64("Type of system we are emulating")
    system_rev = Param.UInt64("Revision of system we are emulating")
    bin = Param.Bool(False, "is this system binned")
    binned_fns = VectorParam.String([], "functions broken down and binned")
''')

AddModule(['m5', 'objects'], 'Trace', 'py', 'm5/python/m5/objects/Trace.py', '''\
from m5 import *
class Trace(ParamContext):
    type = 'Trace'
    flags = VectorParam.String([], "categories to be traced")
    start = Param.Tick(0, "cycle to start tracing")
    bufsize = Param.Int(0, "circular buffer size (0 = send to file)")
    file = Param.String('cout', "trace output file")
    dump_on_exit = Param.Bool(False, "dump trace buffer on exit")
    ignore = VectorParam.String([], "name strings to ignore")

''')

AddModule(['m5', 'objects'], 'Tru64System', 'py', 'm5/python/m5/objects/Tru64System.py', '''\
from m5 import *
from System import System

class Tru64System(System):
    type = 'Tru64System'
    system_type = 12
    system_rev = 2<<1
''')

AddModule(['m5', 'objects'], 'Tsunami', 'py', 'm5/python/m5/objects/Tsunami.py', '''\
from m5 import *
from Device import FooPioDevice
from Platform import Platform

class Tsunami(Platform):
    type = 'Tsunami'
    pciconfig = Param.PciConfigAll("PCI configuration")
    system = Param.System(Parent.any, "system")

class TsunamiCChip(FooPioDevice):
    type = 'TsunamiCChip'
    tsunami = Param.Tsunami(Parent.any, "Tsunami")

class IsaFake(FooPioDevice):
    type = 'IsaFake'
    size = Param.Addr("Size of address range")

class TsunamiIO(FooPioDevice):
    type = 'TsunamiIO'
    time = Param.UInt64(1136073600,
        "System time to use (0 for actual time, default is 1/1/06)")
    tsunami = Param.Tsunami(Parent.any, "Tsunami")
    frequency = Param.Frequency('1024Hz', "frequency of interrupts")

class TsunamiPChip(FooPioDevice):
    type = 'TsunamiPChip'
    tsunami = Param.Tsunami(Parent.any, "Tsunami")
''')

AddModule(['m5', 'objects'], 'Turbolaser', 'py', 'm5/python/m5/objects/Turbolaser.py', '''\
from m5 import *
from Device import PioDevice
from Device import FooPioDevice
from Platform import Platform

class TlaserClock(SimObject):
    type = 'TlaserClock'
    delay = Param.Latency('1ms', "number of cycles to delay clock start")
    frequency = Param.Frequency('1200Hz', "clock interrupt frequency")
    intr_control = Param.IntrControl(Parent.any, "interrupt controller")

class TlaserIpi(PioDevice):
    type = 'TlaserIpi'
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")
    addr = 0xff8e000000

class TlaserMBox(SimObject):
    type = 'TlaserMBox'
    physmem = Param.PhysicalMemory(Parent.any, "physical memory")

class TlaserMC146818(PioDevice):
    type = 'TlaserMC146818'
    clock = Param.TlaserClock(Parent.any, "turbolaser clock")
    time = Param.Int(1136073600,
        "System time to use (0 for actual time, default is 1/1/06")
    addr = 0xffb0000000

class TlaserNodeType(Enum):
    vals = ["KFTHA", "KFTIA", "MS7CC", "SingleProc4M", "SingleProc16M",
            "DualProc4M", "DualProc16M"]

class TlaserNode(PioDevice):
    type = 'TlaserNode'
    node_type = Param.TlaserNodeType("Node Type")
    number = Param.Int("Node Number")
    tlaser = Param.Turbolaser(Parent.any, "Turbolaser")

class TlaserPciDev(FooPioDevice):
    type = 'TlaserPciDev'
    VendorID = Param.UInt16("Vendor ID")
    DeviceID = Param.UInt16("Device ID")
    Command = Param.UInt16(0, "Command")
    Status = Param.UInt16(0, "Status")
    Revision = Param.UInt8(0, "Device")
    ProgIF = Param.UInt8(0, "Programming Interface")
    SubClassCode = Param.UInt8(0, "Sub-Class Code")
    ClassCode = Param.UInt8(0, "Class Code")
    CacheLineSize = Param.UInt8(0, "System Cacheline Size")
    LatencyTimer = Param.UInt8(0, "PCI Latency Timer")
    HeaderType = Param.UInt8(0, "PCI Header Type")
    BIST = Param.UInt8(0, "Built In Self Test")

    BAR0 = Param.UInt32(0x00, "Base Address Register 0")
    BAR1 = Param.UInt32(0x00, "Base Address Register 1")
    BAR2 = Param.UInt32(0x00, "Base Address Register 2")
    BAR3 = Param.UInt32(0x00, "Base Address Register 3")
    BAR4 = Param.UInt32(0x00, "Base Address Register 4")
    BAR5 = Param.UInt32(0x00, "Base Address Register 5")
    BAR0Size = Param.UInt32(0, "Base Address Register 0 Size")
    BAR1Size = Param.UInt32(0, "Base Address Register 1 Size")
    BAR2Size = Param.UInt32(0, "Base Address Register 2 Size")
    BAR3Size = Param.UInt32(0, "Base Address Register 3 Size")
    BAR4Size = Param.UInt32(0, "Base Address Register 4 Size")
    BAR5Size = Param.UInt32(0, "Base Address Register 5 Size")

    CardbusCIS = Param.UInt32(0x00, "Cardbus Card Information Structure")
    SubsystemID = Param.UInt16(0x00, "Subsystem ID")
    SubsystemVendorID = Param.UInt16(0x00, "Subsystem Vendor ID")
    ExpansionROM = Param.UInt32(0x00, "Expansion ROM Base Address")
    InterruptLine = Param.UInt8(0x00, "Interrupt Line")
    InterruptPin = Param.UInt8(0x00, "Interrupt Pin")
    MaximumLatency = Param.UInt8(0x00, "Maximum Latency")
    MinimumGrant = Param.UInt8(0x00, "Minimum Grant")

class TlaserPcia(PioDevice):
    type = 'TlaserPcia'
    addr = 0xc780000000

class TlaserSerial(PioDevice):
    type = 'TlaserSerial'
    sernum = Param.UInt32(0xFAFAFAFAL, "Serial Number")
    addr = 0xffc7000000

class Turbolaser(Platform):
    type = 'Turbolaser'
    clock = Param.TlaserClock(Parent.any, "turbolaser clock")
    mbox = Param.TlaserMBox(Parent.any, "message box")
''')

AddModule(['m5', 'objects'], 'Uart', 'py', 'm5/python/m5/objects/Uart.py', '''\
from m5 import *
from Device import PioDevice

class Uart(PioDevice):
    type = 'Uart'
    abstract = True
    console = Param.SimConsole(Parent.any, "The console")
    size = Param.Addr(0x8, "Device size")

class Uart8250(Uart):
    type = 'Uart8250'

if build_env['ALPHA_TLASER']:
    class Uart8530(Uart):
        type = 'Uart8530'

''')

AddModule(['m5', 'objects'], '__init__', 'py', 'm5/python/m5/objects/__init__.py', '''\
import m5

# specify base part of all object file names
file_bases = ['AlphaConsole',
              'AlphaFullCPU',
              'AlphaTLB',
              'BadDevice',
              'BaseCPU',
              'BaseCache',
              'BaseHier',
              'BaseMem',
              'BaseMemory',
              'BranchPred',
              'Bus',
              'BusBridge',
              'CoherenceProtocol',
              'Debug',
              'Device',
              'DiskImage',
              'Ethernet',
              'ExeTrace',
              'FastCPU',
              'FetchTrace',
              'FreebsdSystem',
              'FullCPU',
              'FunctionalMemory',
              'HierParams',
              'Ide',
              'IntervalStats',
              'IntrControl',
              'LinuxSystem',
              'MainMemory',
              'MemTest',
              'MemoryController',
              'MemoryTrace',
              'Pci',
              'PhysicalMemory',
              'PipeTrace',
              'Platform',
              'Process',
              'PseudoInst',
              'Random',
              'Repl',
              'Root',
              'Sampler',
              'Scsi',
              'Serialize',
              'SimConsole',
              'SimpleCPU',
              'SimpleDisk',
              'Statistics',
              'System',
              'Trace',
              'Tru64System',
              'Tsunami',
              'Uart']

if m5.build_env['ALPHA_TLASER']:
    file_bases += [ 'DmaEngine' ]
    file_bases += [ 'Turbolaser' ]


# actual file names end in ".py"
file_names = [f + ".py" for f in file_bases]

# import all specified files
for f in file_bases:
    exec "from %s import *" % f

# only export actual SimObject classes when "from objects import *" is used.
g = globals()
__all__ = []
for sym,val in g.items():
    if isinstance(val, type) and issubclass(val, (SimObject, ParamContext)):
        __all__.append(sym)
''')

AddModule(['m5'], 'smartdict', 'py', 'm5/python/m5/smartdict.py', '''\
# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

# The SmartDict class fixes a couple of issues with using the content
# of os.environ or similar dicts of strings as Python variables:
#
# 1) Undefined variables should return False rather than raising KeyError.
#
# 2) String values of 'False', '0', etc., should evaluate to False
#    (not just the empty string).
#
# #1 is solved by overriding __getitem__, and #2 is solved by using a
# proxy class for values and overriding __nonzero__ on the proxy.
# Everything else is just to (a) make proxies behave like normal
# values otherwise, (b) make sure any dict operation returns a proxy
# rather than a normal value, and (c) coerce values written to the
# dict to be strings.


from convert import *

class Variable(str):
    """Intelligent proxy class for SmartDict.  Variable will use the
    various convert functions to attempt to convert values to useable
    types"""
    def __int__(self):
        return toInteger(str(self))
    def __long__(self):
        return toLong(str(self))
    def __float__(self):
        return toFloat(str(self))
    def __nonzero__(self):
        return toBool(str(self))
    def convert(self, other):
        t = type(other)
        if t == bool:
            return bool(self)
        if t == int:
            return int(self)
        if t == long:
            return long(self)
        if t == float:
            return float(self)
        return str(self)
    def __lt__(self, other):
        return self.convert(other) < other
    def __le__(self, other):
        return self.convert(other) <= other
    def __eq__(self, other):
        return self.convert(other) == other
    def __ne__(self, other):
        return self.convert(other) != other
    def __gt__(self, other):
        return self.convert(other) > other
    def __ge__(self, other):
        return self.convert(other) >= other

    def __add__(self, other):
        return self.convert(other) + other
    def __sub__(self, other):
        return self.convert(other) - other
    def __mul__(self, other):
        return self.convert(other) * other
    def __div__(self, other):
        return self.convert(other) / other
    def __truediv__(self, other):
        return self.convert(other) / other

    def __radd__(self, other):
        return other + self.convert(other)
    def __rsub__(self, other):
        return other - self.convert(other)
    def __rmul__(self, other):
        return other * self.convert(other)
    def __rdiv__(self, other):
        return other / self.convert(other)
    def __rtruediv__(self, other):
        return other / self.convert(other)

class UndefinedVariable(object):
    """Placeholder class to represent undefined variables.  Will
    generally cause an exception whenever it is used, but evaluates to
    zero for boolean truth testing such as in an if statement"""
    def __nonzero__(self):
        return False
        
class SmartDict(dict):
    """Dictionary class that holds strings, but intelligently converts
    those strings to other types depending on their usage"""

    def __getitem__(self, key):
        """returns a Variable proxy if the values exists in the database and
        returns an UndefinedVariable otherwise"""

        if key in self:
            return Variable(dict.get(self, key))
        else:
            # Note that this does *not* change the contents of the dict,
            # so that even after we call env['foo'] we still get a
            # meaningful answer from "'foo' in env" (which
            # calls dict.__contains__, which we do not override).
            return UndefinedVariable()

    def __setitem__(self, key, item):
        """intercept the setting of any variable so that we always
        store strings in the dict"""
        dict.__setitem__(self, key, str(item))
    
    def values(self):
        return [ Variable(v) for v in dict.values(self) ]

    def itervalues(self):
        for value in dict.itervalues(self):
            yield Variable(value)

    def items(self):
        return [ (k, Variable(v)) for k,v in dict.items(self) ]

    def iteritems(self):
        for key,value in dict.iteritems(self):
            yield key, Variable(value)
    
    def get(self, key, default='False'):
        return Variable(dict.get(self, key, str(default)))

    def setdefault(self, key, default='False'):
        return Variable(dict.setdefault(self, key, str(default)))

__all__ = [ 'SmartDict' ]
''')

import __main__
__main__.m5_build_env =  {'STATS_BINNING': 0, 'ALPHA_TLASER': 0, 'SS_COMPATIBLE_FP': 1, 'NO_FAST_ALLOC': 0, 'USE_FENV': 0, 'FULL_SYSTEM': 0, 'USE_MYSQL': 0}
from m5 import *
AddToPath('/home/stever/bk/m5/build')
m5execfile('m5-test/memtest/run.py', globals())
if globals().has_key('root') and isinstance(root, Root):
    instantiate(root)
else:
    print 'Instantiation skipped: no root object found.'
