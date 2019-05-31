from serial import Serial

with Serial('COM7', 115200) as s:

    s.write(b"\x1b[2J")
    s.write(b"\x1b[0;0H")
    # s.write(bytearray(f"\x1b[30m", "utf-8"))
    s.write(bytearray(f"\x1b[7mHello", "utf-8"))

    s.write(b"\n\n\r")
    for i in range(8):
        s.write(bytearray(f"\x1b[{30+i}m\x1b[7mHelloWorld\x1b[0m Hello\n\r", "utf-8"))

    # for i in range(8):
    #     s.write(bytearray(f"\x1b[{40+i}mHelloWorld\x1b[0m Hello\n\r", "utf-8"))

    import time, sys
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