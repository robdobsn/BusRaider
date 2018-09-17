from SimpleHDLC import HDLC
import serial
import logging
import os
import time
import msvcrt

def onFrame(fr):
    print(fr)
    
logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

with open(r"../../TargetSW/TRS80/Games/galinv1d.cmd", "rb") as galaxyFile:
    galaxyData = galaxyFile.read()
    
galaxyFrame = bytearray(b"{\"cmdName\":\"filetarget\",\"fileType\":\"trs80cmd\"}\0")
galaxyFrame += galaxyData

with serial.Serial('COM15', 115200) as s:
    h = HDLC(s)
    h.startReader(onFrame)

    while True:
        if msvcrt.kbhit():
            k = msvcrt.getch()
            if k == "h":
                h.sendFrame(galaxyFrame)
                print("Sent cmd len", len(galaxyFrame), galaxyFrame)
            elif k == " ":                   
                h.stopReader()
                exit(0)
        
