from SimpleHDLC import HDLC
import serial
import logging
import os
import time
import msvcrt

def onFrame(fr):
    print(fr)
    
logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

galaxyFrame = bytearray(b"{\"cmdName\":\"filetarget\",\"fileType\":\"trs80cmd\"}\0")
galaxyFrame += bytearray(b"TESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTESTTEST")

with serial.Serial('COM15', 115200) as s:
    h = HDLC(s)
    h.startReader(onFrame)

    while True:
        if msvcrt.kbhit():
            k = msvcrt.getch()
#            print(k[0])
            if k[0] == ord("h"):
                h.sendFrame(galaxyFrame)
                print("Sent cmd len", len(galaxyFrame))
            elif k[0] == ord('\x1b'):
                h.stopReader()
                exit(0)
        
