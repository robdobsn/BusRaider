import serial
import sys
import time
import math
from LikeCommsSerial import LikeCommsSerial
from SimpleTCP import SimpleTCP
import logging
import json
import os
import websocket

# def commandLineTestStart():
#     # Get args passed from build/test script
#     try:
#         testname = ' '.join(sys.argv[1:2])
#         testLoops = int(' '.join(sys.argv[2:3]))
#         useIP = ' '.join(sys.argv[3:4])
#         serialOrIp = ' '.join(sys.argv[4:5])
#         flashbaud = int(' '.join(sys.argv[5:6]))
#         baseFolder = ' '.join(sys.argv[6:7])
#         dumpBinFileName = ' '.join(sys.argv[7:8])
#         dumpTextFileName = ' '.join(sys.argv[8:])
#     except Exception:
#         print("Usage: python UnitTest.py TESTNAME LOOPS METHOD PORTORIP BAUDRATE BASEFOLDER DUMPBIN DUMPTXT")
#         print("Where: METHOD = ip for using IP address, ser for serial")
#         print("       TYPE = ihex for Intel Hex, hdlc for hdlc")
#         print("       NAME = serial port or IP address, etc")
#         print("       DUMPBIN & DUMPTXT = log files binary received data and output text")
#         exit(1)
#     commonTestEntry(testname, testLoops, useIP, serialOrIp, flashbaud, baseFolder, dumpBinFileName, dumpTextFileName)

class CommonTest:

    def __init__(self, testName, testBaseFolder, useIP, useWsPath, serialPort, serialBaud, ipAddrOrHostName, dumpBinFileName, dumpTextFileName, frameCallback, logSends=False):
        
        self.testName = testName
        self.useIP = useIP
        self.useWsUrl = useWsUrl
        # Logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        consoleLogger = logging.StreamHandler()
        consoleLogger.setLevel(logging.DEBUG)
        consoleLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
        self.logger.addHandler(consoleLogger)
        if dumpTextFileName is not None and len(dumpTextFileName) != 0:
            logsFileName = os.path.join(testBaseFolder, "logs/", dumpTextFileName)
            print(logsFileName)
            fileLogger = logging.FileHandler(logsFileName)
            fileLogger.setLevel(logging.DEBUG)
            fileLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
            self.logger.addHandler(fileLogger)
        self.logSends = logSends
        self.respAwaited = None
        self.respGot = False

        # Open dump file
        try:
            self.dumpBinFile = None
            if dumpBinFileName is not None and len(dumpBinFileName) > 0:
                self.dumpBinFile = open(os.path.join(testBaseFolder, "logs/", dumpBinFileName), "wb")
        except Exception as excp:
            self.logger.warning("Can't open dump binary file " + os.path.join("./test/logs/", dumpBinFileName))

        # Received Frame handler
        def onRxFrame(fr):
            msgContent = {'cmdName':''}
            try:
                frStr = fr.decode().rstrip('\x00')
                msgContent = json.loads(frStr)
                print(frStr)
            except Exception as excp:
                self.logger.error(f"Failed to parse Json from {frStr}, {excp}")
            try:
                # print("rxmsg", msgContent['cmdName'])
                if msgContent['cmdName'] == "log":
                    try:
                        self.logger.info(f"{msgContent['lev']} : {msgContent['src']} {msgContent['msg']}")
                    except Exception as excp:
                        self.logger.error(f"LOG CONTENT NOT FOUND IN FRAME {fr}, {excp}")
                else:
                    # Check for awaited response
                    if "cmdName" in msgContent and self.respAwaited is not None:
                        if msgContent["cmdName"] == self.respAwaited:
                            self.respGot = True
                    # Callback
                    try:
                        frameCallback(msgContent, self.logger)
                    except Exception as excp:
                        self.logger.error(f"Error in frameCallback with content {msgContent}, {excp}")
            except Exception as excp:
                self.logger.error(f"Failed to extract cmdName {fr}, {excp}")

        # Check for using IP address
        if useIP:
            if useWsPath != "":
                
            else:
                self.tcpHdlcPort = 10001

                # Frame handler
                def onTCPFrame(fr):
                    # Send to HDLC
                    for byt in fr:
                        self.commsSerial._hdlc.handleRxByte(byt)

                # TCP Reader
                self.rdpTCP = SimpleTCP(ipAddrOrHostName, self.tcpHdlcPort)
                self.rdpTCP.startReader(onTCPFrame)

                # Setup HDLC
                self.commsSerial = LikeCommsSerial()
                self.commsSerial.setRxFrameCB(onRxFrame)

                self.logger.info(f"UnitTest BusRaider IP {ipAddrOrHostName} port {self.tcpHdlcPort}")
                
        else:
            # Open the serial connection to the BusRaider
            self.commsSerial = LikeCommsSerial()
            self.commsSerial.setRxFrameCB(onRxFrame)
            self.commsSerial.open(
                {
                    "serialPort": serialPort,
                    "serialBaud": serialBaud,
                    "rxBufferSize": 20000
                })

            self.logger.info(f"UnitTest BusRaider port {serialPort} baudrate {serialBaud}")
            sys.stdout.flush()

    def sendFrame(self, comment, content, respExpected = None):
        self.respAwaited = respExpected
        self.respGot = False
        frame = bytearray(content)
        # for b in frame:
        #     print(hex(b)+" ",end='')
        # print()
        if self.useIP:
            frameEncoded = self.commsSerial._hdlc._encode(frame)
            self.rdpTCP.sendFrame(frameEncoded)
        else:
            try:
                self.commsSerial.send(frame)
                if self.logSends and len(comment) > 0: 
                    self.logger.debug(f"Sent {comment}")
            except Exception as excp:
                self.logger.error(f"Failed to send frame {comment}, {excp}")

    def awaitResponse(self, maxWaitMs):
        if self.respAwaited is not None:
            # print("Awaiting response", self.respAwaited, "MaxWaitMs", maxWaitMs)
            waitMsCount = 0
            while(waitMsCount < maxWaitMs):
                if self.respGot:
                    # print("Awaiting response GOT", self.respAwaited)
                    return True
                time.sleep(0.001)
                waitMsCount += 1
        return False

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
                self.commsSerial.close()
            except Exception as excp:
                self.logger.error(f"Serial port? {excp}")

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
