import serial
import sys
import time
import math
from SimpleHDLC import HDLC

try:
    testname = ' '.join(sys.argv[1:2])
    flashport = ' '.join(sys.argv[2:3])
    flashbaud = int(' '.join(sys.argv[3:]))
except Exception:
    print("Usage: python testserial.py TESTNAME SERIALPORT BAUDRATE")
    exit()

try:
    ser = serial.Serial(
        port=flashport,
        baudrate=flashbaud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
    )
except Exception:
    print("ERROR: Serial Port " + flashport + " busy or inexistent!")
    exit()

# try:
#     F = open(fileaddress,"rb")
#     F.seek(0, 2)
#     size = F.tell()
#     F.seek(0, 0)
# except Exception:
#     print("ERROR: Cannot access file " + fileaddress + ". Check permissions!")
#     ser.close()
#     exit()

print("Testing with baudrate of "+ str(flashbaud) + "...")
sys.stdout.flush()

numCharsToSend = 100000
try:
    lastFewChars = ""
    prevTime = time.time()
    while True:
        bytesToRead = ser.inWaiting()
        if bytesToRead > 0:
            dataRead = ser.read(bytesToRead)
            ascii = dataRead.decode('ascii',errors='ignore')
            print(ascii)
            lastFewChars = lastFewChars[-3:] + ascii
        if time.time() - prevTime > 10:
            break
        if "~~~" in lastFewChars:
            print("Received ~~~ going to send now")
            break
except Exception as excp:
    print("ERROR: Serial port?", excp)
    ser.close()
    exit()

if testname == "test1":
    try:
        print(f"Sending {numCharsToSend} bytes ...")
        sendblock = bytearray()
        for i in range(numCharsToSend):
            sendblock.append(0xaa)
        ser.write(sendblock)
    except Exception as excp:
        print("ERROR: Serial port?", excp)
        ser.close()
        exit()
else:
    with open("kernel.img", "rb") as f:
        dataBlock = f.read()
        fileLen = len(dataBlock)
        print("File is ", fileLen, " bytes long")
        startFrame = bytearray(b"{\"cmdName\":\"ufStart\",\"fileName\":\"kernel.img\",\"fileType\":\"firmware\",\"fileLen\":\"" + bytes(str(fileLen),"ascii") + b"\"}\0")
        # galaxyFrame += bytearray(b"{TESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTEST}")
        h = HDLC(ser)
        # def onFrame(fr):
        #     print(fr)
        # h.startReader(onFrame)
        # numFrames = 1000
        # for i in range(numFrames):
        #     h.sendFrame(galaxyFrame)
        # print("Sent hdlc len", len(galaxyFrame))
        # h.stopReader()
        h.sendFrame(startFrame)
        blockMaxSize = 1000
        numBlocks = fileLen//blockMaxSize + (0 if (fileLen % blockMaxSize == 0) else 1)
        for i in range(numBlocks):
            blockStart = i*blockMaxSize
            blockToSend = dataBlock[blockStart:blockStart+blockMaxSize]
            dataFrame = bytearray(b"{\"cmdName\":\"ufBlock\",\"index\":\"" + bytes(str(blockStart),"ascii") + b"\"}\0") + blockToSend
            h.sendFrame(dataFrame)
        endFrame = bytearray(b"{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + bytes(str(numBlocks),"ascii") + b"\"}\0")
        h.sendFrame(endFrame)

try:
    prevTime = time.time()
    while True:
        bytesToRead = ser.inWaiting()
        if bytesToRead > 0:
            dataRead = ser.read(bytesToRead)
            ascii = dataRead.decode("ascii", errors="ignore")
            print(ascii)
        if time.time() - prevTime > 15:
            break
except Exception as excp:
    print("ERROR: Serial port?", excp)
    ser.close()
    exit()

ser.close()
