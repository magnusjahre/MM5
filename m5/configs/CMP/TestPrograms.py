
from m5 import *

rootdir = os.getenv("BMROOT")
if rootdir == None:
  print "Envirionment variable BMROOT not set. Quitting..."
  sys.exit(-1)

rootpath = rootdir+"/"

class HelloWorld(LiveProcess):
    cmd = rootpath +"hello/hello"
    
class ThrashCache(LiveProcess):
    cmd = rootpath +"thrash_cache/thrash_cache_alpha"
