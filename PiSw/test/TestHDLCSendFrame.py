from SimpleHDLC import HDLC
import serial
import logging
import os

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

with serial.Serial('COM6', 921600) as s:
    h = HDLC(s)
    h.sendFrame(b"hello")

