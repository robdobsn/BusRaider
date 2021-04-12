
import socket
import logging
from typing import Callable, Union
from .WebSocketFrame import WebSocketFrame

logger = logging.getLogger(__name__)

class WebSocket():

    maxSocketBytes = 2000
    MAX_RX_PREUPGRADE_LEN = 2000
    SOCKET_AWAITING_UPGRADE = 0
    SOCKET_UPGRADED = 1

    def __init__(self, 
            onBinaryFrame: Callable[[bytes], None],
            onTextFrame: Callable[[str], None],
            onError: Callable[[str], None],
            ipAddrOrHostname: str,
            ipPort: int = 80,
            wsPath: str = "/ws",
            timeout: float = 30.0) -> None:
        self.ipAddr = socket.gethostbyname(ipAddrOrHostname)
        self.wsPath = wsPath
        self.ipPort = ipPort
        self.timeout = timeout
        self.sock = None
        self.wsFrameCodec = WebSocketFrame()
        self.onBinaryFrame = onBinaryFrame
        self.onTextFrame = onTextFrame
        self.onError = onError
        self.socketState = self.SOCKET_AWAITING_UPGRADE
        self.rxPreUpgrade = bytearray()

    def __del__(self) -> None:
        self.close()

    def open(self) -> bool:
        # Open socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout)
        self.sock.connect((self.ipAddr, self.ipPort))
        # Initiate upgrade
        self._sendUpgradeReq()

    def writeBinary(self, inFrame: bytes) -> int:
        if not self.sock:
            return 0
        frame = WebSocketFrame.encode(inFrame, False, WebSocketFrame.OPCODE_BINARY, True)
        # logger.debug(f"WebSocket write {''.join('{:02x}'.format(x) for x in frame)}") 
        return self.sock.send(frame)

    def close(self) -> None:
        if self.sock:
            self.sock.close()
        self.sock = None

    def _sendUpgradeReq(self) -> None:
        if not self.sock:
            return
        headerStr = f"GET {self.wsPath} HTTP/1.1\r\n" + \
            "Connection: upgrade\r\n" + \
            "Upgrade: websocket\r\n" + \
            "\r\n"
        self.sock.send(headerStr.encode())

    def service(self) -> None:
        # Get any data
        rxData = self.sock.recv(self.maxSocketBytes)
        # Check state of socket connection
        if self.socketState == self.SOCKET_AWAITING_UPGRADE:
            self.rxPreUpgrade += rxData
            # Check for upgrade header complete
            headerEndPos = self.rxPreUpgrade.find(b"\r\n\r\n")
            if headerEndPos > 0:
                # Check upgrade ok
                if b"Sec-WebSocket-Accept" in self.rxPreUpgrade:
                    self.socketState = self.SOCKET_UPGRADED
                    self.wsFrameCodec.addDataToDecode(self.rxPreUpgrade[headerEndPos+4:])
                    # logger.debug("WebSocket upgraded")
            elif len(self.rxPreUpgrade) > self.MAX_RX_PREUPGRADE_LEN:
                self.rxPreUpgrade.clear()
        else:
            self.wsFrameCodec.addDataToDecode(rxData)
            checkForData = True
            while checkForData:
                checkForData = False
                # Check for actions required
                if self.wsFrameCodec.getPongRequired():
                    self._sendPong()
                    checkForData = True
                binaryFrame = self.wsFrameCodec.getBinaryMsg()
                if binaryFrame:
                    checkForData = True
                    # logger.debug(f"WebSocket proc binary len {len(binaryFrame)}")
                    if self.onBinaryFrame:
                        self.onBinaryFrame(binaryFrame)
                textFrame = self.wsFrameCodec.getTextMsg()
                if textFrame and self.onTextFrame:
                    checkForData = True
                    self.onTextFrame(textFrame)

    def _sendPong(self) -> None:
        if not self.sock:
            return 0
        frame = WebSocketFrame.encode(self.wsFrameCodec.getPongData(),
                    False, WebSocketFrame.OPCODE_PONG, True)
        # logger.debug(f"WebSocket pong {''.join('{:02x}'.format(x) for x in self.wsFrameCodec.getPongData())}") 
        return self.sock.send(frame)
