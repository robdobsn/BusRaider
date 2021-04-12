'''
Test communications with a Robotical RIC
'''
from threading import Thread
from typing import Callable, Dict, Union
import serial
import time
import copy
import logging
from .RICCommsBase import RICCommsBase
from .Exceptions import MartyConnectException

logger = logging.getLogger(__name__)

class RICCommsTest(RICCommsBase):
    def __init__(self) -> None:
        super().__init__()
        self._isOpen = True
        self.clearTestOutput()

    def __del__(self) -> None:
        try:
            self.close()
        except:
            pass

    def isOpen(self) -> bool:
        return self._isOpen

    def open(self, openParams: Dict) -> bool:
        self.commsParams.conn = openParams
        self.commsParams.fileTransfer = {"fileBlockMax": 5000, "fileXferSync": False}
        return True

    def close(self) -> None:
        self._isOpen = False

    def send(self, data: bytes) -> None:
        self.outputInfo["buf"].append(data)

    def getTestOutput(self) -> dict:
        dictCopy = copy.deepcopy(self.outputInfo)
        self.clearTestOutput()
        return dictCopy

    def clearTestOutput(self) -> None:
        self.outputInfo: Dict[str, list] = {"buf":[]}
