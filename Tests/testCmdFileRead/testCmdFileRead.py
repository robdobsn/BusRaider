

class Z80CMDFileHandler:

    def __init__(self):
        self.execAddr = 0
        self.segments = []
        self.isLoaded = False
        self.isValid = False
        
    def load(self, fileName):

        self.isValid = False
        self.isLoaded = False
        with open(fileName, "rb") as cmdFile:
            b = cmdFile.read()
            i = 0
            while i < len(b) - 2:

                # Handle record start info
                code = b[i]
                length = b[i+1]
                i += 2
                length = min(length, len(b) - i)
                if length == 0:
                    length = 0x100

                # Code
                if code == 0:
                    print("Unused code", code)
                elif code == 1:
                    # Data segment
                    addr = b[i] + (b[i+1] << 8)
                    i += 2
                    if length < 3:
                        length += 0xfe
                    else:
                        length -= 2
                    self.segments.append({ "addr": addr, "data": b[i:i+length]})
                    print("Code segment", hex(addr), hex(length))
                elif code == 2:
                    # Exec addr
                    if length == 1:
                        self.execAddr = b[i]
                        i += 1
                        print("ExecAddr8", hex(self.execAddr))
                    elif length == 2:
                        self.execAddr = b[i] + (b[i+1] << 8)
                        i += 2
                        print("ExecAddr16", hex(self.execAddr))
                    else:
                        print("Error in exec record")
                    self.isLoaded = True
                    self.isValid = True
                elif code == 3:
                    # Non exec marker
                    processSegments()
                    return
                elif code == 4:
                    # End of partitioned data
                    print("End of partitioned data")
                elif code == 5:
                    # Title
                    print("Title", b[i:i+length])
                else:
                    print("Undecoded block", code)

                i += length

    def processSegments(self):
        print("Segs", len(self.segments))

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

    def createSRECFile(self, startCh, outFileName):
        if not self.isLoaded or not self.isValid:
            return
        bytesPerLine = 16
        with open(outFileName, "w+") as outFile:
            # Iterate segments
            for seg in self.segments:
                lenSeg = len(seg["data"])
                addrLine = seg["addr"]
                segDataIdx = 0
                for i in range(lenSeg//bytesPerLine):
                    lenRec = 16 if lenSeg >= 16 else lenSeg
                    s3Rec = self.genS3Rec(startCh, addrLine, seg["data"], lenRec, segDataIdx)
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
            
z80CmdHandler = Z80CMDFileHandler()

z80CmdHandler.load(r"C:\Users\rob\Downloads\Galaxy Invasion (1980)(Big Five Software)[CMD]\galinv1a.cmd")
z80CmdHandler.createSRECFile("T", r"C:\Users\rob\Downloads\Galaxy Invasion (1980)(Big Five Software)[CMD]\galinv1a.srec")

