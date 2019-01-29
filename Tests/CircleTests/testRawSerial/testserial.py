import serial
import sys
import time
import math

try:
    testname = ' '.join(sys.argv[1:2])
    flashport = ' '.join(sys.argv[2:3])
    flashbaud = int(' '.join(sys.argv[3:]))
except Exception:
    print("Usage: python testserial.py TESTNAME SERIALPORT BAUDRATE")
    exit()

try:
    ser = serial.Serial(
        port=flashport,
        baudrate=flashbaud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
    )
except Exception:
    print("ERROR: Serial Port " + flashport + " busy or inexistent!")
    exit()

# try:
#     F = open(fileaddress,"rb")
#     F.seek(0, 2)
#     size = F.tell()
#     F.seek(0, 0)
# except Exception:
#     print("ERROR: Cannot access file " + fileaddress + ". Check permissions!")
#     ser.close()
#     exit()

print("Testing with baudrate of "+ str(flashbaud) + "...")
sys.stdout.flush()

numCharsToSend = 100000
try:
    lastFewChars = ""
    prevTime = time.time()
    while True:
        bytesToRead = ser.inWaiting()
        if bytesToRead > 0:
            dataRead = ser.read(bytesToRead)
            ascii = dataRead.decode("utf-8")
            print(ascii)
            lastFewChars = lastFewChars[-3:] + ascii
        if time.time() - prevTime > 30:
            break
        if "~~~" in lastFewChars:
            print(f"Received ~~~ going to send {numCharsToSend} now")
            break
except Exception as excp:
    print("ERROR: Serial port?", excp)
    ser.close()
    exit()

try:
    sendblock = bytearray()
    for i in range(numCharsToSend):
        sendblock.append(0xaa)
    ser.write(sendblock)
except Exception as excp:
    print("ERROR: Serial port?", excp)
    ser.close()
    exit()

try:
    prevTime = time.time()
    while True:
        bytesToRead = ser.inWaiting()
        if bytesToRead > 0:
            dataRead = ser.read(bytesToRead)
            ascii = dataRead.decode("utf-8")
            print(ascii)
        if time.time() - prevTime > 30:
            break
except Exception as excp:
    print("ERROR: Serial port?", excp)
    ser.close()
    exit()

ser.close()
