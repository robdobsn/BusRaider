

class Z80BINFileHandler:

    def __init__(self):
        self.execAddr = 0x8000
        self.baseAddr = 0x8000
        
    def genS3Rec(self, startCh, addr, data, leng, offset):
        s3Rec = startCh + "3" + "{:0>2x}".format(leng + 5)
        csum = leng + 5
        s3Rec += "{:0>8x}".format(addr)
        csum += (addr & 0xff) + ((addr >> 8) & 0xff)
        for byteIdx in range(leng):
            byteVal = data[byteIdx+offset]
            s3Rec += "{:0>2x}".format(byteVal)
            csum += byteVal
        s3Rec += "{:0>2x}".format((csum & 0xff) ^ 0xff)
        return s3Rec

    def loadAndStore(self, startCh, inFileName, outFileName):

        with open(inFileName, "rb") as binFile:
            b = bytearray(binFile.read())
            bytesPerLine = 16
            lenSeg = len(b)
            addrLine = self.baseAddr
            segDataIdx = 0
            with open(outFileName, "w+") as outFile:
                for i in range((len(b)/bytesPerLine)+1):
                    lenRec = 16 if lenSeg >= 16 else lenSeg
                    if lenRec == 0:
                        break
                    s3Rec = self.genS3Rec(startCh, addrLine, b, lenRec, segDataIdx)
                    outFile.write(s3Rec + "\n")
                    lenSeg -= bytesPerLine
                    addrLine += bytesPerLine
                    segDataIdx += bytesPerLine
                # Write exec address
                s7Rec = startCh + "705" + "{:0>8x}".format(self.execAddr)
                csum = 5 + (self.execAddr & 0xff) + ((self.execAddr >> 8) & 0xff)
                s7Rec += "{:0>2x}".format((csum & 0xff) ^ 0xff)
                outFile.write(s7Rec + "\n")
                # Write exec vector
                s3Rec = self.genS3Rec(startCh, 0, [0xc3, self.execAddr & 0xff, (self.execAddr >> 8) & 0xff],3, 0)
                outFile.write(s3Rec + "\n")
            
z80BinHandler = Z80BINFileHandler()

z80BinHandler.loadAndStore("T", r"hello.bin", r"hello.srec")

