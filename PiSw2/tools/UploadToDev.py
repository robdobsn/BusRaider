import serial
import sys
import time
import math
import logging
import json
import argparse
from LikeCommsSerial import LikeCommsSerial
from LikeHDLC import calcCRC

# This is a program for uploading firmware to the I2CEmu
# It requires a serial connection to the I2CEmulator and running the I2CEmulator s/w

class I2CEmulatorConn:
    uploadAcked = False

    def __init__(self):
        self.commsHandler = LikeCommsSerial()

    def open(self, openParams):
        self.commsHandler.setRxFrameCB(self._onRxFrameCB)
        openOk = self.commsHandler.open(openParams)

        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        self.lastAddr = 10000

    def close(self):
        self.commsHandler.close()

    def send(self, data):
        self.commsHandler.send(data)

    def uploadStart(self):
        self.uploadAcked = False

    # Received frame handler
    def _onRxFrameCB(self, frame):
        # print("Frame RX")
        msgContent = {'cmdName':''}
        termPos = frame.find(0)
        frameJson = frame
        if termPos >= 0:
            frameJson = frame[0:termPos]
        try:
            msgContent = json.loads(frameJson)
        except Exception as excp:
            print("Failed to parse Json from", frameJson, excp)
        if msgContent['cmdName'] == "ufEndAck":
            print("Upload Acknowledged")
            self.uploadAcked = True
        elif msgContent['cmdName'] == "ufEndNotAck":
            print("Upload FAILED")
        elif msgContent['cmdName'] == "log":
            try:
                print(msgContent['lev'] + ":", msgContent['src'], msgContent['msg'])
            except Exception as excp:
                print("LOG CONTENT NOT FOUND IN FRAME", frameJson, excp)
        elif msgContent['cmdName'] != "busStats" and msgContent['cmdName'] != "addrStats":
            print("Unknown cmd msg", frameJson)

def sendFile(fileName):
    # Send new firmware
    with open(fileName, "rb") as f:

        # Read firmware
        binaryImage = f.read()
        binaryImageLen = len(binaryImage)
        fileCRC = calcCRC(binaryImage)
        print(f"File {fileName} is {binaryImageLen} bytes long crc 0x{fileCRC[0]*256+fileCRC[1]:04x}")

        # Frames follow the approach used in the web interface start, block..., end
        startFrame = bytearray(b"{\"cmdName\":\"ufStart\",\"fileName\":\"kernel.img\",\"fileType\":\"firmware\",\"fileLen\":\"" + \
                        bytes(str(binaryImageLen),"ascii") + b"\"}\0")
        emulatorConn.send(startFrame)

        # Split the file into blocks
        blockMaxSize = 1024
        numBlocks = binaryImageLen//blockMaxSize + (0 if (binaryImageLen % blockMaxSize == 0) else 1)
        for i in range(numBlocks):
            blockStart = i*blockMaxSize
            blockToSend = binaryImage[blockStart:blockStart+blockMaxSize]
            dataFrame = bytearray(b"{\"cmdName\":\"ufBlock\",\"index\":\"" + bytes(str(blockStart),"ascii") + b"\"}\0") + blockToSend
            emulatorConn.send(dataFrame)

        # End frame            
        endFrame = bytearray(b"{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + bytes(str(numBlocks),"ascii") + b"\"}\0")
        emulatorConn.send(endFrame)

        # Check for end frame acknowledged
        prevTime = time.time()
        while True:
            if emulatorConn.uploadAcked:
                break
            if time.time() - prevTime > 2:
                break

argparser = argparse.ArgumentParser(description='UploadToI2CEmulator')
DEFAULT_SERIAL_PORT = "COM7"
DEFAULT_SERIAL_BAUD = 921600
argparser.add_argument('fileName', help='File Name')
argparser.add_argument('--port', help='Serial Port', default=DEFAULT_SERIAL_PORT)
argparser.add_argument('--baud', help='Serial Baud', default=DEFAULT_SERIAL_BAUD)
args = argparser.parse_args()
emulatorConn = I2CEmulatorConn()
emulatorConn.open({"serialPort":args.port,"serialBaud":args.baud})
emulatorConn.uploadStart()
print("Upload Started")
sendFile(args.fileName)
emulatorConn.close()
