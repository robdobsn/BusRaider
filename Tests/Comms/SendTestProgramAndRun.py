from SimpleHDLC import HDLC
import serial
import logging
import os
import time

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

testCodePath = r"Tests/testZ80Lissajous"
testCodeFile = r"/lissajous_CODE.bin"
testSetMcToRobs = True
# testCodePath = r"Tests/testZ80AsmTestMem/"
# testCodeFile = r"testZ80AsmTestMem.bin"
# testSetMcToRobs = False

def getFileData(fileFolder, fileName):
    inData = []
    try:
        with open(r"../../" + fileFolder + fileName, "rb") as inFile:
            inData = inFile.read()
    except:
        pass
    if len(inData) == 0:
        try:
            with open(fileFolder + fileName, "rb") as inFile:
                inData = inFile.read()
        except:
            print("Unable to load file ", fileFolder + fileName)
            exit(1)
    return inData

def formFileFrame(fileFolder, fileName):
    fileFrame = bytearray(b"{\"cmdName\":\"FileTarget\",\"fileName\":") + bytearray(fileName, "utf-8") + bytearray(b"}\0")
    fileFrame += getFileData(fileFolder, fileName)
    return fileFrame

romFrame = formFileFrame(testCodePath, testCodeFile)

testData = bytearray()
for i in range(4096):
    testData.append((i + 33) % 256)
#testData[22] = 0
setTestDataFrame = b"{\"cmdName\":\"FileTarget\",,\"fileName\":\"test.bin\",\"baseAddr\":\"4000\"}\0" 
setTestDataFrame += testData

clearFrame = b"{\"cmdName\":\"imagerClear\"}\0"
#resetFrame = b"{\"cmdName\":\"ResetTarget\"}\0"
progFrame = b"{\"cmdName\":\"imagerWriteAndExec\"}\0"
ioclearFrame = b"{\"cmdName\":\"IOClrTarget\"}\0"
setMCFrame = b"{\"cmdName\":\"SetMachine=Rob's Z80\"}\0"

with serial.Serial('COM6', 115200) as s:
    h = HDLC(s)
    if testSetMcToRobs:
        h.sendFrame(setMCFrame)
        print("Set Machine to RobsZ80")
        time.sleep(2.0)
    h.sendFrame(clearFrame)
    print("Clear Target Buffer")
    time.sleep(1.0)
    h.sendFrame(romFrame)
    print("Sent ROM", len(romFrame))
    time.sleep(0.1)
    h.sendFrame(setTestDataFrame)
    print("Sent Test Data", len(setTestDataFrame))
    time.sleep(0.1)
    h.sendFrame(progFrame)
    print("Sent Program and Reset")
