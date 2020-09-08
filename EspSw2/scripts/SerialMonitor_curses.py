#
# Serial Monitor Utility for RIC Firmware Development
#
# Rob Dobson 2020
#

import time
import logging
import serial
import sys, os
import json
import keyboard
if os.name == 'nt':
    import msvcrt
import threading
import re
import argparse
import binascii

import curses

class SerialMon:

    def __init__(self):
        self.running = False
        self.removeESC = re.compile(r'\x1b[^m]*m')

    def open(self, serialPort, serialBaud, logToFile):

        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        self.serialPort = serialPort
        self.serialBaud = serialBaud
        # logch = logging.StreamHandler()
        # logch.setLevel(logging.DEBUG)
        # self.logger.addHandler(logch)

        # Log to file if required
        if logToFile:
            # Check the log folder exists
            logsFolder = "logs"
            if not os.path.exists(logsFolder):
                os.mkdir(logsFolder)
            
            # Form log file name
            logFileName = time.strftime("%Y%m%d-%H%M%S") + ".log"
            logFileName = os.path.join(logsFolder, logFileName)
            print("Logging to file " + logFileName)
            logfh = logging.FileHandler(logFileName)
            logfh.setLevel(logging.DEBUG)
            # logformatter = logging.Formatter('%(asctime)s: %(name)s %(levelname)s %(message)s')
            # logfh.setFormatter(logformatter)
            self.logger.addHandler(logfh)

        return self.portOpen()

    def portOpen(self):
        # Open the serial connection
        try:
            self.ser = serial.Serial(
                port=self.serialPort,
                baudrate=self.serialBaud,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
        except Exception as excp:
            self.logger.error("Serial Port " + self.serialPort + " " + str(excp))
            return False
        try:
            self.ser.set_buffer_size(20000, None)
        except Exception:
            self.logger.error("Failed to set serial buffer size")

        self.logger.info(f"SerialMonitor port {self.serialPort} baudrate {self.serialBaud}")
        sys.stdout.flush()

        # Setup serial
        self.startReader(self.onRx)
        return True

    def close(self):
        try:
            self.stopReader()
            self.ser.close()
        except Exception as excp:
            print("Error stopping and closing port", excp)

    def send(self, data):
        self.ser.write(data)

    def onRx(self, rxData):
        print(rxData, end="")
        rxLines = rxData.splitlines()
        for line in rxLines:
            sLine = line.strip()
            if len(sLine) == 0:
                continue
            self.logger.info(sLine)

    def startReader(self, onData):
        if self.running:
            raise RuntimeError("reader already running")
        self.reader = threading.Thread(target=self._receiveLoop)
        self.reader.setDaemon(True)
        self.data_callback = onData
        self.running = True
        self.reader.start()

    def stopReader(self):
        self.running = False
        try:
            self.reader.join()
        except:
            pass
        self.reader = None

    def _receiveLoop(self):
        while self.running:
            try:
                numWaiting = self.ser.in_waiting
                if numWaiting < 1:
                    time.sleep(0.001)
                    # print(".", end="")
                    # sys.stdout.flush()
                    continue
                rxBytes = bytearray(self.ser.read(numWaiting))
                endPos = -1
                for i in range(len(rxBytes)):
                    if rxBytes[i] > 127:
                        endPos = i
                        break
                if endPos >= 0:
                    rxAscii = rxBytes[:endPos]
                    rxBytes = rxBytes[endPos:]
                else:
                    rxAscii = rxBytes
                for i in range(len(rxAscii)):
                    if rxAscii[i] > 127:
                        print("SOMETHING WENT WRONG pos", i, "endPos", endPos)
                ascText = rxAscii.decode("utf-8")
                # ascText = self.removeESC.sub('', ascText)
                self.data_callback(ascText)
                if endPos >= 0:
                    self.data_callback("\n" + binascii.hexlify(rxBytes).decode("utf-8") + "\n")
            except Exception as excp:
                print("Error in serial", excp)
                time.sleep(5.0)
                try:
                    self.close()
                    self.portOpen()
                except Exception as excp2:
                    print("Error reopening serial", excp2)

# Mac and Linux only
def handleKey(keyInfo):
    global breakRequested
    if keyInfo.name == 'esc':
        breakRequested = True
    # User is providing command input
    if keyInfo.name == 'i':
        handleCommand()

def handleCommand():
    # Initialise curses and open a window object/terminal screen for command input in the
    # current terminal screen.
    stdscr = curses.initscr()
    stdscr.addstr("Enter your command to send to Marty: ")
    stdscr.refresh()
    # Store the command entered (in bytes automatically) and exit curses terminal screen
    command = stdscr.getstr() + b"\r"
    curses.endwin()
    # Send command through the serial port
    serialMon.send(command)

def sendCommand(command):
    serialMon.send((command + "\r").encode('utf-8'))

def sendCommands(command, n, moveTime):
    print(command)
    while n > 0:
        sendCommand(command)
        time.sleep(moveTime )
        n -= 1

def demo():
    moveTimeFast = "?moveTime=1000"
    moveTimeSlow = "?moveTime=2500"
    moveTimeVerySlow = "?moveTime=4500"
    dance = "traj/dance"
    step = "traj/step"
    turn = "traj/step?turn=50"
    kick = "traj/kick"
    kickLeft = kick + moveTimeSlow + "side=0"
    kickRight = kick + moveTimeSlow + "side=1"
    sidestep = "traj/sidestep"
    sidefall = "traj/sidefall"
    wiggle = "traj/wiggle"
    wave = "traj/wave"
    waveLeft = wave + moveTimeSlow + "side=0"
    waveRight = wave + moveTimeSlow + "side=1"
    wiggleEyes = "traj/wiggleEyes"
    circle = "traj/circle"
    eyesAngry = "traj/eyesAngry"
    eyesExcited = "traj/eyesExcited"
    eyesNormal = "traj/eyesNormal"
    eyesWide = "traj/eyesWide"
    hold = "traj/hold"
    getReady = "traj/getReady"
    joint = "traj/joint"
    liftFoot = "traj/liftFoot"
    lowerFoot = "traj/lowerFoot"

    sendCommands(getReady + moveTimeFast, 2, 1)
    sendCommands(step + moveTimeFast, 10, 1)
    sendCommands(circle + moveTimeSlow, 1, 2.5)
    sendCommands(dance + moveTimeVerySlow, 1, 4.5)
    sendCommands(turn + moveTimeFast, 12, 1)
    time.sleep(3)
    sendCommands(waveLeft, 1, 1)
    sendCommands(waveRight, 1, 1)
    sendCommands(kickLeft, 1, 2.5)
    sendCommands(kickRight, 1, 2.5)
    time.sleep(3)
    sendCommands(dance + moveTimeSlow, 1, 1.5)
    sendCommands(getReady, 1, 1)


# Handle arguments
argparser = argparse.ArgumentParser(description='Serial Monitor')
argparser.add_argument('serialPort', action='store')
argparser.add_argument('-b', action='store', default=115200, dest='serialbaud')
argparser.add_argument('-g', action='store_true', default=False, dest='logToFile',
                help='log to a file in ./logs folder with date-time based name')
args = argparser.parse_args()

# Serial
serialMon = SerialMon()
serMonOpenOk = serialMon.open(args.serialPort, args.serialbaud, args.logToFile)
if os.name != 'nt':
    keyboard.on_press(handleKey)
breakRequested = False
if serMonOpenOk:
    print("Serial Monitor Started")
else:
    print("Couldn't open serial port")
while serMonOpenOk and not breakRequested:
    if os.name == 'nt':
        if msvcrt.kbhit():
            ch = msvcrt.getch()
            if ord(ch) == 27: # ESC
                breakRequested = True
                print("Stopping - user hit ESC")
            elif ord(ch) == ord('i'): #i sets up input
                handleCommand()
            elif ord(ch) == ord("d"): #d sets up demo
                demo()
    time.sleep(0.1)
serialMon.close()
