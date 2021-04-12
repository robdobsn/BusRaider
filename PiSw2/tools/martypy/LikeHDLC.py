'''
HDLC-like communications
'''

__version__ = '0.7.0'

import logging
import struct
import time
from enum import Enum
from typing import Callable, Union

logger = logging.getLogger(__name__)

class HDLCStats:
    '''
    HDLCStats - statistics on HDLC-like communication
    '''
    crcErrors = 0

class HDLCState(Enum):
    STATE_READ = 1
    STATE_ESCAPE = 2

# Frame
class Frame(object):
    '''
    Frame - represents an encoded HDLC-like frame
    '''
    CRC16_LUT = [
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
        ]

    DELIMITER_CODE_NON_ASCII = 0xE7
    ESCAPE_CODE_NON_ASCII = 0xD7
    DELIMITER_CODE_ASCII = 0x7E
    ESCAPE_CODE_ASCII = 0x7D

    def __init__(self, delimiterCode: int, escapeCode: int) -> None:
        '''
        Initialise Frame

        Args:
            delimiterCode (for HDLC frame start/end)
            escapeCode (for HDLC escapes)

        Returns:
            None
        '''
        self.finished = False
        self.error = False
        self.state = HDLCState.STATE_READ
        self.data = bytearray()
        self.crc = bytearray()
        self.reader = None
        self.delimiterCode = delimiterCode
        self.escapeCode = escapeCode

    def __len__(self) -> int:
        return len(self.data)

    def addByte(self, b: int) -> None:
        '''
        Add byte to frame

        Args:
            b: byte to add

        Returns:
            None
        '''
        if b == self.escapeCode:
            self.state = HDLCState.STATE_ESCAPE
        elif self.state == HDLCState.STATE_ESCAPE:
            self.state = HDLCState.STATE_READ
            b = b ^ 0x20
            self.data.append(b)
        else:
            self.data.append(b)

    def finish(self) -> None:
        '''
        Complete frame creation - updates crc and data variables

        Args:
            none

        Returns:
            None
        '''
        self.crc = self.data[-2:]
        self.data = self.data[:-2]
        self.finished = True

    def checkCRC(self) -> bool:
        '''
        Check the CRC received matches expected

        Args:
            none

        Returns:
            bool - False indicates CRC check failure
        '''
        res = bool(self.crc == LikeHDLC.calcCRC(self.data))
        if not res:
            logger.warning(f"invalid crc {self.crc} != {LikeHDLC.calcCRC(self.data)}")
            self.error = True
        return res
        
    def toString(self) -> str:
        return self.data.decode('utf-8').rstrip('\0')

# LikeHDLC
class LikeHDLC:
    '''
    HDLC-like with encode and decode capabilities
    To decode, construct LikeHDLC and supply onFrame callback -
        then send data to decodeData() and onFrame
        callback will be called for each frame decoded
    To encode use class method encode()
    The communications protocol is based on HDLC but with some simplifications:
    - byte-wise only, escape codes can be modified from standard
    - encoding and decoding only
    '''
    def __init__(self, 
            onFrame: Callable[[Union[bytes, str]], None],
            onError: Callable[[], None],
            asciiEscapes: bool = False,
            payloadsAreStrings: bool = False) -> None:
        '''
        Initialise LikeHDLC
        Args:
            onFrame: callback function (takes 1 parameter which is a received frame)
            onError: callback function (0 parameters) when an error occurs
            asciiEscapes: bool - use the standard HDLC escape codes (which are ASCII values)
            payloadsAreStrings: bool - received frames are returned as strings (as opposed to bytes)
        Returns:
            None
        '''
        self.payloadsAreStrings = payloadsAreStrings
        self.onFrame = onFrame
        self.onError = onError
        self.asciiEscapes = asciiEscapes
        self.setAsciiEscapes(asciiEscapes)
        self.clear()
        self.clearStats()

    def setAsciiEscapes(self, asciiEscapes: bool = False) -> None:
        '''
        Set HDLC delimiter and escape codes

        Args:
            asciiEscapes: use standard ASCII values

        Returns:
            None
        '''
        self.delimiterCode = Frame.DELIMITER_CODE_ASCII if asciiEscapes \
                        else Frame.DELIMITER_CODE_NON_ASCII
        self.escapeCode = Frame.ESCAPE_CODE_ASCII if asciiEscapes \
                        else Frame.ESCAPE_CODE_NON_ASCII

    def clear(self) -> None:
        
        '''
        Get statistics on HDLC conversion
        Args:
            none
        Returns:
            None
        '''
        self.currentFrame = None

    def clearStats(self) -> None:
        '''
        Clear statistics on HDLC operation
        Args:
            none
        Returns:
            None
        '''
        self.stats = HDLCStats()

    def decodeData(self, b: int) -> None:
        '''
        Add a byte to be decoded
        Args:
            b byte to add
        Returns:
            None
        '''
        if b < 0 or b > 255:
            return
        if b == self.delimiterCode:
            # Start or End
            if (self.currentFrame is None) or len(self.currentFrame) < 1:
                # Start
                self.currentFrame = Frame(self.delimiterCode, self.escapeCode)
                # logger.debug(f"START time {time.time()}")
            else:
                # End
                self.currentFrame.finish()
                self.currentFrame.checkCRC()
                # logger.debug(f"END time {time.time()}")
        else:
            if self.currentFrame is not None:
                if not self.currentFrame.finished:
                    # Add to current frame
                    self.currentFrame.addByte(b)

        # Validate and callback if finished
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

    def getStats(self) -> HDLCStats:
        
        '''
        Get statistics on HDLC conversion

        Args:
            none

        Returns:
            HDLCStats object containing statistics
        '''
        return self.stats

    @classmethod
    def encode(cls, data: bytes, asciiEscapes: bool = False) -> bytes:
        '''
        Encode data into an HDLC-like frame

        Args:
            data: bytes representing the data

        Returns:
            bytes representing the frame
        '''
        delimiterCode = Frame.DELIMITER_CODE_ASCII if asciiEscapes \
                    else Frame.DELIMITER_CODE_NON_ASCII
        escapeCode = Frame.ESCAPE_CODE_ASCII if asciiEscapes \
                    else Frame.ESCAPE_CODE_NON_ASCII
        frame = bytearray()
        frame.append(delimiterCode)
        crc = cls.calcCRC(data)
        data = data + crc
        for byte in data:
            if byte == delimiterCode or byte == escapeCode:
                frame.append(escapeCode)
                frame.append(byte ^ 0x20)
            else:
                frame.append(byte)
        frame.append(delimiterCode)
        return bytes(frame)

    @classmethod
    def calcCRC(cls, data: bytes) -> bytes:
        
        '''
        Calculate CRC-CCITT 16Bit check code
        https://en.wikipedia.org/wiki/High-Level_Data_Link_Control

        Args:
            data: bytes representing the data

        Returns:
            bytes representing the checksum (2 bytes big-endian)
        '''
        
        crc = 0xffff
        for c in data:
            crc = ((crc<<8)&0xff00) ^ Frame.CRC16_LUT[((crc>>8)&0xff)^c]
        return bytearray(struct.pack(">H", crc))

    @classmethod
    def toBytes(cls, data):
        '''
        Convert iterable data (list, etc) into bytes

        Args:
            data: the data to turn into bytes (1 byte is added per value)

        Returns:
            None
        '''
        return bytearray(data)
