from threading import Thread
import logging
import time
import websocket

class SimpleWS(object):

    def __init__(self, wsUrl, dumpFile=None):
        self.wsUrl = wsUrl
        self.frame_callback = None
        self.error_callback = None
        self.open_callback = None
        self.close_callback = None
        self.running = False
        self.dumpFile = dumpFile

    @classmethod
    def toBytes(cls, data):
        return bytearray(data)

    def sendFrame(self, data):
        # res = \
        self.ws.send(data, opcode=websocket.ABNF.OPCODE_BINARY)
        # time.sleep(0.05)
        # logger.info("Send %s bytes", res)

    def _wsLoop(self):
        self.ws.run_forever()

    def onWSOpen(self):
        print(f"WS onWSOpen")
        self.reader = Thread(target=self._wsLoop)
        self.reader.setDaemon(True)
        self.reader.start()
        self.running = True
        if (self.open_callback):
            self.open_callback()

    def onWSClose(self):
        print(f"WS onWSClose")
        if (self.close_callback):
            self.close_callback()

    def onWSFrame(self, frame):
        print(f"WS RX Frame len {len(frame)}")
        if (self.frame_callback):
            self.frame_callback(frame)

    def onWSError(self, error):
        print(f"WS Error {error}")
        if (self.error_callback):
            self.error_callback()

    def startReader(self, onFrame, onError=None, onOpen=None, onClose=None):
        if self.running:
            raise RuntimeError("reader already running")

        self.frame_callback = onFrame
        self.error_callback = onError
        self.close_callback = onClose
        self.open_callback = onOpen

        # websocket.enableTrace(True)
        self.ws = websocket.WebSocketApp(self.wsUrl,
                              on_message = self.onWSFrame,
                              on_error = self.onWSError,
                              on_close = self.onWSClose)
        self.ws.on_open = self.onWSOpen
        
        # Ensure running
        for _ in range(20):
            if self.running:
               break
            time.sleep(0.1)
        if not self.running:
            raise RuntimeError("cannot open websocket")
        time.sleep(1)

    def stopReader(self):
        self.ws.close()
        self.running = False
        self.reader = None
