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
