import serial
import sys
import time
import math
from SimpleHDLC import HDLC
import logging

# This is a program for testing the BusRaider firmware
# It requires a BusRaider supporting the CommandHandler protocol
# The script can run a number of tests

# Function to send 
def SendIntelHexFirmwareToDWelchBootloader():
    with open(baseFolder + "/build/BusRaider.hex", "rb") as f:
        program = f.read()
        ser.write(program)
        ser.write(b"G")


# Get args passed from build/test script
logging.basicConfig(format='%(asctime)s: %(message)s', level=logging.DEBUG)
try:
    testname = ' '.join(sys.argv[1:2])
    sendBoot = ' '.join(sys.argv[2:3])
    iterCount = int(' '.join(sys.argv[3:4]))
    flashport = ' '.join(sys.argv[4:5])
    flashbaud = int(' '.join(sys.argv[5:6]))
    baseFolder = ' '.join(sys.argv[6:])
except Exception:
    print("Usage: python SendAndTest.py TESTNAME SERIALPORT BAUDRATE BASEFOLDER")
    exit()

# Open the serial connection to the BusRaider
try:
    ser = serial.Serial(
        port=flashport,
        baudrate=flashbaud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
    )
except Exception:
    print("ERROR: Serial Port " + flashport + " busy or non-existent!")
    exit()

print("Serial baudrate "+ str(flashbaud) + "...")
sys.stdout.flush()

# See if firmware is to be sent to bootloader first
if sendBoot:
    # This test assumes that the DWelch bootloader is running on the BusRaider and expecting an Intel Hex file
    SendIntelHexFirmwareToDWelchBootloader()    
    # Now wait a mo
    print("Pausing for 4s to allow firmware to start")
    time.sleep(4)

# Setup HDLC sender
hdlcHandler = HDLC(ser)

# Test results
ackCount = 0
notAckCount = 0
loopCount = 0

# Perform tests
if testname == "test1":

    # Test state
    def onFrame(fr):
        global ackCount, notAckCount
        print(fr)
        if "ufEndAck" in fr:
            ackCount += 1
            print("Received end frame acknowledgement")
        elif "ufEndNotAck" in fr:
            notAckCount += 1
            print("Received end frame NOT acknowledgement")
    hdlcHandler.startReader(onFrame)

    # Send new firmware in the bin folder using the CommandHandler protocol
    imageFileName = "/bin/kernel.img"
    with open(baseFolder + imageFileName, "rb") as f:

        # Read firmware
        binaryImage = f.read()
        binaryImageLen = len(binaryImage)
        print(f"File {imageFileName} is {binaryImageLen} bytes long")

        # Test iterations
        for testIdx in range(iterCount):

            # Frames follow the approach used in the web interface start, block..., end
            startFrame = bytearray(b"{\"cmdName\":\"ufStart\",\"fileName\":\"kernel.img\",\"fileType\":\"firmware\",\"fileLen\":\"" + \
                            bytes(str(binaryImageLen),"ascii") + b"\"}\0")
            hdlcHandler.sendFrame(startFrame)

            # Split the file into blocks
            blockMaxSize = 1024
            numBlocks = binaryImageLen//blockMaxSize + (0 if (binaryImageLen % blockMaxSize == 0) else 1)
            for i in range(numBlocks):
                blockStart = i*blockMaxSize
                blockToSend = binaryImage[blockStart:blockStart+blockMaxSize]
                dataFrame = bytearray(b"{\"cmdName\":\"ufBlock\",\"index\":\"" + bytes(str(blockStart),"ascii") + b"\"}\0") + blockToSend
                hdlcHandler.sendFrame(dataFrame)

            # End frame            
            endFrame = bytearray(b"{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + bytes(str(numBlocks),"ascii") + b"\"}\0")
            hdlcHandler.sendFrame(endFrame)

            # Check for end frame acknowledged
            prevTime = time.time()
            while True:
                if time.time() - prevTime > 2:
                    break
            loopCount += 1

# Results
print(f"Test results ack {ackCount} not-ack {notAckCount} of {loopCount} tests")

# Remain running for a little while to hoover up diagnostics, etc
try:
    prevTime = time.time()
    while True:
        if time.time() - prevTime > 15:
            break
    hdlcHandler.stopReader()
except Exception as excp:
    print("ERROR: Serial port?", excp)
    hdlcHandler.stopReader()

ser.close()
