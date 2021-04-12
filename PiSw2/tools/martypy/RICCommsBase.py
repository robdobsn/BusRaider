'''
Generic communications with a Robotical RIC
'''
from .RICCommsParams import RICCommsParams
from typing import Callable, Dict, Union
import logging
from .LikeHDLC import LikeHDLC
from .ProtocolOverAscii import ProtocolOverAscii

logger = logging.getLogger(__name__)

class RICCommsBase:
    '''
    RICCommsBase
    Provides an interface for RIC communications
    '''
    def __init__(self) -> None:
        '''
        Initialise RICCommsBase
        '''
        self.rxFrameCB = None
        self.logLineCB = None
        self.commsParams = RICCommsParams()

    def __del__(self) -> None:
        '''
        Destructor
        '''
        self.close()

    def isOpen(self) -> bool:
        '''
        Check if comms open
        Returns:
            True if comms open
        '''
        return False

    def setRxFrameCB(self, onFrame: Callable[[Union[bytes, str]], None]) -> None:
        '''
        Set callback on frame received
        Args:
            onFrame: callback function (takes 1 parameter which is a received frame)
        Returns:
            None
        '''
        self.rxFrameCB = onFrame

    def setRxLogLineCB(self, onLogLine: Callable[[str], None]) -> None:
        '''
        Set callback on logging line received
        Args:
            onLogLine: callback function (takes 1 parameter which is the line of logging information)
        Returns:
            None
        '''
        self.logLineCB = onLogLine

    def open(self, openParams: Dict) -> bool:
        '''
        Open connection
        For serial communications: protocol can be plain (if the port is not used 
                    for any other purpose) or overascii (if the port is also used 
                    for logging information)
        Args:
            openParams: dict containing params used to open the connection, may include
                        "serialPort", 
                        "serialBaud",
                        "ifType" == "plain" or "overascii"
        Returns:
            True if open succeeded or already open
        Throws:
            MartyConnectException: if a connection cannot be opened
        '''
        return False

    def close(self) -> None:
        '''
        Close connection
        '''
        pass

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
        pass

    def getTestOutput(self) -> dict:
        return {}
