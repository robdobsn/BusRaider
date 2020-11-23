#!/usr/bin/python
# coding: utf8

__version__ = '0.1.0'

# Frame
class ProtocolOverAscii():
    OVERASCII_ESCAPE_1 = 0x85
    OVERASCII_ESCAPE_2 = 0x8E
    OVERASCII_ESCAPE_3 = 0x8F
    OVERASCII_MOD_CODE = 0x20

    def __init__(self):
        self.escapeSeqCode = 0

    # Function to handle char for decoding
    # Returns -1 if char was an escape code - remainder will be returned later
    def decodeByte(self, ch):
        # Check if in escape sequence
        if self.escapeSeqCode != 0:
            prevEscCode = self.escapeSeqCode
            self.escapeSeqCode = 0
            if prevEscCode == 1:
                return (ch ^ self.OVERASCII_MOD_CODE) & 0x7f
            elif prevEscCode == 2:
                return ch ^ self.OVERASCII_MOD_CODE
            return ch
        elif ch == self.OVERASCII_ESCAPE_1:
            self.escapeSeqCode = 1
            return -1
        elif ch == self.OVERASCII_ESCAPE_2:
            self.escapeSeqCode = 2
            return -1
        elif ch == self.OVERASCII_ESCAPE_3:
            self.escapeSeqCode = 3
            return -1
        else:
            return ch & 0x7f

    # Encode frame
    # Values 0x00-0x0F map to ESCAPE_CODE_1, VALUE_XOR_20H_AND_MSB_SET
    # Values 0x10-0x7f map to VALUE_WITH_MSB_SET
    # Values 0x80-0x8F map to ESCAPE_CODE_2, VALUE_XOR_20H
    # Values 0x90-0xff map to ESCAPE_CODE_3, VALUE
    # Value ESCAPE_CODE_1 maps to ESCAPE_CODE_1 + VALUE
    @classmethod
    def encode(cls, inData):
        # Iterate over frame
        encodedFrame = bytearray()
        for toEnc in inData:
            # Handle escape chars
            if toEnc <= 0x0f:
                encodedFrame.append(ProtocolOverAscii.OVERASCII_ESCAPE_1)
                encodedFrame.append((toEnc ^ ProtocolOverAscii.OVERASCII_MOD_CODE) | 0x80)
            elif (toEnc >= 0x10) and (toEnc <= 0x7f):
                encodedFrame.append(toEnc | 0x80)
            elif (toEnc >= 0x80) and (toEnc <= 0x8f):
                encodedFrame.append(ProtocolOverAscii.OVERASCII_ESCAPE_2)
                encodedFrame.append(toEnc ^ ProtocolOverAscii.OVERASCII_MOD_CODE)
            else:
                encodedFrame.append(ProtocolOverAscii.OVERASCII_ESCAPE_3)
                encodedFrame.append(toEnc)
        return encodedFrame
