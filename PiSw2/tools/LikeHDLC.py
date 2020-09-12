#!/usr/bin/python
# coding: utf8

__version__ = '0.5.0'

import logging
import struct
import time
from threading import Thread
import sys

logger = logging.getLogger(__name__)

class HDLCStats:
    crcErrors = 0

# Frame
class Frame(object):
    STATE_READ = 0x01
    STATE_ESCAPE = 0x02
    FRAME_DELIMITER = 0x7E
    ESCAPE_CODE = 0x7D

    def __init__(self):
        self.finished = False
        self.error = False
        self.state = self.STATE_READ
        self.data = bytearray()
        self.crc = bytearray()
        self.reader = None

    def __len__(self):
        return len(self.data)

    def addByte(self, b):
        if b == self.ESCAPE_CODE:
            self.state = self.STATE_ESCAPE
        elif self.state == self.STATE_ESCAPE:
            self.state = self.STATE_READ
            b = b ^ 0x20
            self.data.append(b)
        else:
            self.data.append(b)

    def finish(self):
        self.crc = self.data[-2:]
        self.data = self.data[:-2]
        self.finished = True

    def checkCRC(self):
        calculatedCRC = HDLC.calcCRC(self.data)
        res = bool(self.crc == calculatedCRC)
        if not res:
            frameStr = ''.join('{:02x}'.format(x) for x in self.data)
            calcCRCStr = ''.join('{:02x}'.format(x) for x in calculatedCRC)
            receivedCRC = ''.join('{:02x}'.format(x) for x in self.crc)
            logger.warning(f"invalid rx crc 0x{receivedCRC} != 0x{calcCRCStr} calculated Len {len(self.data)} Frame {frameStr}")
            self.error = True
        return res
        
    def toString(self):
        return self.data.decode('utf-8').rstrip('\0')

# HDLC
class HDLC:
    def __init__(self, onFrame, onError):
        self.currentFrame = None
        self.payloadsAreStrings = False
        self.onFrame = onFrame
        self.onError = onError
        self.stats = HDLCStats()

    @classmethod
    def toBytes(cls, data):
        return bytearray(data)

    @classmethod
    def calcCRC(cls, data):
        crc = 0xffff
        msb = crc >> 8
        lsb = crc & 255
        for c in data:
            x = c ^ msb
            x ^= (x >> 4)
            msb = (lsb ^ (x >> 3) ^ (x << 4)) & 255
            lsb = (x ^ (x << 5)) & 255
        return bytearray(struct.pack(">H", (msb << 8) + lsb))

    def handleRxByte(self, b):
        assert 0 <= b <= 255
        if b == Frame.FRAME_DELIMITER:
            # Start or End
            if (self.currentFrame is None) or len(self.currentFrame) < 1:
                # Start
                self.currentFrame = Frame()
                # print("START", time.time())
            else:
                # End
                self.currentFrame.finish()
                self.currentFrame.checkCRC()
                # print("END", time.time())
        else:
            if self.currentFrame is not None:
                if not self.currentFrame.finished:
                    # Add to current frame
                    self.currentFrame.addByte(b)

        # Validate and return
        if (self.currentFrame is not None) and self.currentFrame.finished:
            if not self.currentFrame.error:
                # Success
                if self.payloadsAreStrings:
                    self.onFrame(self.currentFrame.toString())
                else:
                    self.onFrame(self.currentFrame.data)
            elif self.currentFrame.finished:
                # Error
                self.stats.crcErrors += 1
                self.onError()
            self.currentFrame = None

    @classmethod
    def _encode(cls, bs):
        data = bytearray()
        data.append(Frame.FRAME_DELIMITER)
        crc = calcCRC(bs)
        bs = bs + crc
        for byte in bs:
            if byte == Frame.FRAME_DELIMITER or byte == Frame.ESCAPE_CODE:
                data.append(Frame.ESCAPE_CODE)
                data.append(byte ^ 0x20)
            else:
                data.append(byte)
        data.append(Frame.FRAME_DELIMITER)
        return bytes(data)

    def getStats(self):
        return self.stats
