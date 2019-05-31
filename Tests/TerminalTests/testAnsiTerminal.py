from serial import Serial
import time, sys

with Serial('COM7', 115200) as s:

    s.write(b"\x1b[2J")
    s.write(b"\x1b[0;0H")
    # s.write(bytearray(f"\x1b[30m", "utf-8"))
    s.write(bytearray(f"\x1b[7mHello\x1b[0m", "utf-8"))

    s.write(b"\n\n\r")
    for i in range(8):
        s.write(bytearray(f"\x1b[{30+i};1mHelloWorld\x1b[0m Hello\n\r", "utf-8"))

    for i in range(8):
        s.write(bytearray(f"\x1b[{40+i};1mHelloWorld\x1b[0m Hello\n\r", "utf-8"))

    for i in range(0, 16):
        for j in range(0, 16):
            code = str(i * 16 + j)
            scodes = "\u001b[38;5;" + code + "m " + code.ljust(4)
            s.write(bytearray(scodes, "utf-8"))
            time.sleep(0.01)
        s.write(bytearray(f"\x1b[0m", "utf-8"))

    def loading():
        s.write(b"Loading...\r\n")
        for i in range(0, 100):
            time.sleep(0.01)
            scodes = "\x1b[1000D" + str(i + 1) + "%"
            s.write(bytearray(scodes, "utf-8"))
        s.write(b"\r\n")
        
    loading()

    def progbar():
        s.write(b"Progress...\r\n")
        for i in range(0, 100):
            time.sleep(0.01)
            width = (i + 1) // 4
            bar = "[" + "#" * width + " " * (25 - width) + "]"
            scodes = "\x1b[1000D" + bar
            s.write(bytearray(scodes, "utf-8"))
        s.write(b"\r\n")

        
    progbar()