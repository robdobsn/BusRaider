'''
WebSocketFrame
'''
import io
import struct
import secrets
import itertools
import collections
import logging
import time
from typing import Deque

logger = logging.getLogger(__name__)

class WebSocketFrame():

    max_size = 20000
    OPCODE_CONT = 0x00
    OPCODE_TEXT = 0x01
    OPCODE_BINARY = 0x02
    OPCODE_CLOSE = 0x08
    OPCODE_PING = 0x09
    OPCODE_PONG = 0x0a

    def __init__(self) -> None:
        self.curInputData: bytearray = bytearray()
        self.rxFrameData: bytearray = bytearray()
        self.rxFrameIsText = False
        self.pongRequired = False
        self.pongData = bytearray()
        self.closeRequired = False
        self.textMsgs: Deque[str] = collections.deque()
        self.binaryMsgs: Deque[bytes] = collections.deque()
        self.statsPings = 0
        self.statsPongs = 0
        self.statsText = 0
        self.statsBinary = 0

    def getPongRequired(self) -> bool:
        tmpPongReq = self.pongRequired
        self.pongRequired = False
        return tmpPongReq

    def getPongData(self) -> bytes:
        return self.pongData

    def getCloseRequired(self) -> bool:
        return self.closeRequired

    def getTextMsg(self) -> str:
        if len(self.textMsgs) > 0:
            return self.textMsgs.popleft()
        return None

    def getBinaryMsg(self) -> bytes:
        if len(self.binaryMsgs) > 0:
            binaryMsg = self.binaryMsgs.popleft()
            return binaryMsg
        return None

    def addDataToDecode(self, data: bytes) -> None:
        '''
        Add data to be decoded
        Args:
            data bytes to add
        Returns:
            None
        '''
        # Add data
        self.curInputData += data
        # logger.debug(f"{''.join('{:02x}'.format(x) for x in data)}")
        while self._extractFrames():
            pass

    def _extractFrames(self) -> bool:
        if len(self.curInputData) < 2:
            return False
        head1, head2 = struct.unpack("!BB", self.curInputData[:2])

        # Extract header info
        fin = (head1 & 0b10000000) != 0
        opcode = head1 & 0b00001111
        mask = (head2 & 0b10000000) != 0
        frameLen = head2 & 0b01111111
        curPos = 2
        if frameLen == 126:
            if len(self.curInputData) < 4:
                return False
            (frameLen,) = struct.unpack("!H", self.curInputData[2:4])
            curPos = 4
        elif frameLen == 127:
            if len(self.curInputData) < 10:
                return False
            (frameLen,) = struct.unpack("!Q", self.curInputData[2:10])
            curPos = 10
        if frameLen > self.max_size:
            self.curInputData.clear()
            return False
        if mask and len(self.curInputData) < curPos + 4:
            return False
        if mask:
            self.mask_bytes = self.curInputData[curPos:curPos+4]
            curPos += 4

        # Check data is present
        if len(self.curInputData) < curPos + frameLen:
            return False
        rxDataBlock = self.curInputData[curPos : curPos + frameLen]
        if mask:
            rxDataBlock = self.applyMask(rxDataBlock, self.mask_bytes)
        
        # Remove data consumed
        self.curInputData = self.curInputData[curPos + frameLen:]

        # Check for pings
        if opcode == self.OPCODE_PING:
            self.pongRequired = True
            self.pongData = rxDataBlock
            self.statsPings += 1
            # logger.debug(f"wsFrame PING {''.join('{:02x}'.format(x) for x in rxDataBlock)}")
            return True
        elif opcode == self.OPCODE_PONG:
            self.statsPongs += 1
            # logger.debug(f"wsFrame PONG {''.join('{:02x}'.format(x) for x in rxDataBlock)}")
            return True
        elif opcode == self.OPCODE_CLOSE:
            # logger.debug(f"wsFrame CLOSE")
            self.closeRequired = True
            return True
        elif opcode == self.OPCODE_BINARY:
            # logger.debug(f"wsFrame BINARY PART {''.join('{:02x}'.format(x) for x in rxDataBlock)}")
            self.rxFrameIsText = False
        elif opcode == self.OPCODE_TEXT:
            # logger.debug(f"wsFrame TEXT PART {''.join('{:02x}'.format(x) for x in rxDataBlock)}")
            self.rxFrameIsText = True

        # Add data to received frame
        self.rxFrameData += rxDataBlock

        # Add completed frame to queue
        if fin:
            if self.rxFrameIsText:
                self.textMsgs.append(self.rxFrameData.decode('utf-8'))
                self.statsText += 1
            else:
                # logger.debug(f"wsFrame BINARY DONE {''.join('{:02x}'.format(x) for x in self.rxFrameData)}")
                self.binaryMsgs.append(self.rxFrameData[:])
                self.statsBinary += 1
            self.rxFrameData.clear()
        return True

    @classmethod
    def encode(self, inFrame: bytes, useMask: bool,
                opcode: int, fin: bool) -> bytes:

        output = io.BytesIO()

        # Prepare the header.
        head1 = 0b10000000 if fin else 0 | opcode
        head2 = 0b10000000 if useMask else 0
        length = len(inFrame)
        if length < 126:
            output.write(struct.pack("!BB", head1, head2 | length))
        elif length < 65536:
            output.write(struct.pack("!BBH", head1, head2 | 126, length))
        else:
            output.write(struct.pack("!BBQ", head1, head2 | 127, length))

        if useMask:
            mask_bytes = secrets.token_bytes(4)
            output.write(mask_bytes)
            data = self.applyMask(inFrame, mask_bytes)
        else:
            data = inFrame
        output.write(data)
        return output.getvalue()

    @classmethod
    def applyMask(cls, data: bytes, mask: bytes) -> bytes:
        if len(mask) != 4:
            return
        return bytes(b ^ m for b, m in zip(data, itertools.cycle(mask)))
