from SimpleHDLC import HDLC
import serial
import logging
import os
import time

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

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

romFrame = formFileFrame(r"TargetSW/TRS80/ROMS/", r"level1.rom")
programFrame = formFileFrame(r"TargetSW/TRS80/Games/", r"galinv1d.cmd")

clearFrame = b"{\"cmdName\":\"progClear\"}\0"
progFrame = b"{\"cmdName\":\"progWriteAndExec\"}\0"
setMCFrame = b"{\"cmdName\":\"SetMachine=TRS80\"}\0"

with serial.Serial('COM6', 115200) as s:
    h = HDLC(s)
    # h.sendFrame(setMCFrame)
    # print("Sent setmachine=TRS80 len", len(setMCFrame))
    # time.sleep(2.0)
    h.sendFrame(clearFrame)
    print("Clear Target Buffer")
    time.sleep(1.0)
    h.sendFrame(romFrame)
    print("Sent ROM", len(romFrame))
    time.sleep(2.1)
    h.sendFrame(programFrame)
    print("Sent Program", len(romFrame))
    time.sleep(2.1)
    h.sendFrame(progFrame)
    print("Sent Program and Reset")
