import time
from martypy import RICCommsWiFi, RICInterface, RICProtocols
import logging
import random

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

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
time.sleep(1)
mcListResp = ric.sendRICRESTCmdFrameSync('{"cmdName":"getStatus"}')
print(mcListResp)

# Set machines alternately
# for mc in mcListResp.get('machineList',[]):
#     ric.sendRICRESTCmdFrameSync('{"cmdName":"SetMcJson"}\0{"name":"' + mc + '"}')
#     curMachine = mc
#     ric.sendRICRESTCmdFrameSync('{"cmdName":"getStatus"}')

# Test data
testWriteAddr = 0x8000
testWriteData = bytearray(b"\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55")

mc = "Serial Terminal ANSI"
logger.debug(f"Setting machine {mc}")
resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"SetMcJson"}', '{"name":"' + mc + '"}')
if resp.get("cmdName","") == "SetMcJsonResp" and resp.get("rslt","") == "ok":
    logger.debug(f"SetMcJson failed {resp}")
assert(resp.get("rslt","") == "ok")

# Set processor clock
logger.debug(f"Setting processor clock")
resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"clockHzSet","clockHz":250000}')
logger.debug(f"Set processor clock resp {resp}")
assert(resp.get("rslt","") == "ok")

# Memory tests
# commonTest.logger.debug(f"Send blockWrite {i}")
for i in range(100):
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"testWrRd","addr":"{testWriteAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}", testWriteData)
    assert(resp.get("rslt","") == "ok")

    # resp = ric.sendRICRESTCmdFrameSync(
    #         "{" + f'"cmdName":"Wr","addr":"{testWriteAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}", testWriteData)
    # assert(resp.get("rslt","") == "ok")
    # print(resp)
    # commonTest.logger.debug(f"Send blockRead {i}")
    # resp = ric.sendRICRESTCmdFrameSync("{" + f'"cmdName":"Rd","addr":"{testWriteAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}")
    # if resp.get("rslt", "") == "ok":
    #     a = resp.get("data", "")
    # print(resp)
    # if not testStats["msgRdOk"]:
    #     break
    # Check data
    expectedResp = ''.join(f'{x:02x}' for x in testWriteData)
    assert(expectedResp == resp.get("data",""))
    # Change test data for next time
    testWriteData = bytearray(len(testWriteData))
    for i in range(len(testWriteData)):
        testWriteData[i] = random.randint(0,255)

while True:
    pass
