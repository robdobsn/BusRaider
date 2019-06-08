from PyCRC.CRCCCITT import CRCCCITT
import struct

dumpFileName = "./test/logs/Test.bin"

framePos = 0
rxCRC = 0
frameData = bytearray()
escVal = False
okFrameCount = 0
badFrameCount = 0

def formatHex(bytArr):
    return ''.join('{:02x}'.format(x) for x in bytArr)

def addByte(byt):
    global framePos, frameData, rxCRC, escVal, okFrameCount, badFrameCount
    if framePos == 0:
        if byt == 0x7E:
            framePos = 1
    elif byt == 0x7E:
        if len(frameData) == 0:
            return
        rxCRC = frameData[-2:]
        calculatedCRC = calcCRC(frameData[:-2])
        if calculatedCRC == bytearray([0xff, 0xff]):
            print("hello")
        if rxCRC != calculatedCRC:
            print(f"CRC error offset {f.tell()} rx {formatHex(rxCRC)} != calc {formatHex(calculatedCRC)}")
            badFrameCount += 1
        else:
            okFrameCount += 1
            # print("Frame, len", len(frameData))
            frameData = frameData[:-2]
            ascChars = [chr(byt) if byt < 0x7e else ("\\x%02x" % byt) for byt in frameData[:180]]
            print("".join(ascChars))
        framePos = 0
        frameData = bytearray()
    elif byt == 0x7D:
        escVal = True
    elif escVal:
        dat = byt ^ 0x20
        frameData.append(dat)
        escVal = False
    else:
        frameData.append(byt)

def calcCRC(data):
    crc = CRCCCITT("FFFF").calculate(bytes(data))
    b = bytearray(struct.pack(">H", crc))
    return b

# Check dumpfile
with open(dumpFileName, "rb") as f:
    cnt = 0
    while(True):
        linPos = 0
        try:
            dat = f.read(1)
            # print(dat)
        except:
            break
        # print(f"{dat[0]:02x}", end=" ")
        if (len(dat) < 1):
            break
        addByte(int(dat[0]))
        # cnt += 1
        # if cnt > 1000:
        #     break
    print()
    print(f"OkFrames {okFrameCount} badFrames {badFrameCount}")
