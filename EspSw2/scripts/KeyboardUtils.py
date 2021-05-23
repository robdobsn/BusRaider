import threading
import time

class KeyboardUtils:
    
    # Keyboard input on windows and linux
    
    ansiEscSeqCodes = {
        49: [4, "HOME"], 
        50: [4, "INS"], 
        51: [4, "DEL"], 
        52: [4, "END"], 
        53: [4, "PAGEUP"], 
        54: [4, "PAGEDOWN"], 
        55: [4, "HOME"], 
        56: [4, "END"], 
        65: [3, "UP"], 
        66: [3, "DOWN"], 
        67: [3, "RIGHT"], 
        68: [3, "LEFT"], 
        70: [3, "END"], 
        72: [3, "HOME"], 
        }

    windowsKeyCodes = {
        71: [2, "HOME"], 
        72: [2, "UP"], 
        75: [2, "LEFT"], 
        77: [2, "RIGHT"], 
        73: [2, "PAGEUP"], 
        79: [2, "END"],
        80: [2, "DOWN"],
        81: [2, "PAGEDOWN"], 
        82: [2, "INS"],
        83: [2, "DEL"],
    }

    asciiCodes = {
        8: "BACKSPACE",
        9: "TAB",
        13: "ENTER",
        27: "ESC",
        127: "BACKSPACE",
    }
        
    def __init__(self, keyHandler):
        self.keybd = None
        self._keyHandler = keyHandler
        self._running = False
        self._isWindows = False
        self._multiSeqStart = 0
        self._multiSeqLookupPos = 1
        self._multiSeqCodes = self.windowsKeyCodes
        try:
            self.keybd = _KeybdWindows()
            self._isWindows = True
            self._multiSeqStart = 0
            self._multiSeqLookupPos = 1
            self._multiSeqCodes = self.windowsKeyCodes
        except ImportError:
            self.keybd = _KeybdUnix()
            self._multiSeqStart = 27
            self._multiSeqLookupPos = 2
            self._multiSeqCodes = self.ansiEscSeqCodes

        # Start threads
        self._running = True
        self._keyInThread = threading.Thread(target=self._KeyInThreadFn)
        self._keyInThread.setDaemon(True)
        self._keyInThread.start()
        self._keyEscThread = threading.Thread(target=self._KeyEscThreadFn)
        self._keyEscThread.setDaemon(True)
        self._keyEscThread.start()

    def close(self):
        self.keybd.close()
        self._running = False  

    def getch(self, blocking=False):
        return self.keybd.getch(blocking)

    def _KeyInThreadFn(self):
        self.lastKeyChecked = True
        self.lastKeyTime = time.time()
        self.multiSeqChars = []
        while self._running:
            char = self.getch(True)
            if char is None:
                continue
            # Check if we're in an escape sequence
            if len(self.multiSeqChars) > 0:
                self._handleEscSeq(ord(char))
                continue
            self.lastKeyChecked = False
            self.lastKeyTime = time.time()
            if ord(char) == self._multiSeqStart:
                self.multiSeqChars = [ord(char)]
            elif char.isprintable():
                self._keyHandler(char)
            else:
                self._handleControlKeys(ord(char))

    def _KeyEscThreadFn(self):
        while self._running:
            time.sleep(1)
            if not self.lastKeyChecked:
                # print(time.time() - self.lastKeyTime, len(self.multiSeqChars))
                if time.time() - self.lastKeyTime > 0.1:
                    if not self._isWindows and len(self.multiSeqChars) == 1:
                        self._keyHandler("ESC")
                    self.lastKeyChecked = True

    def _handleEscSeq(self, charCode):
        self.multiSeqChars.append(charCode)
        if (len(self.multiSeqChars) > self._multiSeqLookupPos):
            keyInfo = self._multiSeqCodes.get(self.multiSeqChars[self._multiSeqLookupPos], [])
            seqLen = keyInfo[0] if len(keyInfo) > 0 else (2 if self._isWindows else 3)
            keyName = keyInfo[1] if len(keyInfo) > 1 else ""
            # print("Keyinfo = ", seqLen, keyName, len(self.multiSeqChars))
            if len(self.multiSeqChars) == seqLen or len(self.multiSeqChars) == 4:
                # print(self.multiSeqChars, keyName)
                if self._keyHandler(keyName):
                    self._running = False
                self.multiSeqChars = []

    def _handleControlKeys(self, asciiCode):
        keyName = self.asciiCodes.get(asciiCode, "UNKNOWN")
        if keyName == "UNKNOWN" and asciiCode < 32:
            keyName = "CTRL-" + chr(asciiCode+64)
        self._keyHandler(keyName)

class _KeybdWindows:

    # Windows using msvcrt

    def __init__(self):
        import msvcrt
        if msvcrt.kbhit():
            pass

    def getch(self, blocking):
        import msvcrt
        # Check if there is a character waiting
        if blocking:
            return msvcrt.getwch()
        else:
            if msvcrt.kbhit():
                return msvcrt.getwch()
        return None

    def close(self):
        pass

class _KeybdUnix:

    # Linux using termios

    def __init__(self):
        import sys, tty, termios
        from select import select
        fd = sys.stdin.fileno()
        self.old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
        except Exception as excp:
            print("Failed to init setraw")

    def getch(self, blocking):
        import sys, tty, termios
        from select import select
        # Params to select are...
        # [ Wait until ready for reading,
        #   wait until ready for writing
        #   wait for an "exception condition"
        #   timeout in seconds ]
        if blocking:
            keyIn = sys.stdin.read(1)
            # print(ord(keyIn))
            return keyIn
        else:
            [i, o, e] = select([sys.stdin.fileno()], [], [], .001)
            if i:
                return sys.stdin.read(1)
        print(".", end="")
        return None

    def close(self):
        import sys, tty, termios
        from select import select
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self.old_settings)

