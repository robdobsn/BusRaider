from SimpleHDLC import HDLC
import serial
import logging
import os
import time
import keyboard

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

def onFrame(frame):
    print(frame)

def onError(err):
    print(err)
          
with serial.Serial('COM5', 115200) as s:
    h = HDLC(s)

    h.startReader(onFrame, onError)

    while True:
        if keyboard.is_pressed(' '):
            break    

    h.stopReader()

    
