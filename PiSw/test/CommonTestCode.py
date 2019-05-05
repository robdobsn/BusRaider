import serial
import sys
import time
import math
from SimpleHDLC import HDLC
from SimpleTCP import SimpleTCP
import logging
import json
import os
import asyncio

def commandLineTestStart():
    # Get args passed from build/test script
    try:
        testname = ' '.join(sys.argv[1:2])
        testLoops = int(' '.join(sys.argv[2:3]))
        useIP = ' '.join(sys.argv[3:4])
        serialOrIp = ' '.join(sys.argv[4:5])
        flashbaud = int(' '.join(sys.argv[5:6]))
        baseFolder = ' '.join(sys.argv[6:7])
        dumpBinFileName = ' '.join(sys.argv[7:8])
        dumpTextFileName = ' '.join(sys.argv[8:])
    except Exception:
        print("Usage: python UnitTest.py TESTNAME LOOPS METHOD PORTORIP BAUDRATE BASEFOLDER DUMPBIN DUMPTXT")
        print("Where: METHOD = ip for using IP address, ser for serial")
        print("       TYPE = ihex for Intel Hex, hdlc for hdlc")
        print("       NAME = serial port or IP address, etc")
        print("       DUMPBIN & DUMPTXT = log files binary received data and output text")
        exit(1)
    commonTestEntry(testname, testLoops, useIP, serialOrIp, flashbaud, baseFolder, dumpBinFileName, dumpTextFileName)

class CommonTest:

    def setup(self, useIP, serialPort, serialBaud, ipAddrOrHostName, dumpBinFileName, dumpTextFileName, frameCallback, logSends=False):
        
        self.useIP = useIP
        # Logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        consoleLogger = logging.StreamHandler()
        consoleLogger.setLevel(logging.DEBUG)
        consoleLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
        self.logger.addHandler(consoleLogger)
        if dumpTextFileName is not None and len(dumpTextFileName) != 0:
            fileLogger = logging.FileHandler(os.path.join("./test/logs/", dumpTextFileName))
            fileLogger.setLevel(logging.DEBUG)
            fileLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
            self.logger.addHandler(fileLogger)
        self.logSends = logSends

        # Open dump file
        try:
            self.dumpBinFile = None
            if dumpBinFileName is not None and len(dumpBinFileName) > 0:
                self.dumpBinFile = open(os.path.join("./test/logs/", dumpBinFileName), "wb")
        except Exception(excp):
            self.logger.warning("Can't open dump binary file " + os.path.join("./test/logs/", dumpBinFileName))

        # HDLC Frame handler
        def onHDLCFrame(fr):
            msgContent = {'cmdName':''}
            try:
                msgContent = json.loads(fr)
                if msgContent['cmdName'] == "log":
                    try:
                        self.logger.info(f"{msgContent['lev']} : {msgContent['src']} {msgContent['msg']}")
                    except Exception as excp:
                        self.logger.error(f"LOG CONTENT NOT FOUND IN FRAME {fr}, {excp}")
                else:
                    frameCallback(msgContent)
            except Exception as excp:
                self.logger.error(f"Failed to parse Json from {fr}, {excp}")

        # Check for using IP address
        if useIP:
            self.rdpPort = 10000

            def sendDataToTCP(dataToSend):
                self.rdpTCP.sendFrame(dataToSend)

            # Frame handler
            def onTCPFrame(fr):
                # Send to HDLC
                self.hdlcHandler.processBytes(fr)

            # TCP Reader
            self.rdpTCP = SimpleTCP(ipAddrOrHostName, self.rdpPort)
            self.rdpTCP.startReader(onTCPFrame)

            # Setup HDLC
            self.hdlcHandler = HDLC(None, sendDataToTCP, self.dumpBinFile)
            self.hdlcHandler.setCallbacks(onHDLCFrame)

        else:
            # Open the serial connection to the BusRaider
            try:
                self.ser = serial.Serial(
                    port=serialPort,
                    baudrate=serialBaud,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    bytesize=serial.EIGHTBITS
                )
            except Exception:
                self.logger.error("Serial Port " + serialPort + " busy or non-existent!")
                return False
            try:
                self.ser.set_buffer_size(20000, None)
            except Exception:
                self.logger.error("Failed to set serial buffer size")

            self.logger.info(f"UnitTest BusRaider port {serialPort} baudrate {serialBaud}")
            sys.stdout.flush()

            # Setup HDLC sender
            self.hdlcHandler = HDLC(self.ser, self.dumpBinFile)
            self.hdlcHandler.startReader(onHDLCFrame)

    def sendFrame(self, comment, content):
        frame = bytearray(content)
        # for b in frame:
        #     print(hex(b)+" ",end='')
        # print()
        try:
            self.hdlcHandler.sendFrame(frame)
            if self.logSends and len(comment) > 0: 
                self.logger.debug(f"Sent {comment}")
        except Exception as excp:
            self.logger.error(f"Failed to send frame {comment}, {excp}")

    def cleardown(self):
        # Remain running for a little while to hoover up diagnostics, etc
        prevTime = time.time()
        while True:
            if time.time() - prevTime > 1:
                break
        # Close down
        if self.dumpBinFile is not None:
            self.dumpBinFile.close()
        if self.useIP:
            self.logger.info("UnitTest closing socket")
            try:
                self.rdpTCP.stopReader()
            except Exception as excp:
                self.logger.error(f"{excp}")
        else:
            try:
                self.hdlcHandler.stopReader()
            except Exception as excp:
                self.logger.error(f"Serial port? {excp}")

            self.ser.close()

        # Results
        # testOk = (errCount == 0 and loopCount != 0)
        # testResultStr = "OK" if testOk else "FAILED"
        # logger.info(f"\n---------------------------------------\n")
        # logger.info(f"Test results: {testResultStr} loops {loopCount} errors {errCount} isrs {isrCount}\n")
        # logger.info(f"---------------------------------------\n")

    def getFileData(self, fileFolder, fileName):
        inData = []
        try:
            with open(os.path.join(fileFolder, fileName), "rb") as inFile:
                inData = inFile.read()
        except:
            pass
        if len(inData) == 0:
            try:
                with open(os.path.join(fileFolder, fileName), "rb") as inFile:
                    inData = inFile.read()
            except:
                logger.error("Unable to load file ", os.path.join(fileFolder, fileName))
                return None
        return inData

    def formFileFrame(self, fileFolder, fileName):
        fileFrame = bytearray(b"{\"cmdName\":\"FileTarget\",\"fileName\":\"") + bytearray(fileName, "utf-8") + b"\"}\0"
        fileData = self.getFileData(fileFolder, fileName)
        if fileData is None:
            return None
        fileFrame += fileData
        return fileFrame
