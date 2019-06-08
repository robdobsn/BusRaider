import time
import json
import logging
import os
from SimpleHDLC import HDLC
from SimpleTCP import SimpleTCP

class Tracer:

    # Start trace
    def start(self):
        self.setup("192.168.86.192", "./examples/logs", "TraceLong.txt", "TraceLongLog.txt", "TraceLong.bin")

        # # Test data - jump to 0000
        # testWriteData = b"\xc3\x00\x00"
        # testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0,
        #              "unknownMsgCount":0, "isrCount":0, "tracerErrCount":0, "clrMaxUs":0, "tracerRespCount":0}

        # Set serial terminal machine - to avoid conflicts with display updates, etc
        mc = "Serial Terminal ANSI"
        self.sendFrame("SetMachine", b"{\"cmdName\":\"setMcJson\"}\0"+b"{\"name\":\"Serial Terminal ANSI\",\"hw\":[],\"emulate6850\":{}}\0")
        time.sleep(2)

        # Processor clock
        # self.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")
        # time.sleep(0.1)

        # # Check hardware list and set 64K RAM
        # self.sendFrame("hwList", b"{\"cmdName\":\"hwList\"}\0")
        # self.sendFrame("hwEnable", b"{\"cmdName\":\"hwEnable\",\"hwName\":\"64KRAM\",\"enable\":1}\0")
        # time.sleep(0.1)

        # Bus reset
        self.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
        # self.sendFrame("reset", b"\"cmdName\":\"ResetTarget\"\0")
        time.sleep(1)

        # # Send program
        # time.sleep(0.1)
        # # commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"lenDec\":3,\"isIo\":0}\0" + testWriteData)
        # # time.sleep(0.1)

        # Stop tracer
        self.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")
        # commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":0,\"lenDec\":3,\"isIo\":0}\0")
        time.sleep(.1)
        # commonTest.sendFrame("valPrime", b"{\"cmdName\":\"tracerPrimeFromMem\"}\0")   
        self.sendFrame("tracerStart", b"{\"cmdName\":\"tracerStart\",\"logging\":0,\"record\":1,\"longForm\":1}\0")
        # commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")   

        # Run
        self.awaitingTraceResponse = False
        for i in range(10000):
            j = 0
            while self.awaitingTraceResponse and j < 1000:
                time.sleep(0.001)
            if j >= 1000:
                break
            self.sendFrame("tracerGetLong", b"{\"cmdName\":\"tracerGetLong\"}\0")
            self.awaitingTraceResponse = True

        # # Get status
        # msgIdx = 0
        # for j in range(testValStatusCount):
        #     commonTest.sendFrame("statusReq", b"{\"cmdName\":\"tracerStatus\",\"msgIdx\":\"" + bytes(str(msgIdx),'utf-8') + b"\"}\0")
        #     msgIdx += 1
        #     commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
        #     time.sleep(0.2)

        # # Send messages to stop
        # commonTest.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")

        # # Breakout early if failing
        # if not testStats["msgRdOk"] or testStats['tracerErrCount'] > 0:
        #     break
        
        # # Bus reset
        # commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
        # time.sleep(1)
        
        # # Wait for test end and cleardown
        # commonTest.cleardown()
        # assert(testStats["msgRdOk"] == True)
        # assert(testStats["msgWrRespCount"] == testRepeatCount)
        # assert(testStats["msgRdRespCount"] == testRepeatCount)
        # assert(testStats["msgWrRespErrCount"] == 0)
        # assert(testStats["msgWrRespErrMissingCount"] == 0)
        # assert(testStats["unknownMsgCount"] == 0)
        # assert(testStats["clockSetOk"] == True)
        # assert(testStats["isrCount"] > 0)
        # assert(testStats["tracerRespCount"] == testRepeatCount * testValStatusCount)
        # assert(testStats["tracerErrCount"] == 0)

    def frameCallback(self, msgContent, logger):
        if msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % testWriteData[i]) for i in range(len(testWriteData)))
            respOk = requiredResp == msgContent['data']
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "WrResp":
            try:
                if msgContent['err'] != 'ok':
                    logger.error(f"WrResp err not ok {msgContent}")
            except:
                logger.error(f"WrResp doesn't contain err {msgContent}")
        elif msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "clockHzSetResp":
            pass
        elif msgContent['cmdName'] == "setMcJsonResp":
            pass
        elif msgContent['cmdName'] == "tracerPrimeFromMemResp" or \
                msgContent['cmdName'] == "tracerStopResp" or \
                msgContent['cmdName'] == "tracerStartResp":
            pass
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'] == "tracerStatusResp":
            logger.info(f"tracerStatus ISRCount {msgContent['isrCount']} errors {msgContent['errors']}")
        elif msgContent['cmdName'] == "busStatusResp":
            pass
        elif msgContent['cmdName'] == "hwListResp" or msgContent['cmdName'] == "hwEnableResp":
            pass
        elif msgContent['cmdName'] == "tracerGetLongResp":
            if not self.dumpTraceFile is None:
                self.dumpTraceFile.write(str(msgContent) + "\n")
            self.awaitingTraceResponse = False
        else:
            logger.info(f"Unknown message {msgContent}")

    def sendFrame(self, comment, content):
        frame = bytearray(content)
        # for b in frame:
        #     print(hex(b)+" ",end='')
        # print()
        # try:
        self.hdlcHandler.sendFrame(frame)
        if self.logSends and len(comment) > 0: 
            self.logger.debug(f"Sent {comment}")
        # except Exception as excp:
        #     self.logger.error(f"Failed to send frame {comment}, {excp}")

    # HDLC Frame handler
    def onHDLCFrame(self, fr):
        if fr[0] == "{":
            msgContent = {'cmdName':''}
            # try:
            msgContent = json.loads(fr)
            # except Exception as excp:
            #     self.logger.error(f"Failed to parse Json from {fr}, {excp}")
            # try:
            if 'cmdName' in msgContent:
                if msgContent['cmdName'] == "log":
                    try:
                        self.logger.info(f"{msgContent['lev']} : {msgContent['src']} {msgContent['msg']}")
                    except Exception as excp:
                        self.logger.error(f"LOG CONTENT NOT FOUND IN FRAME {fr}, {excp}")
                else:
                    self.frameCallback(msgContent, self.logger)
        else:
            # Not JSON data
            pass        

        # except Exception as excp:
        #     self.logger.error(f"Failed to extract cmdName {fr}, {excp}")

    # Check for using IP address
    def setup(self, ipAddrOrHostName, fileBase, dumpTraceFileName, dumpBinFileName, dumpTextFileName, logSends=False):

        # Logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        consoleLogger = logging.StreamHandler()
        consoleLogger.setLevel(logging.DEBUG)
        consoleLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
        self.logger.addHandler(consoleLogger)
        if dumpTextFileName is not None and len(dumpTextFileName) != 0:
            self.fileLogger = logging.FileHandler(os.path.join(fileBase, dumpTextFileName))
            self.fileLogger.setLevel(logging.DEBUG)
            self.fileLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
            self.logger.addHandler(self.fileLogger)
        self.logSends = logSends

        # Open trace file
        self.dumpTraceFile = None
        try:
            if dumpTraceFileName is not None and len(dumpTraceFileName) > 0:
                self.dumpTraceFile = open(os.path.join(fileBase, dumpTraceFileName), "w")
        except Exception(excp):
            self.logger.warning("Can't open dump trace file " + os.path.join(fileBase, dumpTraceFile))

        # Open dump file
        try:
            self.dumpBinFile = None
            if dumpBinFileName is not None and len(dumpBinFileName) > 0:
                self.dumpBinFile = open(os.path.join(fileBase, dumpBinFileName), "wb")
        except Exception(excp):
            self.logger.warning("Can't open dump binary file " + os.path.join(fileBase, dumpBinFileName))

        self.tcpHdlcPort = 10001

        def sendDataToTCP(dataToSend):
            self.rdpTCP.sendFrame(dataToSend)

        # Frame handler
        def onTCPFrame(fr):
            # Send to HDLC
            self.hdlcHandler.processBytes(fr)

        # TCP Reader
        self.rdpTCP = SimpleTCP(ipAddrOrHostName, self.tcpHdlcPort)
        self.rdpTCP.startReader(onTCPFrame)

        # Setup HDLC
        self.hdlcHandler = HDLC(None, sendDataToTCP, self.dumpBinFile)
        self.hdlcHandler.setCallbacks(self.onHDLCFrame)

        self.logger.info(f"UnitTest BusRaider IP {ipAddrOrHostName} port {self.tcpHdlcPort}")

tracer = Tracer()
tracer.start()
