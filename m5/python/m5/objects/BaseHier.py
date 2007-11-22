from m5 import *
class BaseHier(SimObject):
    type = 'BaseHier'
    abstract = True
    hier = Param.HierParams(Parent.any, "Hierarchy global variables")
