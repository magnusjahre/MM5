import operator, types

def unproxy(proxy):
    if isinstance(proxy, ProxyBase):
        return proxy.unproxy()

    return proxy

class ProxyBase(object):
    def __add__(self, other):
        return MathProxy(operator.__add__, self, other)
    def __sub__(self, other):
        return MathProxy(operator.__sub__, self, other)
    def __mul__(self, other):
        return MathProxy(operator.__mul__, self, other)
    def __div__(self, other):
        return MathProxy(operator.__div__, self, other)
    def __truediv__(self, other):
        return MathProxy(operator.__truediv__, self, other)
    def __floordiv__(self, other):
        return MathProxy(operator.__floordiv__, self, other)
    def __mod__(self, other):
        return MathProxy(operator.__mod__, self, other)
    def __divmod__(self, other):
        return MathProxy(operator.__divmod__, self, other)
    def __pow__(self, *args):
        return MathProxy(operator.__pow__, self, *args)
    def __lshift__(self, other):
        return MathProxy(operator.__lshift__, self, other)
    def __rshift__(self, other):
        return MathProxy(operator.__rshift__, self, other)
    def __and__(self, other):
        return MathProxy(operator.__and__, self, other)
    def __xor__(self, other):
        return MathProxy(operator.__xor__, self, other)
    def __or__(self, other):
        return MathProxy(operator.__or__, self, other)

    def __radd__(self, other):
        return MathProxy(operator.__add__, other, self)
    def __rsub__(self, other):
        return MathProxy(operator.__sub__, other, self)
    def __rmul__(self, other):
        return MathProxy(operator.__mul__, other, self)
    def __rdiv__(self, other):
        return MathProxy(operator.__div__, other, self)
    def __rtruediv__(self, other):
        return MathProxy(operator.__truediv__, other, self)
    def __rfloordiv__(self, other):
        return MathProxy(operator.__floordiv__, other, self)
    def __rmod__(self, other):
        return MathProxy(operator.__mod__, other, self)
    def __rdivmod__(self, other):
        return MathProxy(operator.__divmod__, other, self)
    def __rpow__(self, other):
        return MathProxy(operator.__pow__, other, self)
    def __rlshift__(self, other):
        return MathProxy(operator.__lshift__, other, self)
    def __rrshift__(self, other):
        return MathProxy(operator.__rshift__, other, self)
    def __rand__(self, other):
        return MathProxy(operator.__and__, other, self)
    def __rxor__(self, other):
        return MathProxy(operator.__xor__, other, self)
    def __ror__(self, other):
        return MathProxy(operator.__or__, other, self)

    def __iadd__(self, other):
        self.__init__(operator.__add__, MathProxy(self.op, self.args), other)
    def __isub__(self, other):
        self.__init__(operator.__sub__, MathProxy(self.op, self.args), other)
    def __imul__(self, other):
        self.__init__(operator.__mul__, MathProxy(self.op, self.args), other)
    def __idiv__(self, other):
        self.__init__(operator.__div__, MathProxy(self.op, self.args), other)
    def __itruediv__(self, other):
        self.__init__(operator.__truediv__,
                      MathProxy(self.op, self.args), other)
    def __ifloordiv__(self, other):
        self.__init__(operator.__floordiv__,
                      MathProxy(self.op, self.args), other)
    def __imod__(self, other):
        self.__init__(operator.__mod__, MathProxy(self.op, self.args), other)
    def __ipow__(self, *args):
        self.__init__(operator.__pow__, MathProxy(self.op, self.args), other)
    def __ilshift__(self, other):
        self.__init__(operator.__lshift__,
                      MathProxy(self.op, self.args), other)
    def __irshift__(self, other):
        self.__init__(operator.__rshift__,
                      MathProxy(self.op, self.args), other)
    def __iand__(self, other):
        self.__init__(operator.__and__, MathProxy(self.op, self.args), other)
    def __ixor__(self, other):
        self.__init__(operator.__xor__, MathProxy(self.op, self.args), other)
    def __ior__(self, other):
        self.__init__(operator.__or__, MathProxy(self.op, self.args), other)

    def __neg__(self):
        return MathProxy(operator.__neg__, self)
    def __pos__(self):
        return MathProxy(operator.__pos__, self)
    def __abs__(self):
        return MathProxy(operator.__abs__, self)
    def __invert__(self):
        return MathProxy(operator.__invert__, self)

    def __int__(self):
        return int(unproxy(self))
    def __long__(self):
        return long(unproxy(self))
    def __float__(self):
        return float(unproxy(self))
    def __complex__(self):
        return complex(unproxy(self))
    def __str__(self):
        return str(unproxy(self))

class Proxy(ProxyBase):
    def __init__(self, name, dict):
        self.name = name
        self.dict = dict

    def unproxy(self):
        return self.dict[self.name]

    def __getitem__(self, index):
        return ItemProxy(self, index)

    def __getattr__(self, attr):
        return AttrProxy(self, attr)

class ProxyGroup(object):
    def __init__(self, dict=None, **kwargs):
        self.__dict__['dict'] = {}

        if dict is not None:
            self.dict.update(dict)

        if kwargs:
            self.dict.update(kwargs)

    def __getattr__(self, name):
        return Proxy(name, self.dict)

    def __setattr__(self, attr, value):
        self.dict[attr] = value

class ItemProxy(Proxy):
    def __init__(self, proxy, index):
        self.proxy = proxy
        self.index = index

    def unproxy(self):
        return unproxy(self.proxy)[self.index]

class AttrProxy(Proxy):
    def __init__(self, proxy, attr):
        self.proxy = proxy
        self.attr = attr

    def unproxy(self):
        return object.__getattribute__(unproxy(self.proxy), self.attr)

class MathProxy(ProxyBase):
    def __init__(self, op, *args):
        self.op = op
        self.args = args

    def unproxy(self):
        args = [ unproxy(arg) for arg in self.args ]
        return self.op(*args)

__all__ = [ 'unproxy', 'ProxyGroup' ]

if __name__ == "__main__":
    class test(object):
        x = [ 9, 8, 7, 6, 5 ]

    proxy = ProxyGroup()
    x = proxy.x
    y = (x.x[4] + 5) * 11

    proxy.x = test
    print y
