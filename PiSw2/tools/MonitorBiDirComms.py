import serial
import sys
import time
import math
import logging
import json
import os
import argparse
from LikeCommsSerial import LikeCommsSerial
if os.name == 'nt':
    import msvcrt

# Monitor rx and tx serial ports

class MonitorConn:

    def __init__(self, directionStr):
        self.commsHandler = LikeCommsSerial()
        self.directionStr = directionStr
        self.fileUploadInProgress = False
        self.fileUploadLastBlockAddr = 0
        self.fileUploadLastBlockLen = 0
        self.fileUploadBlockCount = 0

    def open(self, openParams):
        self.commsHandler.setRxFrameCB(self._onRxFrameCB)
        openOk = self.commsHandler.open(openParams)
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)

    def close(self):
        self.commsHandler.close()

    # Received frame handler
    def _onRxFrameCB(self, frame):
        # print(f"Frame {self.directionStr}") 
        msgContent = {'cmdName':''}
        termPos = frame.find(0)
        frameJson = frame
        payloadLen = 0
        if termPos >= 0:
            frameJson = frame[0:termPos]
            payloadLen = len(frame) - termPos - 1
        try:
            msgContent = json.loads(frameJson)
        except Exception as excp:
            self.logger.error(f"{self.directionStr} failed to parse Json from {frameJson} excp {excp}")
        if msgContent['cmdName'] == "ufStart":
            if self.fileUploadInProgress:
                self.logger.error(f"{self.directionStr} ufStart UNEXPECTED upload in progress {frameJson}")
            else:
                self.logger.info(f"{self.directionStr} ufStart UPLOAD START {frameJson}")
                self.fileUploadInProgress = True
                self.fileUploadLastBlockAddr = 0
                self.fileUploadLastBlockLen = 0
                self.fileUploadBlockCount
        elif msgContent['cmdName'] == "ufBlock":
            if not self.fileUploadInProgress:
                self.logger.error(f"{self.directionStr} ufBlock UNEXPECTED no upload in progress {frameJson} payloadLen {payloadLen}")
            else:
                blockStart = int(msgContent["index"])
                blockLen = int(msgContent["len"])
                expBlockStart = self.fileUploadLastBlockAddr + self.fileUploadLastBlockLen
                if blockStart != expBlockStart:
                    self.logger.info(f"{self.directionStr} ufBlock BLOCK MISSING? Expected Block Start {expBlockStart} BlkCnt {self.fileUploadBlockCount} {frameJson} payloadLen {payloadLen}")
                else:
                    self.logger.info(f"{self.directionStr} ufBlock Block OK BlkCnt {self.fileUploadBlockCount} frameLen {len(frame)} payloadLen {payloadLen} {frameJson}")
                self.fileUploadLastBlockAddr = blockStart
                self.fileUploadLastBlockLen = blockLen
                self.fileUploadBlockCount += 1
        elif msgContent['cmdName'] == "ufEnd":
                self.logger.info(f"{self.directionStr} ufEnd hdlcCRCErrs {self.commsHandler.getHDLCStats().crcErrors} {frameJson}")
                self.fileUploadInProgress = False
        elif msgContent['cmdName'] == "ufEndAck":
            if self.fileUploadInProgress:
                self.logger.error(f"{self.directionStr} ufEndAck UNEXPECTED no upload in progress {frameJson}")
            else:
                self.logger.info(f"{self.directionStr} ufEndAck {frameJson}")
        elif msgContent['cmdName'] == "ufEndNotAck":
            if self.fileUploadInProgress:
                self.logger.error(f"{self.directionStr} ufEndNotAck UNEXPECTED no upload in progress {frameJson}")
            else:
                self.logger.info(f"{self.directionStr} UPLOAD NOT ACK ufEndNotAck {frameJson}")
        elif msgContent['cmdName'] == "log":
            try:
                self.logger.info(f"{self.directionStr} {msgContent['msg']} {frameJson}")
            except Exception as excp:
                self.logger.info(f"{self.directionStr} LOG CONTENT NOT FOUND IN FRAME {frameJson}")
        else:
            self.logger.info(f"{self.directionStr} {frameJson}")
        # print("Unknown cmd msg", frameJson)

def handleUserCommands():
    if os.name == 'nt':
        if msvcrt.kbhit():
            k = msvcrt.getch()
            if k[0] == ord('\x1b'):
                print("Closing")
                return False
    return True

def setupLogToFile(logToFile, logsFolder):
    # Log to file if required
    if logToFile:
        # Check the log folder exists
        if not os.path.exists(logsFolder):
            os.mkdir(logsFolder)
        
        # Form log file name
        logFileName = "LogTest_" + time.strftime("%Y%m%d-%H%M%S") + ".log"
        logFileName = os.path.join(logsFolder, logFileName)
        print("Logging to file " + logFileName)
        logfh = logging.FileHandler(logFileName)
        logfh.setLevel(logging.DEBUG)
        formatter = logging.Formatter('%(levelname)s: %(asctime)s.%(msecs)03d -- %(message)s', datefmt = '%Y-%m-%d %H:%M:%S')
        logfh.setFormatter(formatter)
        # logformatter = logging.Formatter('%(asctime)s: %(name)s %(levelname)s %(message)s')
        # logfh.setFormatter(logformatter)
        logger.addHandler(logfh)
        return logFileName
    return ""

# Start
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
argparser = argparse.ArgumentParser(description='MonitorRxAndTx')
DEFAULT_RX_SERIAL_PORT = "COM9"
DEFAULT_TX_SERIAL_PORT = "COM11"
DEFAULT_SERIAL_BAUD = 460800
DEFAULT_IP_ADDRESS = ""
DEFAULT_LOG_FOLDER = "log"
argparser.add_argument('--rxport', help='Rx Serial Port', default=DEFAULT_RX_SERIAL_PORT)
argparser.add_argument('--txport', help='Tx Serial Port', default=DEFAULT_TX_SERIAL_PORT)
argparser.add_argument('--baud', help='Serial Baud', default=DEFAULT_SERIAL_BAUD)
argparser.add_argument('--logfolder', help='Log folder', default=DEFAULT_LOG_FOLDER)
args = argparser.parse_args()
setupLogToFile(True, args.logfolder)
rxConn = MonitorConn("FromPi")
rxConn.open({"serialPort":args.rxport,"serialBaud":args.baud,"rxBufferSize":1000000})
txConn = MonitorConn("ToPi")
txConn.open({"serialPort":args.txport,"serialBaud":args.baud,"rxBufferSize":1000000})
print("Monitoring started")
while handleUserCommands():
    pass
txConn.close()
rxConn.close()