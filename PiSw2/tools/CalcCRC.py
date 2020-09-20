import argparse
import struct

def calcCRC(data):
    crc = 0xffff
    msb = crc >> 8
    lsb = crc & 255
    for c in data:
        x = c ^ msb
        x ^= (x >> 4)
        msb = (lsb ^ (x >> 3) ^ (x << 4)) & 255
        lsb = (x ^ (x << 5)) & 255
    return bytearray(struct.pack(">H", (msb << 8) + lsb))

argparser = argparse.ArgumentParser(description='CalcCRC')
argparser.add_argument('fileName', help='File Name')
args = argparser.parse_args()

with open(args.fileName, "rb") as inFile:
    dat = inFile.read()
    crc = calcCRC(dat)
    print(f"CRC 0x{crc[0]:02x}{crc[1]:02x}")
    
