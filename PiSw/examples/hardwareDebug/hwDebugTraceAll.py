# Script to collect Z80 execution data using a BusRaider
import time, datetime, json, logging, os
from SimpleHDLC import HDLC
from SimpleTCP import SimpleTCP

class Tracer:

    # Start trace
    def start(self):
        # Setup
        self.setup("192.168.86.192", "./examples/logs", "TraceLong.txt")

        # Stop any previous tracer
        self.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")

        # Set serial terminal machine (with emulated 6850) - to avoid conflicts with display updates, etc
        mc = "Serial Terminal ANSI"
        self.sendFrame("SetMachine", b"{\"cmdName\":\"setMcJson\"}\0"+b"{\"name\":\"Serial Terminal ANSI\",\"hw\":[],\"emulate6850\":{}}\0")
        time.sleep(2)

        # Bus reset
        self.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
        time.sleep(1)

        # Start tracer with recording (dense binary records) turned on (logging should be off as this creates verbose text log)
        self.sendFrame("tracerStart", b"{\"cmdName\":\"tracerStart\",\"logging\":0,\"record\":1}\0")

        # Start message
        timeStart = datetime.datetime.now()
        self.logger.info(f"Execution trace started at {str(timeStart)}")

        # Run the trace
        self.awaitingTraceResponse = False
        for i in range(10):
            j = 0
            while self.awaitingTraceResponse and j < 2000:
                time.sleep(0.001)
            if j >= 2000:
                break
            self.sendFrame("tracerGetLong", b"{\"cmdName\":\"tracerGetBin\"}\0")
            self.awaitingTraceResponse = True
            time.sleep(0.01)
        
        # Calculate rate
        timeEnd = datetime.datetime.now()
        elapsed = (timeEnd - timeStart).total_seconds()
        if elapsed > 0 and not self.instrCountAtStart is None:
            instrCount = self.curInstructionCount - self.instrCountAtStart
            self.logger.info(f"{instrCount} instructions in {elapsed:.2f} secs => {instrCount/elapsed:.1f} per second {self.instructionsSkipped} lost")

        # Stop tracing
        self.cleardown()

    def cleardown(self):
        # Clear down
        try:
            self.rdpTCP.stopReader()
        except Exception as excp:
            self.logger.error(f"{excp}")
        if self.dumpTraceFile:
            self.dumpTraceFile.close()

    # Handle received messages
    def frameCallback(self, msgContent, binContent, logger):
        if (msgContent['cmdName'] == "tracerGetBinData") and (not self.dumpTraceFile is None):
            # Traced bus access contents
            dataCount = msgContent['dataLen'] // self.sizeOfTraceBinElem
            traceCount = msgContent['traceCount']
            # Check if any bus accesses lost in communication
            if self.curInstructionCount != traceCount:
                self.instructionsSkipped += traceCount - self.curInstructionCount
            # For calculating rate
            if self.instrCountAtStart is None:
                self.instrCountAtStart = traceCount
            self.curInstructionCount = traceCount + dataCount
            elemPos = 0
            # Extract the data, format and write to file
            for i in range(dataCount):
                dataStr = self.formatTraceBinElem(binContent, elemPos, traceCount)
                elemPos += self.sizeOfTraceBinElem
                traceCount += 1
                self.dumpTraceFile.write(dataStr + "\n")
            self.awaitingTraceResponse = False

    # Format for trace dumping
    def formatTraceBinElem(self, binContent, contentPos, traceCount):
        addr = binContent[contentPos] + (binContent[contentPos+1]*256)
        busData = binContent[contentPos+2]
        retData = binContent[contentPos+3]
        flags = binContent[contentPos+4]
        retData = f"{traceCount:08d} {addr:04x} {busData:02x} {self.formatFlags1(flags)}{self.formatFlags2(flags)}"
        if (flags & 0x80) != 0:
              retData += f"{retData:02x}"
        return retData

    def formatFlags1(self, busFlags):
        return self.flags1[(busFlags >> 3) & 0x03]

    def formatFlags2(self, busFlags):
        return self.flags2[busFlags & 0x07]

    # Send frame to BusRaider
    def sendFrame(self, comment, content):
        frame = bytearray(content)
        self.hdlcHandler.sendFrame(frame)

    # HDLC Frame handler
    def onHDLCBinFrame(self, fr):
        msgContent = {'cmdName':''}
        try:
            # Split string - the format is a JSON string terminated with NULL then
            # a binary section to the end of the buffer
            nullPos = fr.find(b"\0")
            binContent = b""
            if nullPos >= 0:
                jsonStr = fr[:nullPos].decode('utf-8').rstrip('\0')
                msgContent = json.loads(jsonStr)
                binContent = fr[nullPos+1:]
            else:
                jsonStr = fr.decode('utf-8').rstrip('\0')
                msgContent = json.loads(jsonStr)
        except Exception as excp:
            self.logger.error(f"Failed to parse Json from {fr}, {excp}")
        try:
            # All messages should contain cmdName to identify message type
            if 'cmdName' in msgContent:
                if msgContent['cmdName'] == "log":
                    # Check for a log message from Pi
                    try:
                        self.logger.info(f"{msgContent['lev']} : {msgContent['src']} {msgContent['msg']}")
                    except Exception as excp:
                        self.logger.error(f"LOG CONTENT NOT FOUND IN FRAME {fr}, {excp}")
                else:
                    self.frameCallback(msgContent, binContent, self.logger)
        except Exception as excp:
            self.logger.error(f"Failed to extract cmdName {fr}, {excp}")

    # Check for using IP address
    def setup(self, ipAddrOrHostName, fileBase, dumpTraceFileName):

        # Format of trace binary element
        self.sizeOfTraceBinElem = 5
        self.flags1 = ("..",".I","1.","1I")
        self.flags2 = ("...","..R",".W.",".WR","M..","M.R","MW.","MWR")

        # Instruction timing
        self.instrCountAtStart = None
        self.curInstructionCount = 0
        self.instructionsSkipped = 0

        # Logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        consoleLogger = logging.StreamHandler()
        consoleLogger.setLevel(logging.DEBUG)
        consoleLogger.setFormatter(logging.Formatter('%(asctime)s: %(message)s'))
        self.logger.addHandler(consoleLogger)

        # Open trace file
        self.dumpTraceFile = None
        try:
            if dumpTraceFileName is not None and len(dumpTraceFileName) > 0:
                self.dumpTraceFile = open(os.path.join(fileBase, dumpTraceFileName), "w")
        except Exception(excp):
            self.logger.warning("Can't open dump trace file " + os.path.join(fileBase, dumpTraceFile))

        # HDLC port
        self.tcpHdlcPort = 10001

        # Callback to send to TCP
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
        self.hdlcHandler = HDLC(None, sendDataToTCP, None)
        self.hdlcHandler.setCallbacks(None, self.onHDLCBinFrame)

        # Welcome
        self.logger.info(f"UnitTest BusRaider IP {ipAddrOrHostName} port {self.tcpHdlcPort}")

# Create and start the tracer
tracer = Tracer()
tracer.start()
