import time
import logging
import json
import os
import argparse
import configparser
from typing import Union
from .martypy import RICCommsWiFi, RICInterface, RICProtocols

class CommonTest:

    def getConfig(self):
        # Handle arguments
        argparser = argparse.ArgumentParser(description='BusRaider Tests')
        argparser.add_argument('--configfile', help='Config ini file', default='devconfig.ini')
        argparser.add_argument('--configsection', help='Config file section', default='DEFAULT')
        args, _ = argparser.parse_known_args()

        # Handle config
        config = configparser.ConfigParser()
        config.read(args.configfile)

        # Extract config
        fullConf = config[args.configsection]
        logConfig = {"logfolder": fullConf.get("logfolder", "./log")}
        ifConfig = {
            "ifType": fullConf.get("ifType", "websocket"),
            "wsURL": fullConf.get("wsURL", ""),
            "ipAddr": fullConf.get("ipAddr", ""),
            "serialPort": fullConf.get("serialPort", ""),
            "serialBaud": fullConf.get("serialBaud", ""),
        }
        testConfig = {
        }
        return logConfig, ifConfig, testConfig

    def __init__(self, testName, testBaseFolder, dumpBinFileName, dumpTextFileName, frameCallback, logSends=False):
        
        self.logConfig, self.ifConfig, self.testConfig = self.getConfig()
        self.testName = testName
        self.frameCallback = frameCallback
        self.useIP = self.ifConfig.get("ifType","") == "websocket" or self.ifConfig.get("ifType","") == "TCP"
        self.ipAddr = self.ifConfig.get("ipAddr", "")
        self.wsURL = self.ifConfig.get("wsURL","")
        self.serialPort = self.ifConfig.get("serialPort","")
        self.serialBaud = self.ifConfig.get("serialBaud",921600)
        self.logBaseFolder = self.logConfig.get("logfolder","")
        # Logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        consoleLogger = logging.StreamHandler()
        consoleLogger.setLevel(logging.DEBUG)
        consoleLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
        self.logger.addHandler(consoleLogger)
        if dumpTextFileName is not None and len(dumpTextFileName) != 0:
            logsFileName = os.path.join(self.logBaseFolder, "testlogs/", dumpTextFileName)
            print(logsFileName)
            fileLogger = logging.FileHandler(logsFileName)
            fileLogger.setLevel(logging.DEBUG)
            fileLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
            self.logger.addHandler(fileLogger)
        self.logSends = logSends
        self.respAwaited = None
        self.respGot = False
        # self.ricProtocols = RICProtocols()

        # Open dump file
        self.dumpBinFile = None
        dumpBinPath = os.path.join(self.logBaseFolder, "testlogs/", dumpBinFileName)
        try:
            if dumpBinFileName is not None and len(dumpBinFileName) > 0:
                self.dumpBinFile = open(dumpBinPath, "wb")
        except Exception as excp:
            self.logger.warning("Can't open dump binary file " + dumpBinPath + " excp " + str(excp))

        #         ipAddr = self.ifConfig["ipAddr"]
        #         self.tcpHdlcPort = 10001

        #         # Frame handler
        #         def onTCPFrame(fr):
        #             # Send to HDLC
        #             for byt in fr:
        #                 self.commsSerial._hdlc.handleRxByte(byt)

        #         # TCP Reader
        #         self.rdpTCP = SimpleTCP(ipAddr, self.tcpHdlcPort)
        #         self.rdpTCP.startReader(onTCPFrame)

        #         # Setup HDLC
        #         self.commsSerial = LikeCommsSerial()
        #         self.commsSerial.setRxFrameCB(self.onRxFrame)

        #         self.logger.info(f"UnitTest BusRaider IP {ipAddr} port {self.tcpHdlcPort}")
        ricComms = RICCommsWiFi()
        self.busRaider = RICInterface.RICInterface(ricComms)
        self.busRaider.open({"ipAddrOrHostname":self.ipAddr})
        self.busRaider.setDecodedMsgCB(self.decodedMsg)
        self.busRaider.setLogLineCB(self.logLine)

    def logLine(msg):
        print(msg)
        
    def decodedMsg(self, msg: RICProtocols.DecodedMsg, ricIf):
        self.onRxFrame(msg.payload)
        print(f"WS Frame len {len(msg.payload)}")

    # Received Frame handler
    def onRxFrame(self, fr):
        msgContent = {'cmdName':''}
        frStr = ""
        try:
            if not isinstance(fr, str):
                frStr = fr.decode().rstrip('\x00')
            else:
                frStr = fr.rstrip('\x00')
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
                    self.frameCallback(msgContent, self.logger)
                except Exception as excp:
                    self.logger.error(f"Error in frameCallback with content {msgContent}, {excp}")
        except Exception as excp:
            self.logger.error(f"Failed to extract cmdName {fr}, {excp}")

    def sendFrame(self, comment: str, content: Union[str,bytes], payload: bytes = None, respExpected: bool = None):
        self.respAwaited = respExpected
        self.respGot = False
        if len(comment) > 0:
            self.logger.info(comment)
        self.busRaider.sendRICRESTCmdFrame(content, payload)

    def sendFrameSync(self, comment: str, content: Union[str,bytes], payload: bytes = None):
        self.respGot = False
        if len(comment) > 0:
            self.logger.info(comment)
        return self.busRaider.sendRICRESTCmdFrameSync(content, payload)

    def awaitResponse(self, maxWaitMs):
        if self.respAwaited is not None:
            print("Awaiting response", self.respAwaited, "MaxWaitMs", maxWaitMs)
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
        self.busRaider.close()

        # if self.useIP:
        #     self.logger.info("UnitTest closing socket")
        #     if self.wsURL != "":
        #         try:
        #             self.ws.stopReader()
        #         except Exception as excp:
        #             self.logger.error(f"{excp}")
        #     else:
        #         try:
        #             self.rdpTCP.stopReader()
        #         except Exception as excp:
        #             self.logger.error(f"{excp}")
        # else:
        #     try:
        #         self.commsSerial.close()
        #     except Exception as excp:
        #         self.logger.error(f"Serial port? {excp}")

        # Results
        # testOk = (errCount == 0 and loopCount != 0)
        # testResultStr = "OK" if testOk else "FAILED"
        # self.logger.info(f"\n---------------------------------------\n")
        # self.logger.info(f"Test results: {testResultStr} loops {loopCount} errors {errCount} isrs {isrCount}\n")
        # self.logger.info(f"---------------------------------------\n")

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
                self.logger.error(f"Unable to load file {os.path.join(fileFolder, fileName)}")
                return None
        return inData

    def formFileFrame(self, fileFolder, fileName):
        fileFrame = bytearray(b"{\"cmdName\":\"FileTarget\",\"fileName\":\"") + bytearray(fileName, "utf-8") + b"\"}\0"
        fileData = self.getFileData(fileFolder, fileName)
        if fileData is None:
            return None
        fileFrame += fileData
        return fileFrame
