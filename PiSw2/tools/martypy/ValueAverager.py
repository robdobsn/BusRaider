class ValueAverager:
    
    def __init__(self):
        self.valList = []
        self.windowSize = 10

    def add(self, newVal):
        self.valList = self.valList[-self.windowSize:]
        self.valList.append(newVal)

    def getAvg(self):
        if len(self.valList) > 0:
            return sum(self.valList)/len(self.valList)
        return 0

