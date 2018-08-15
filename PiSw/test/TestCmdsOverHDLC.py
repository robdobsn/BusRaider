from SimpleHDLC import HDLC
import serial
import logging
import os
import time

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

with open(r"../../TRS80SW/galinv1d.cmd", "rb") as galaxyFile:
    galaxyData = galaxyFile.read()
    
galaxyFrame = bytearray(b"{\"cmdName\":\"filetarget\",\"fileType\":\"trs80cmd\"}\0")
galaxyFrame += galaxyData

with serial.Serial('COM6', 921600) as s:
    h = HDLC(s)
    h.sendFrame(galaxyFrame)
    print("Sent cmd len", len(galaxyFrame))
