class Z80Registers:
    def __init__(self):
        self.clear()

    def clear(self):
        self.regVals = {
                "PC":"0x0000",
                "SP":"0x0000",
                "BC":"0x0000",
                "AF":"0x0000",
                "HL":"0x0000",
                "DE":"0x0000",
                "IX":"0x0000",
                "IY":"0x0000",
                "AFDASH":"0x0000",
                "BCDASH":"0x0000",
                "HLDASH":"0x0000",
                "DEDASH":"0x0000",
                "I":"0x00",
                "R":"0x00",
                "IM":0,
                "IFF":0,
                "VPS":0
        }

class MockImager:

    def __init__(self):
        self.targetMemSize = 65536
        self.memImage = bytearray(self.targetMemSize)
        self.memBlocks = []
        self.targetRegs = Z80Registers
        self.setRegsCodeAddr = 0

    def clear(self):
        self.memImage = bytearray(self.targetMemSize)
        self.memBlocks = []
        self.targetRegs.clear()
        self.setRegsCodeAddr = 0

    def addMemBlock(self, addr, data):
        print(addr)
        print(len(data))
        