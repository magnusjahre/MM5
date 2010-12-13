
class Workload:
    
    def __init__(self):
        self.benchmarks = []
    
    def addBenchmark(self, bm):
        self.benchmarks.append(bm)
        
    def containsBenchmark(self, bm):
        if bm in self.benchmarks:
            return True
        return False
    
    def getNumBms(self):
        return len(self.benchmarks)

    def __str__(self):
        out = ""
        for b in self.benchmarks:
            out += " "+b
        return out