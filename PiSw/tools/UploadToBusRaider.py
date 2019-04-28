import serial
import sys
import time
import math
from SimpleHDLC import HDLC
import logging
import json

# This is a program for uploading to the BusRaider firmware
# It requires either:
# a) a BusRaider supporting the CommandHandler protocol (HDLC) and operating on a serial port
#   (BusRaider jumpers configured to connect Pi serial to FTDI)
# b) a BusRaider with ESP32 firmware for uploading (normal firmware) and configured
#    with ESP Serial 0 to Pi
# c) a BusRaider with DWelch Intel Hex bootloader and Pi Serial to FTDI

# Function to send 
def sendFileSerialIntelHexToDWelchBootloader(ser):
    with open(baseFolder + "/build/BusRaider.hex", "rb") as f:
        program = f.read()
        ser.write(program)
        ser.write(b"G")

# Function to send 
def sendFileSerialSRECToDWelchBootloader(ser):
    with open(baseFolder + "/build/BusRaider.srec", "rb") as f:
        program = f.read()
        ser.write(program)
        ser.write(b"G")

def sendFileSerialHDLC(ser):
    # Frame received
    def onFrame(fr):
        global uploadAck
        msgContent = {'cmdName':''}
        try:
            msgContent = json.loads(fr)
        except Exception as excp:
            print("Failed to parse Json from", fr, excp)
        if msgContent['cmdName'] == "ufEndAck":
            print("Upload Acknowledged")
            uploadAck = True
        elif msgContent['cmdName'] == "ufEndNotAck":
            print("Upload FAILED")
        elif msgContent['cmdName'] == "log":
            try:
                print(msgContent['lev'] + ":", msgContent['src'], msgContent['msg'])
            except Exception as excp:
                print("LOG CONTENT NOT FOUND IN FRAME", fr, excp)
        else:
            print("Unknown cmd msg", fr)

    # Setup HDLC sender
    hdlcHandler = HDLC(ser)
    hdlcHandler.startReader(onFrame)

    # Upload acknowledged
    uploadAck = False

    # Send new firmware in the bin folder using the CommandHandler protocol
    imageFileName = "/kernel.img"
    with open(baseFolder + imageFileName, "rb") as f:

        # Read firmware
        binaryImage = f.read()
        binaryImageLen = len(binaryImage)
        print(f"File {imageFileName} is {binaryImageLen} bytes long")

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
            if uploadAck:
                break
            if time.time() - prevTime > 2:
                break

    # Remain running for a little while to hoover up diagnostics, etc
    try:
        prevTime = time.time()
        while True:
            if uploadAck:
                break
            if time.time() - prevTime > 1:
                break
        hdlcHandler.stopReader()
    except Exception as excp:
        print("ERROR: Serial port?", excp)
        hdlcHandler.stopReader()

    ser.close()

# Get args passed from build/test script
logging.basicConfig(format='%(asctime)s: %(message)s', level=logging.DEBUG)
try:
    useIP = ' '.join(sys.argv[1:2])
    sendType = ' '.join(sys.argv[2:3])
    serialOrIp = ' '.join(sys.argv[3:4])
    flashbaud = int(' '.join(sys.argv[4:5]))
    baseFolder = ' '.join(sys.argv[5:])
except Exception:
    print("Usage: python UploadToBusRaider.py METHOD TYPE NAME BAUDRATE BASEFOLDER")
    print("Where: METHOD = ip for using IP address, ser for serial")
    print("       TYPE = ihex for Intel Hex, hdlc for hdlc")
    print("       NAME = serial port or IP address, etc")
    exit()

# Check for using IP address
if useIP == 'ip':
    print("IP not yet supported")
else:
    # Open the serial connection to the BusRaider
    try:
        serObj = serial.Serial(
            port=serialOrIp,
            baudrate=flashbaud,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS
        )
    except Exception:
        print("ERROR: Serial Port " + serialOrIp + " busy or non-existent!")
        exit()

    print("Serial baudrate "+ str(flashbaud) + "...")
    sys.stdout.flush()
    # See if firmware is to be sent to bootloader first
    if sendType == 'ihex':
        # This test assumes that the DWelch bootloader is running on the BusRaider and expecting an Intel Hex file
        sendFileSerialIntelHexToDWelchBootloader(serObj)
    elif sendType == 'srec':
        # This test assumes that the DWelch bootloader is running on the BusRaider and expecting an SREC file
        sendFileSerialSRECToDWelchBootloader(serObj)
    else:
        sendFileSerialHDLC(serObj)

exit(0)