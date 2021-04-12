'''
Serial communications with a Robotical RIC
'''
from threading import Thread
from typing import Callable, Dict, Union
import serial
import time
import logging
from serial.serialutil import SerialException
from .RICCommsBase import RICCommsBase
from .LikeHDLC import LikeHDLC
from .ProtocolOverAscii import ProtocolOverAscii
from .Exceptions import MartyConnectException

logger = logging.getLogger(__name__)

class RICCommsSerial(RICCommsBase):
    '''
    RICCommsSerial
    Provides a serial interface for RIC communications
    '''
    def __init__(self) -> None:
        '''
        Initialise RICCommsSerial
        '''
        super().__init__()
        self._isOpen = False
        self.serialReaderThread: Thread = None
        self.serialDevice: serial.Serial = None
        self.serialThreadEnabled = False
        self._hdlc = LikeHDLC(self._onHDLCFrame, self._onHDLCError)
        self.overAscii = False
        self.serialLogLine = ""
        self.serialPortErrors = 0

    def __del__(self) -> None:
        '''
        Destructor
        '''
        try:
            self.close()
        except:
            pass

    def isOpen(self) -> bool:
        '''
        Check if comms open
        Returns:
            True if comms open
        '''
        return self._isOpen

    def open(self, openParams: Dict) -> bool:
        '''
        Open connection
        Protocol can be plain (if the port is not used for any other purpose) or
                 overascii (if the port is also used for logging information)
        Args:
            openParams: dict containing params used to open the connection, may include
                        "serialPort", 
                        "serialBaud",
                        "ifType" == "plain" or "overascii",
                        "asciiEscapes"
        Returns:
            True if open succeeded or already open
        Throws:
            MartyConnectException: if connection cannot be opened
        '''
        # Check not already open
        if self._isOpen:
            return True

        # Get params
        self.commsParams.conn = openParams
        self.commsParams.fileTransfer = {"fileBlockMax": 5000, "fileXferSync": False}
        serialPort = openParams.get("serialPort", "")
        serialBaud = openParams.get("serialBaud", 115200)
        self.overAscii = openParams.get("ifType", "plain") != "plain"
        if self.overAscii:
            self.protocolOverAscii = ProtocolOverAscii()
        hdlcAsciiEscapes = openParams.get("asciiEscapes", False)

        # Validate
        if len(serialPort) == 0:
            return False

        # Open serial port
        try:
            self.serialDevice = serial.Serial(port=None, baudrate=serialBaud)
            self.serialDevice.port = serialPort
            self.serialDevice.rts = 0
            self.serialDevice.dtr = 0
            self.serialDevice.dsrdtr = False
            self.serialDevice.open()
        except Exception as excp:
            raise MartyConnectException("Serial port problem") from excp

        # Configure HDLC
        self._hdlc.setAsciiEscapes(hdlcAsciiEscapes)
        
        # Start receive loop
        self.serialThreadEnabled = True
        self.serialReaderThread = Thread(target=self._serialRxLoop)
        self.serialReaderThread.daemon = True
        self.serialReaderThread.start()
        self._isOpen = True
        return True

    def close(self) -> None:
        '''
        Close serial port
        '''
        if not self._isOpen:
            return
        # Stop thread function
        if self.serialReaderThread is not None:
            self.serialThreadEnabled = False
            time.sleep(0.01)
            self.serialReaderThread.join()
            self.serialReaderThread = None
        # Close port
        if self.serialDevice is not None:
            self.serialDevice.close()
            self.serialDevice = None
        self._isOpen = False

    def send(self, data: bytes) -> None:
        '''
        Send data to serial port
        Args:
            data: bytes to send on serial port
        Returns:
            none
        Throws:
            MartyConnectException: if the connection has an error
        '''
        # logger.debug(f"Sending to IF len {len(bytesToSend)} {str(bytesToSend)}")
        hdlcEncoded = self._hdlc.encode(data)
        try:
            if self.overAscii:
                encodedFrame = ProtocolOverAscii.encode(hdlcEncoded)
                self._sendBytesToIF(encodedFrame)
                # logger.debug(f"send {''.join('{:02x}'.format(x) for x in encodedFrame)}")
            else:
                self._sendBytesToIF(hdlcEncoded)
        except Exception as excp:
            raise MartyConnectException("Serial send problem") from excp

    def _serialRxLoop(self) -> None:
        '''
        Thread function used to process serial port data
        '''
        while self.serialThreadEnabled:
            i = self.serialDevice.in_waiting
            if i < 1:
                time.sleep(0.001)
                continue
            byt = self.serialDevice.read(i)
            for b in byt:
                if self.overAscii:
                    if b >= 128:
                        decodedVal = self.protocolOverAscii.decodeByte(b)
                        if decodedVal >= 0:
                            # logger.debug(f"{decodedVal:02x}")
                            self._hdlc.decodeData(decodedVal)
                    else:
                        if b == 0x0a:
                            if self.logLineCB:
                                self.logLineCB(self.serialLogLine)
                            self.serialLogLine = ""
                        else:
                            self.serialLogLine += chr(b)
                else:
                    self._hdlc.decodeData(b)
            # logger.debug(f"CommsSerial rx {''.join('{:02x}'.format(x) for x in byt)}")
        # logger.debug("Exiting serialRxLoop")

    def _onHDLCFrame(self, frame: bytes) -> None:
        if self.rxFrameCB is not None:
            self.rxFrameCB(frame)
        
    def _onHDLCError(self) -> None:
        pass
        
    def _sendBytesToIF(self, bytesToSend: bytes) -> None:
        # logger.debug(f"Sending to IF len {len(bytesToSend)} {str(bytesToSend)}")
        if not self._isOpen:
            return
        # logger.debug(f"CommsSerial sendBytesToIF {''.join('{:02x}'.format(x) for x in bytesToSend)}")
        if self.serialDevice is not None:
            try:
                self.serialDevice.write(bytesToSend)
            except SerialException as excp:
                # The port has connected but now failing to send characters
                # This may sort itself out or require user intervention
                self.serialPortErrors += 1
                pass

    def getTestOutput(self) -> dict:
        return {}
        