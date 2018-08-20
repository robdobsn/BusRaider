import serial
import threading
import keyboard
import time

# Read data from serial port and echo
def serialRead():
    global serialIsClosing, serPort

    while True:
        if serialIsClosing:
            break
        if serPort.isOpen():
            val = serPort.read()
            if len(val) == 0:
                continue
            for v in val:
                print("{:02x} ".format(v), end="")
            print()

serPort = serial.Serial('COM5', 115200)
serialIsClosing = False

# Thread for reading from port
thread = threading.Thread(target=serialRead, args=())
thread.start()

while True:
    if keyboard.is_pressed(' '):
        serialIsClosing = True
        time.sleep(1.0)
        break
    
serPort.close()
