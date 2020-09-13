import json

# Protocol definitions
MSG_TYPE_COMMAND = 0x00
PROTOCOL_ROSSERIAL = 0x00
PROTOCOL_M1SC = 0x01
PROTOCOL_RICREST = 0x02
RICREST_ELEM_CODE_URL = 0x00
RICREST_ELEM_CODE_JSON = 0x01
RICREST_ELEM_CODE_BODY = 0x02
RICREST_ELEM_CODE_CMD_FRAME = 0x03
RICREST_ELEM_CODE_FILE_BLOCK = 0x04
dirnStrs = ["cmd", "resp", "publish", "report"]
restCmds = ["url", "json", "body", "cmd", "fileBlk"]

class DecodedMsg:

    def __init__(self):
        self.errMsg = ""
        self.msgNum = None
        self.protocolID = None
        self.isText = None
        self.payload = None
        self.restType = None
        self.direction = None

    def setError(self, errMsg):
        self.errMsg = errMsg

    def setMsgNum(self, msgNum):
        self.msgNum = msgNum

    def setProtocol(self, protocolID):
        self.protocolID = protocolID

    def setDirection(self, dirn):
        self.direction = dirn

    def setRESTType(self, restType):
        self.restType = restType

    def setPayload(self, isText, payload):
        self.isText = isText
        self.payload = payload

    def getJSONDict(self):
        msgContent = {}
        if self.isText:
            try:
                termPos = self.payload.find("\0")
                frameJson = self.payload
                if termPos >= 0:
                    frameJson = self.payload[0:termPos]
                    binData = self.payload[termPos+1:len(self.payload)-1]
                else:
                    return msgContent
                msgContent = json.loads(frameJson)
            except Exception as excp:
                print("RICProtocols getJSONDict failed to extract JSON from", self.payload, "error", excp)
        # print("msgContent ", msgContent)
        return msgContent

    def toString(self):
        msgStr = ""
        if self.msgNum is not None:
            msgStr += f"MsgNum: {self.msgNum} "
        if self.protocolID is not None:
            msgStr += "Protocol:"
            if self.protocolID == PROTOCOL_RICREST:
                msgStr += "RICREST"
            elif self.protocolID == PROTOCOL_M1SC:
                msgStr += "M1SC"
            elif self.protocolID == PROTOCOL_ROSSERIAL:
                msgStr += "ROSSERIAL"
            else:
                msgStr += f"OTHER {self.protocolID}"
            msgStr += " "
        if self.direction is not None:
            msgStr += "Dirn:"
            if self.direction >= 0 and self.direction < len(dirnStrs):
                msgStr += dirnStrs[self.direction]
            else:
                msgStr += f"OTHER {self.direction}"
            msgStr += " "
        if self.restType is not None:
            msgStr += "Type:"
            if self.restType >= 0 and self.restType < len(restCmds):
                msgStr += restCmds[self.restType]
            else:
                msgStr += f"OTHER {self.restType}"
            msgStr += " "
        if self.isText is not None:
            if self.isText:
                msgStr += "Payload:" + self.payload
            else:
                msgStr += f"PayloadLen: {len(self.payload)} "
        return msgStr

class RICProtocols:

    def __init__(self):
        self.ricSerialMsgNum = 1
        pass

    def encodeRICRESTURL(self, cmdStr):
        # RICSerial URL
        msgNum = self.ricSerialMsgNum
        cmdFrame = bytearray([msgNum, MSG_TYPE_COMMAND + PROTOCOL_RICREST, RICREST_ELEM_CODE_URL])
        cmdFrame += cmdStr.encode() + b"\0"
        self.ricSerialMsgNum += 1
        if self.ricSerialMsgNum > 255:
            self.ricSerialMsgNum = 1
        return cmdFrame, msgNum

    def encodeRICRESTCmdFrame(self, cmdStr):
        # RICSerial command frame
        msgNum = self.ricSerialMsgNum
        cmdFrame = bytearray([msgNum, MSG_TYPE_COMMAND + PROTOCOL_RICREST, RICREST_ELEM_CODE_CMD_FRAME])
        cmdFrame += cmdStr.encode() + b"\0"
        self.ricSerialMsgNum += 1
        if self.ricSerialMsgNum > 255:
            self.ricSerialMsgNum = 1
        return cmdFrame, msgNum

    def encodeRICRESTFileBlock(self, cmdBuf):
        # RICSerial file block
        msgNum = self.ricSerialMsgNum
        cmdFrame = bytearray([msgNum, MSG_TYPE_COMMAND + PROTOCOL_RICREST, RICREST_ELEM_CODE_FILE_BLOCK])
        cmdFrame += cmdBuf
        self.ricSerialMsgNum += 1
        if self.ricSerialMsgNum > 255:
            self.ricSerialMsgNum = 1
        return cmdFrame, msgNum

    @classmethod
    def decodeRICFrame(cls, fr):
        msg = DecodedMsg()
        if len(fr) < 2:
            msg.setError(f"Frame too short {len(fr)} bytes")
        else:
            # Decode header
            msgNum = fr[0]
            msg.setMsgNum(msgNum)
            protocol = fr[1] & 0x3f
            msg.setProtocol(protocol)
            dirn = fr[1] >> 6
            msg.setDirection(dirn)
            if protocol == PROTOCOL_RICREST:
                restElemCode = fr[2]
                msg.setRESTType(restElemCode)
                if restElemCode == RICREST_ELEM_CODE_URL or restElemCode == RICREST_ELEM_CODE_JSON:
                    msg.setPayload(True, fr[3:].decode('ascii'))
                else:
                    msg.setPayload(False, fr[3:])
                # print(f"RICREST {dirnStrs[dirn]} msgNum {msgNum} {fr[3:].decode('ascii')}")
            elif protocol == PROTOCOL_ROSSERIAL:
                print("ROSSerial")
                # elapTime = time.time() - servoStatusPubStartTime
                # print(f"ROSSERIAL {dirnStrs[dirn]} rate {servoStatusPubPerSec:0.1f}/s msgNum {msgNum}", ''.join('{:02x}'.format(x) for x in fr[2:]))
                # servoStatusPubCount += 1
                # if elapTime > 1.0:
                #     servoStatusPubPerSec = servoStatusPubCount / elapTime
                #     servoStatusPubStartTime = time.time()
                #     servoStatusPubCount = 0
                #     print(f"ROSSERIAL {dirnStrs[dirn]} rate {servoStatusPubPerSec:0.1f}/s")
            else:
                print("Unknown frame received", fr)
        return msg

    def encodeRICFrameRICREST(self, fr, protocolID, dirn, ricRESTElemCode):
        outFrame = bytearray(len(fr)+3)
        outFrame[0] = self.ricSerialMsgNum
        self.ricSerialMsgNum += 1
        if self.ricSerialMsgNum > 255:
            self.ricSerialMsgNum = 1
        outFrame[1] = (dirn << 6) | protocolID
        outFrame[2] = ricRESTElemCode
        outFrame[3:len(fr)+3] = fr
        return outFrame
        
