
import socket
from threading import Thread
import logging
import time

class SimpleTCP(object):

    def __init__(self, ipAddr, port, dumpFile=None):
        self.ipAddr = ipAddr
        self.port = port
        self.frame_callback = None
        self.error_callback = None
        self.running = False
        self.dumpFile = dumpFile

    @classmethod
    def toBytes(cls, data):
        return bytearray(data)

    def sendFrame(self, data):
        res = self.rdpSocket.send(data)
        # time.sleep(0.05)
        # logger.info("Send %s bytes", res)

    def _receiveLoop(self):
        while self.running:
            try:
                rxData = self.rdpSocket.recv(10000)
                if not rxData:
                    if self.error_callback is not None:
                        self.error_callback()
                    continue
                if self.dumpFile is not None:
                    self.dumpFile.write(rxData)
                if self.frame_callback is not None:
                    self.frame_callback(rxData)
            except Exception as excp:
                print(excp)
        self.rdpSocket.close()

    def startReader(self, onFrame, onError=None):
        if self.running:
            raise RuntimeError("reader already running")

        self.rdpSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.rdpSocket.connect((self.ipAddr, self.port))

        self.reader = Thread(target=self._receiveLoop)
        self.reader.setDaemon(True)
        self.frame_callback = onFrame
        self.error_callback = onError
        self.running = True
        self.reader.start()

    def stopReader(self):
        self.running = False
        self.rdpSocket.shutdown(socket.SHUT_WR)
        time.sleep(1)
        try:
            self.reader.join()
        except:
            pass
        self.reader = None

