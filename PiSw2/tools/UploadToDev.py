import argparse
from martypy import RICCommsWiFi, RICInterface

# This is a program for uploading firmware to BusRaider
# It requires either:
# - a standard BusRaider with ESP32 running BusRaider ESP firmware and on Wifi talking to a Pi running BusRaider s/w v3+
# - a direct serial connection to the Pi (maybe using Adafruit PiUART) and running the BusRaider s/w V3+

# class BusRaiderConn:

#     def __init__(self) -> None:
#         pass

#     def open(self, openParams) -> bool:
#         # Comms
#         ricComms = RICCommsWiFi()
#         self.busRaider = RICInterface.RICInterface(ricComms)
#         self.busRaider.open({"ipAddrOrHostname":self.ipAddr})
#         self.busRaider.setDecodedMsgCB(self.decodedMsg)
#         self.busRaider.setLogLineCB(self.logLine)
#         # Logging
#         self.logger = logging.getLogger(__name__)
#         self.logger.setLevel(logging.DEBUG)
#         return True

#     def close(self) -> None:
#         self.busRaider.close()

#     # def send(self, data) -> None:
    #     self.commsHandler.send(data)

    # def statsClear(self) -> None:
    #     self.uploadAcked = False

    # Received frame handler
    # def _onRxFrameCB(self, frame) -> None:
    #     # print("Frame RX")
    #     msgContent = {'cmdName':''}
    #     termPos = frame.find(0)
    #     frameJson = frame
    #     if termPos >= 0:
    #         frameJson = frame[0:termPos]
    #     try:
    #         msgContent = json.loads(frameJson)
    #     except Exception as excp:
    #         print("Failed to parse Json from", frameJson, excp)
    #     if msgContent['cmdName'] == "ufEndAck":
    #         print("Upload Acknowledged")
    #         self.uploadAcked = True
    #     elif msgContent['cmdName'] == "ufEndNotAck":
    #         print("Upload FAILED")
    #     elif msgContent['cmdName'] == "log":
    #         try:
    #             print(msgContent['lev'] + ":", msgContent['src'], msgContent['msg'])
    #         except Exception as excp:
    #             print("LOG CONTENT NOT FOUND IN FRAME", frameJson, excp)
    #     elif msgContent['cmdName'] != "busStats" and msgContent['cmdName'] != "addrStats":
    #         print("Unknown cmd msg", frameJson)

# def sendFile(fileName: str, conn: Union[None, BusRaiderConn]) -> None:
#     # Send new firmware
#     with open(fileName, "rb") as f:

#         # Read firmware
#         binaryImage = f.read()
#         binaryImageLen = len(binaryImage)

#         # Calc CRC
#         fileCRC = calcCRC(binaryImage)
#         crcStr = f"0x{fileCRC[0]*256+fileCRC[1]:04x}"
#         print(f"File {fileName} is {binaryImageLen} bytes long crc is {crcStr}")

#         # Conn type
#         if conn is None:

#             # Send using http
#             url = "http://" + args.ipaddr + "/uploadpisw"
#             # url = "https://httpbin.org/post"
#             files = {'file': ("kernel.img", open(fileName, 'rb'), 
#                         'application/octet-stream', 
#                         {'CRC16': crcStr, "FileLengthBytes": binaryImageLen})
#             }
#             # print("+++++++++++++++++++++++++++++++++++++++++++++++++")
#             # print(requests.Request('POST', url, files=files).prepare().body)
#             # print("+++++++++++++++++++++++++++++++++++++++++++++++++")
#             r = requests.post(url, files=files)
#             # print("+++++++++++++++++++++++++++++++++++++++++++++++++")
#             # print(r.json())
#             # print("+++++++++++++++++++++++++++++++++++++++++++++++++")
#             # pprint(r.json()['headers'])
#             # print("+++++++++++++++++++++++++++++++++++++++++++++++++")
#             print(r.text)

#         else:
#             # Frames follow the approach used in the web interface start, block..., end
#             startJson = {
#                     "cmdName":"ufStart", 
#                     "fileName":"kernel.img", 
#                     "fileType":"firmware", 
#                     "fileLen":str(binaryImageLen), 
#                     "CRC16": crcStr 
#             }
#             startFrame = json.dumps(startJson).encode('utf-8') + '\0'
#             emulatorConn.send(startFrame)

#             # Split the file into blocks
#             blockMaxSize = 1024
#             numBlocks = binaryImageLen//blockMaxSize + (0 if (binaryImageLen % blockMaxSize == 0) else 1)
#             for i in range(numBlocks):
#                 blockStart = i*blockMaxSize
#                 blockToSend = binaryImage[blockStart:blockStart+blockMaxSize]
#                 dataFrame = bytearray(b"{\"cmdName\":\"ufBlock\",\"index\":\"" + bytes(str(blockStart),"ascii") + b"\"}\0") + blockToSend
#                 emulatorConn.send(dataFrame)

#             # End frame            
#             endFrame = bytearray(b"{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + bytes(str(numBlocks),"ascii") + b"\"}\0")
#             emulatorConn.send(endFrame)

#             # Check for end frame acknowledged
#             prevTime = time.time()
#             while True:
#                 if emulatorConn.uploadAcked:
#                     break
#                 if time.time() - prevTime > 2:
#                     break

argparser = argparse.ArgumentParser(description='UploadToBusRaider')
DEFAULT_SERIAL_PORT = "COM7"
DEFAULT_SERIAL_BAUD = 921600
DEFAULT_IP_ADDRESS = "192.168.86.7"
argparser.add_argument('--fileName', help='File Name', default='/home/rob/rdev/BusRaider/PiSw2/src/kernel.img')
argparser.add_argument('--port', help='Serial Port', default=DEFAULT_SERIAL_PORT)
argparser.add_argument('--baud', help='Serial Baud', default=DEFAULT_SERIAL_BAUD)
argparser.add_argument('--ipaddr', help='IP Address', default=DEFAULT_IP_ADDRESS)
args = argparser.parse_args()

# Connection
try:
    ricComms = RICCommsWiFi()
    busRaider = RICInterface.RICInterface(ricComms)
    busRaider.open({"ipAddrOrHostname":args.ipaddr})
except Exception as excp:
    print(f"Cannot open connection {excp}")
    exit()

print("Upload Started")

try:
    rslt = busRaider.sendFile(args.fileName, "fs", "uploadpisw")
    if not rslt:
        print("File transfer failed")
    else:
        print("File transferred ok")
except Exception as excp:
    print(f"File transfer exception {excp}")