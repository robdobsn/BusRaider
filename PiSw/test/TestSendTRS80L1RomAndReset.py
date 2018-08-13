from SimpleHDLC import HDLC
import serial
import logging
import os
import time

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

with open(r"../../ROMS/TRS80/level1.srec", "rb") as romFile:
    romData = romFile.read()

cmdFrame = bytearray(b"srectarget\0")
cmdFrame += romData

resetFrame = b"resettarget\0"
progFrame = b"programtarget\0"
ioclearFrame = b"ioclrtarget\0"

with serial.Serial('COM6', 921600) as s:
    h = HDLC(s)
    h.sendFrame(ioclearFrame)
    print("Sent ioclear len", len(ioclearFrame))
    time.sleep(1.0)
    h.sendFrame(cmdFrame)
    print("Sent srectarget len", len(cmdFrame))
    time.sleep(1.0)
    h.sendFrame(progFrame)
    print("Sent progtarget len", len(progFrame))
    time.sleep(2.0)
    h.sendFrame(resetFrame)
    print("Sent resettarget len", len(resetFrame))
    
