#
# Serial Monitor Utility
#
# Rob Dobson 2020-21
#

import threading
import time
import sys
import os
import logging
import serial
from serial.serialutil import SerialException
import argparse
import KeyboardUtils

class LogHelper:

    def __init__(self, logToFile, logsFolder):
        self._logger = logging.getLogger(__name__)
        self._logger.setLevel(logging.DEBUG)
        self._curLine = ""

        # Log to file if required
        if logToFile:
            # Check the log folder exists
            if not os.path.exists(logsFolder):
                os.mkdir(logsFolder)
            
            # Form log file name
            logFileName = time.strftime("%Y%m%d-%H%M%S") + ".log"
            logFileName = os.path.join(logsFolder, logFileName)
            logfh = logging.FileHandler(logFileName)
            logfh.setLevel(logging.DEBUG)
            self._logger.addHandler(logfh)

    def getLogger(self):
        return self._logger

    def info(self, logMsg):
        self._logger.info(logMsg)

class SerialIO:

    def __init__(self, dataCallback, logHelper):
        self._running = False
        self._serialThread = None
        self._dataCallback = dataCallback
        self._isTestMode = False
        self._logHelper = logHelper
        self._serial = None

    def open(self, serialPort, serialBaud, testMode):
        self._serialPort = serialPort
        self._serialBaud = serialBaud
        self._isTestMode = testMode
        if self._isTestMode:
            self._serialThreadStart(self._testLoop)
        else:
            self._openSerial()

    def close(self):
        try:
            self._serialThreadStop()
        except Exception as excp:
            print("Error stopping and closing port", excp)

    def send(self, toSend):
        if self._isTestMode:
            print("Test sending", toSend)
        else:
            asciiOut = (toSend + '\n').encode("ascii")
            # print(asciiOut)
            self._serial.write(asciiOut)

    def _serialThreadStart(self, threadFn):
        if self._running:
            raise RuntimeError("reader already running")
        self._serialThread = threading.Thread(target=threadFn)
        self._serialThread.setDaemon(True)
        self._running = True
        self._serialThread.start()

    def _serialThreadStop(self):
        self._running = False
        try:
            self._serialThread.join()
        except:
            pass
        self._serialThread = None

    def _serialRxThreadFn(self):
        while self._running:
            try:
                numWaiting = self._serial.in_waiting
                if numWaiting < 1:
                    time.sleep(0.001)
                    continue
                rxBytes = self._serial.read(numWaiting)
                self._dataCallback(rxBytes)
            except SerialException as excp:
                print("Error in serial " + str(excp))
                try:
                    # Try to reopen port
                    time.sleep(2)
                    self.close()
                    self._openSerial()
                except Exception as excp2:
                    print("Error reopening serial " + str(excp2))
            except Exception as excp3:
                print("Non-serial error " + str(excp3))

    def _openSerial(self):
        # Open the serial connection
        try:
            self._serial = serial.Serial(
                port=self._serialPort,
                baudrate=self._serialBaud,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
        except Exception as excp:
            print("Serial Port " + str(self._serialPort) + " " + str(excp))
            return False
        try:
            self._serial.set_buffer_size(20000, None)
        except Exception:
            print("Failed to set serial buffer size")

        print(f"SerialMonitor port {self._serialPort} baudrate {self._serialBaud}")

        if not self._running:
            # Start serial reader
            self._serialThreadStart(self._serialRxThreadFn)

    def _testLoop(self):
        while self._running:
            self._dataCallback(b"this is a test\r\n")
            time.sleep(1)

class InputLine:

    RET_OK = 0
    RET_EXIT = 1
    RET_ENTER = 2

    def __init__(self):
        self._inLineBuf = ""
        self._inLinePos = 0
        self._history = []
        self._historyCurPos = 0

    def isActive(self):
         return len(self._inLineBuf) != 0

    def show(self):
        sys.stdout.write(Terminal.LINE_CLEAR_ALL + Terminal.CURSOR_LINE_START + self._inLineBuf)
        if len(self._inLineBuf) != self._inLinePos:
            Terminal.cursorLeft(len(self._inLineBuf) - self._inLinePos)
        sys.stdout.flush()

    def getLine(self):
        return self._inLineBuf

    def clear(self):
        self._inLineBuf = ""
        self._inLinePos = 0
        self.show()

    def handleKey(self, keyName):
        if len(keyName) == 1:
            # Printable character
            # prevCmdLen = len(self._inLineBuf)
            if len(self._inLineBuf) != self._inLinePos:
                self._inLineBuf = self._inLineBuf[:self._inLinePos] + keyName + self._inLineBuf[self._inLinePos:]
            else:
                self._inLineBuf += keyName
            self._inLinePos += 1
            self.show()
            # print(self._inLineBuf)
        elif keyName == 'CTRL-C' or keyName == 'ESC':
            return self.RET_EXIT
        elif keyName == 'BACKSPACE':
            # prevCmdLen = len(self._inLineBuf)
            if self._inLinePos != 0 and len(self._inLineBuf) != 0:
                self._inLineBuf = self._inLineBuf[:self._inLinePos-1] + self._inLineBuf[self._inLinePos:]
                self._inLinePos -= 1
                self.show()
        elif keyName == 'DEL':
            if self._inLinePos != len(self._inLineBuf) and len(self._inLineBuf) != 0:
                self._inLineBuf = self._inLineBuf[:self._inLinePos] + self._inLineBuf[self._inLinePos+1:]
                self.show()
        elif keyName == 'LEFT':
            if self._inLinePos > 0:
                self._inLinePos -= 1
                self.show()
        elif keyName == 'RIGHT':
            if self._inLinePos < len(self._inLineBuf):
                self._inLinePos += 1
                self.show()
        elif keyName == 'HOME':
            self._inLinePos = 0
            self.show()
        elif keyName == 'END':
            self._inLinePos = len(self._inLineBuf)
            self.show()
        elif keyName == 'UP':
            if len(self._history) > 0:
                self._historyCurPos = (len(self._history) + self._historyCurPos - 1) % len(self._history)
                self._inLineBuf = self._history[self._historyCurPos]
                self._inLinePos = len(self._inLineBuf)
            self.show()
        elif keyName == 'DOWN':
            if len(self._history) > 0:
                if (self._historyCurPos == len(self._history)):
                    self._historyCurPos = 0
                else:
                    self._historyCurPos = (len(self._history) + self._historyCurPos + 1) % len(self._history)
                self._inLineBuf = self._history[self._historyCurPos]
                self._inLinePos = len(self._inLineBuf)
            self.show()
        elif keyName == 'ENTER':
            if len(self._inLineBuf) != 0:
                if len(self._history) == 0 or self._history[-1] != self._inLineBuf:
                    self._history.append(self._inLineBuf)
            self._historyCurPos = len(self._history)
            return self.RET_ENTER
        # else:
            # sys.stdout.write(Terminal.CURSOR_DOWN)
            # print(f"Special {keyName}")
        return self.RET_OK
        
class Terminal:

    CURSOR_UP = "\u001b[1A"
    CURSOR_DOWN = "\u001b[1B"
    CURSOR_RIGHT = "\u001b[1C"
    CURSOR_LEFT = "\u001b[1D"
    CURSOR_LEFT_N = "\u001b[{}D"
    CURSOR_LINE_START = "\u001b[1000D"
    CURSOR_BOTTOM_LEFT = "\u001b[1000;1H"
    CURSOR_SAVE = "\u001b[s"
    CURSOR_RESTORE = "\u001b[u"
    LINE_CLEAR_TO_END = "\u001b[0K"
    LINE_CLEAR_TO_START = "\u001b[1K"
    LINE_CLEAR_ALL = "\u001b[2K"

    def __init__(self, logHelper, stripTermColours):
        self._logHelper = logHelper
        self._stripTermColours = stripTermColours
        self._inputLine = InputLine()
        self._outLineBuf = ""
        self._serialInEsc = False
        self._serialLastChCode = 0
        self._running = True
        self._keyboardUtils = KeyboardUtils.KeyboardUtils(self._handleKey)

    def close(self):
        if (len(self._outLineBuf)) > 0:
            self._sendToLog(self._outLineBuf)
        self._running = False
        self._keyboardUtils.close()

    def setOutput(self, outputConn):
        self._outputConn = outputConn

    def isOpen(self):
        return self._running

    @classmethod
    def cursorLeft(cls, moves):
        oStr = cls.CURSOR_LEFT_N.format(moves)
        sys.stdout.write(oStr)

    def handleSerialData(self, serialData: bytes):
        termOutStr = ""
        for chCode in serialData:
            termAmended = False
            termStr = ""
            if self._serialInEsc:
                if chCode == ord('m'): # assume escaped output is only colour info (which ends with 'm')
                    self._serialInEsc = False
                if self._stripTermColours:
                    termAmended = True
                    termStr = ""
            elif chCode == 27:
                self._serialInEsc = True
                if self._stripTermColours:
                    termAmended = True
                    termStr = ""
            elif chCode == 10:
                if self._serialLastChCode != 13:
                    self._sendToLog(self._outLineBuf)
                    self._outLineBuf = ""
            elif chCode == 13:
                if self._serialLastChCode != 10:
                    self._sendToLog(self._outLineBuf)
                    self._outLineBuf = ""
            elif chCode == 9 or (chCode >= 32 and chCode < 127):
                self._outLineBuf += chr(chCode)
            else:
                termStr = f"/x{chCode:02x}"
                self._outLineBuf += termStr
                termAmended = True
            self._serialLastChCode = chCode
            termOutStr += chr(chCode) if not termAmended else termStr
        self._sendToOutWindow(termOutStr)

    def _sendToLog(self, outStr):
        self._logHelper.info(outStr)

    def _handleKey(self, keyName):
        # print("Handle key ", keyName)
        rslt = self._inputLine.handleKey(keyName)
        if rslt == InputLine.RET_EXIT:
            self._running = False
        elif rslt == InputLine.RET_ENTER:
            # Send to serial
            outLine = self._inputLine.getLine()
            # print("Sending " + outLine)
            self._outputConn.send(outLine)
            self._inputLine.clear()

    def _sendToOutWindow(self, serialData):
        if self._inputLine.isActive():
            sys.stdout.write(self.CURSOR_UP + self.LINE_CLEAR_ALL + self.CURSOR_LINE_START)
            sys.stdout.write(serialData)
            sys.stdout.write(self.CURSOR_DOWN)
            self._inputLine.show()
        else:
            sys.stdout.write(serialData)
            sys.stdout.flush()

# Handle arguments
argparser = argparse.ArgumentParser(description='Serial Monitor')
argparser.add_argument('serialPort', action='store')
argparser.add_argument('-b', action='store', default=2000000, dest='serialbaud')
argparser.add_argument('-g', action='store_true', dest='logToFile',
                help='log to a file in ./logs folder with date-time based name')
argparser.add_argument('-c', action='store_true', dest='retainColors',
                help='retain colours in terminal output (log is always stripped of colours)')
args = argparser.parse_args()
# Logging
logHelper = LogHelper(args.logToFile, "logs")
# Terminal
terminal = Terminal(logHelper, not args.retainColors)
# Serial
serialIO = SerialIO(terminal.handleSerialData, logHelper)
terminal.setOutput(serialIO)
serialIO.open(args.serialPort, args.serialbaud, False)
try:
    while terminal.isOpen():
        pass
except KeyboardInterrupt:
    pass
serialIO.close()
terminal.close()
print()
