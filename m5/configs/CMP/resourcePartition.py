
class ResourcePartition:
    
    def __init__(self, np):
        self.np = np
        self.ways = []
        self.utils = []
        self.metricValue = 0
        self.predictedIPCs = {}
        
        self.initialized = False
        
    def setPartition(self, ways, utils, metricValue, predictedIPCs):
        self.ways = ways
        self.utils = utils
        self.metricValue = metricValue
        self.predictedIPCs = predictedIPCs
        
        self.initialized = True
        
    def isInitialized(self):
        return self.initialized
    
    def __str__(self):
        return str(self.ways)+", "+str(self.utils)+", "+str(self.metricValue)