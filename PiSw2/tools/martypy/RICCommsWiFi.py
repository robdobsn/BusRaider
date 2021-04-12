'''
WiFi communications with a Robotical RIC
'''
from threading import Thread
from typing import Callable, Dict, Union
import time
import logging
from .RICCommsBase import RICCommsBase
from .LikeHDLC import LikeHDLC
from .Exceptions import MartyConnectException
from .WebSocket import WebSocket

logger = logging.getLogger(__name__)

class RICCommsWiFi(RICCommsBase):
    '''
    RICCommsWiFi
    Provides an interface for RIC communications
    '''
    def __init__(self) -> None:
        '''
        Initialise RICCommsWiFi
        '''
        super().__init__()
        self._isOpen = False
        self.webSocketThread: Thread = None
        self.webSocket: WebSocket = None
        self.webSocketThreadEnabled = False
        self._hdlc = LikeHDLC(self._onHDLCFrame, self._onHDLCError)
        self.socketErrors = 0

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
        return self.isOpen

    def open(self, openParams: Dict) -> bool:
        '''
        Open connection
        Args:
            openParams: dict containing params used to open the connection, may include
                        "ipAddrOrHostname", 
                        "ipPort",
                        "wsPath",
                        "asciiEscapes"
        Returns:
            True if open succeeded or already open
        Throws:
            MartyConnectException: if a connection cannot be opened
        '''
        # Check not already open
        if self._isOpen:
            return True

        # Get params
        self.commsParams.conn = openParams
        self.commsParams.fileTransfer = {"fileBlockMax": 5000, "fileXferSync": False}
        ipAddrOrHostname = openParams.get("ipAddrOrHostname", "")
        ipPort = openParams.get("ipPort", 80)
        wsPath = openParams.get("wsPath", "/ws")
        hdlcAsciiEscapes = openParams.get("asciiEscapes", False)

        # Validate
        if len(ipAddrOrHostname) == 0:
            return False

        # Open websocket
        try:
            self.webSocket = WebSocket(self._onWSBinaryFrame, 
                        self._onWSTextFrame, 
                        self._onWSError, 
                        ipAddrOrHostname, ipPort, wsPath)
            self.webSocket.open()
        except Exception as excp:
            raise MartyConnectException("Websocket problem") from excp

        # Configure HDLC
        self._hdlc.setAsciiEscapes(hdlcAsciiEscapes)

        # Start receive loop
        self.webSocketThreadEnabled = True
        self.webSocketThread = Thread(target=self._webSocketThreadFn)
        self.webSocketThread.daemon = True
        self.webSocketThread.start()
        self._isOpen = True
        return True

    def close(self) -> None:
        '''
        Close connection
        '''
        if not self._isOpen:
            return
        # Stop thread function
        if self.webSocketThread is not None:
            self.webSocketThreadEnabled = False
            time.sleep(0.01)
            self.webSocketThread.join()
            self.webSocketThread = None
        # Close socket
        if self.webSocket is not None:
            self.webSocket.close()
            self.webSocket = None
        self._isOpen = False

    def send(self, data: bytes) -> None:
        '''
        Send data
        Args:
            data: bytes to send
        Returns:
            none
        Throws:
            MartyConnectException: if the connection has an error
        '''
        # logger.debug(f"WiFi send len {len(data)} {''.join('{:02x}'.format(x) for x in data)}")
        hdlcEncoded = self._hdlc.encode(data)
        try:
            self._sendBytesToIF(hdlcEncoded)
        except Exception as excp:
            raise MartyConnectException("Connection send problem") from excp

    def _onHDLCFrame(self, frame: bytes) -> None:
        if self.rxFrameCB is not None:
            self.rxFrameCB(frame)
        
    def _onHDLCError(self) -> None:
        pass
        
    def _sendBytesToIF(self, bytesToSend: bytes) -> None:
        # logger.debug(f"Sending to IF len {len(bytesToSend)} {str(bytesToSend)}")
        if not self._isOpen:
            return
        # logger.debug(f"CommsWiFi sendBytesToIF {''.join('{:02x}'.format(x) for x in bytesToSend)}")
        if self.webSocket is not None:
            try:
                self.webSocket.writeBinary(bytesToSend)
            except Exception as excp:
                # The socket has connected but now failing to send characters
                # This may sort itself out or require user intervention
                self.socketErrors += 1
                pass

    def _webSocketThreadFn(self) -> None:
        '''
        Thread function used to process websocket data
        '''
        while self.webSocketThreadEnabled:
            try:
                self.webSocket.service()
            except OSError as excp:
                logger.debug(f"webSocket problem {excp}")
                self.webSocketThreadEnabled = False
                raise MartyConnectException("WebSocket closed unexpectedly")
            except Exception as excp:
                logger.debug(f"WebSocket exception {excp}")
            time.sleep(0.001)
        logger.debug("Exiting WebSocket thread")

    def _onWSBinaryFrame(self, rxFrame: bytes) -> None:
        # logger.debug(f"webSocketRx {''.join('{:02x}'.format(x) for x in rxFrame)}")
        for rxByte in rxFrame:
            self._hdlc.decodeData(rxByte)

    def _onWSTextFrame(self, rxFrame: str) -> None:
        logger.debug(f"webSocketRx TEXT UNEXPECTED {rxFrame}")

    def _onWSError(self, err: str) -> None:
        logger.debug(f"CommsWiFi WS error {err}")

    def getTestOutput(self) -> dict:
        return {}
        