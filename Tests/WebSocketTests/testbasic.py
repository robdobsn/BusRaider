import time
from martypy import RICCommsWiFi, RICInterface, RICProtocols

def decodedMsg(msg: RICProtocols.DecodedMsg, ricIf):
    print(msg.payload, ricIf)


def logLine(msg):
    print(msg)

ricComms = RICCommsWiFi()
ric = RICInterface.RICInterface(ricComms)
ric.open({"ipAddrOrHostname":"192.168.86.7"})
ric.setDecodedMsgCB(decodedMsg)
ric.setLogLineCB(logLine)
# ric.sendRICRESTURL("v")
# ric.sendRICRESTURL("queryESPHealth")
# ric.sendRICRESTURL("querystatus")
mcListResp = ric.sendRICRESTCmdFrameSync('{"cmdName":"getStatus"}')
time.sleep(10)