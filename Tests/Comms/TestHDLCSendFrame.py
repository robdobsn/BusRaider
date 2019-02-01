from SimpleHDLC import HDLC
import serial
import logging
import os

logging.basicConfig(level=os.environ.get("LOGLEVEL", "INFO"))

from PyCRC.CRCCCITT import CRCCCITT
import struct

def calcCRC(data):
    crc = CRCCCITT("FFFF").calculate(bytes(data))
    b = bytearray(struct.pack(">H", crc))
    return b

print(calcCRC(b'0'))

with serial.Serial('COM6', 921600) as s:
    h = HDLC(s)
    h.sendFrame(b'0')

POLYNOMIAL = 0x1021
PRESET = 0xffff

def _initial(c):
    crc = 0
    c = c << 8
    for j in range(8):
        if (crc ^ c) & 0x8000:
            crc = (crc << 1) ^ POLYNOMIAL
        else:
            crc = crc << 1
        c = c << 1
    return crc & 0xffff

_tab = [ _initial(i) for i in range(256) ]

def _update_crc(crc, c):
    cc = 0xff & c

    tmp = (crc >> 8) ^ cc
    crc = (crc << 8) ^ _tab[tmp & 0xff]
    crc = crc & 0xffff
#    print (crc)

    return crc

def crc(str):
    crc = PRESET
    for c in str:
        crc = _update_crc(crc, ord(c))
    return crc

def crcb(*i):
    crc = PRESET
    for c in i:
        crc = _update_crc(crc, c)
    return crc

#print (crcb(0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39))
print("{:04x}".format(crc('0')))

print("TAB")
for t in _tab:
    print ("0x{:04x},".format(t), end="")
    
