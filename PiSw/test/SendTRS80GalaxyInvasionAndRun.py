from SimpleHDLC import HDLC
import serial
import logging
import os
import time

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

galaxyData = []
try:
    with open(r"../../TargetSW/TRS80/Games/galinv1d.cmd", "rb") as galaxyFile:
        galaxyData = galaxyFile.read()
except:
    print("Not found ... trying alternate")

if len(galaxyData) == 0:
    try:
        with open(r"TargetSW/TRS80/Games/galinv1d.cmd", "rb") as galaxyFile:
            galaxyData = galaxyFile.read()
    except:
        print("Not found there either")
        exit()

romFrame = bytearray(b"{\"cmdName\":\"filetarget\",\"fileType\":\"trs80bin\"}\0")
romFrame += romData

galaxyFrame = bytearray(b"{\"cmdName\":\"filetarget\",\"fileType\":\"trs80cmd\"}\0")
galaxyFrame += galaxyData

clearFrame = b"{\"cmdName\":\"cleartarget\"}\0"
resetFrame = b"{\"cmdName\":\"resettarget\"}\0"
progFrame = b"{\"cmdName\":\"programtarget\"}\0"
ioclearFrame = b"{\"cmdName\":\"ioclrtarget\"}\0"

with serial.Serial('COM6', 921600) as s:
    h = HDLC(s)
    h.sendFrame(clearFrame)
    print("Sent cleartarget len", len(clearFrame))
    time.sleep(1.0)
    h.sendFrame(ioclearFrame)
    print("Sent ioclear len", len(ioclearFrame))
    time.sleep(1.0)
    h.sendFrame(romFrame)
    print("Sent ROM srcs len", len(romFrame))
    time.sleep(1.0)
    h.sendFrame(progFrame)
    print("Sent progtarget len", len(progFrame))
    time.sleep(2.0)
    h.sendFrame(resetFrame)
    print("Sent resettarget len", len(resetFrame))
    time.sleep(2.0)    
    h.sendFrame(galaxyFrame)
    print("Sent Galaxy Invasion srcs len", len(galaxyFrame))
    time.sleep(1.0)
    h.sendFrame(progFrame)
    print("Sent progtarget len", len(progFrame))
    time.sleep(2.0)
    h.sendFrame(resetFrame)
    print("Sent resettarget len", len(resetFrame))
    time.sleep(2.0)
