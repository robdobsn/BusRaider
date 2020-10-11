from threading import Thread
import serial
import time
from .LikeHDLC import HDLC
from .ProtocolOverAscii import ProtocolOverAscii

class LikeCommsSerial:

    def __init__(self):
        self.isOpen = False
        self.serialReader = None
        self.serialPort = None
        self.serialThreadEn = False
        self.rxFrameCB = None
        self._hdlc = HDLC(self._onHDLCFrame, self._onHDLCError)
        self.overAscii = False

    def __del__(self):
        self.close()

    def setRxFrameCB(self, cb):
        self.rxFrameCB = cb

    def open(self, openParams):
        # Check not already open
        if self.isOpen:
            return True

        # Get params
        serialPort = openParams.get("serialPort", "")
        serialBaud = openParams.get("serialBaud", 115200)
        rxBufferSize = openParams.get("rxBufferSize", 20000)

        self.overAscii = openParams.get("ifType", "plain") != "plain"
        if self.overAscii:
            self.protocolOverAscii = ProtocolOverAscii()
            print("Protocol overAscii")

        # Open serial port
        self.serialPort = serial.Serial(serialPort, serialBaud)
        self.serialPort.set_buffer_size(rxBufferSize, None)

        # Start receive loop
        self.serialThreadEn = True
        self.serialReader = Thread(target=self._serialRxLoop)
        self.serialReader.setDaemon(True)
        self.serialReader.start()
        self.isOpen = True
        return True

    def close(self):
        if not self.isOpen:
            return
        if self.serialReader is not None:
            self.serialThreadEn = False
            time.sleep(0.01)
            self.serialReader.join()
            self.serialReader = None
        if self.serialPort is not None:
            self.serialPort.close()
            self.serialPort = None
        self.isOpen = False

    def send(self, frame):
        # print("Sending to IF len", len(bytesToSend), " ", str(bytesToSend))
        hdlcEncoded = self._hdlc._encode(frame)
        if self.overAscii:
            encodedFrame = ProtocolOverAscii.encode(hdlcEncoded)
            self._sendBytesToIF(encodedFrame)
            # print(time.time(), ''.join('{:02x}'.format(x) for x in encodedFrame))
        else:
            self._sendBytesToIF(hdlcEncoded)
        
    # def getHDLCStats(self):
    #     return self._hdlc.getStats()

    def _serialRxLoop(self):
        while self.serialThreadEn:
            i = self.serialPort.in_waiting
            if i < 1:
                continue
            byt = self.serialPort.read(i)
            for b in byt:
                if self.overAscii:
                    if b >= 128:
                        decodedVal = self.protocolOverAscii.decodeByte(b)
                        if decodedVal >= 0:
                            self._hdlc.handleRxByte(decodedVal)
                            # print(f"{decodedVal:02x}")
                    else:
                        print(chr(b),end='')
                else:
                    self._hdlc.handleRxByte(b)
            # print(time.time(), ''.join('{:02x}'.format(x) for x in byt))
        # print("Exiting serialRxLoop")

    def _onHDLCFrame(self, frame):
        if self.rxFrameCB is not None:
            self.rxFrameCB(frame)
        
    def _onHDLCError(self):
        # print("HDLC error")
        pass
        
    def _sendBytesToIF(self, bytesToSend):
        # print("Sending to IF len", len(bytesToSend), " ", str(bytesToSend))
        if not self.isOpen:
            return
        # print(time.time(), ''.join('{:02x}'.format(x) for x in bytesToSend))
        if self.serialPort is not None:
            self.serialPort.write(bytesToSend)
