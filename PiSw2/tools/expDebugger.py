from os import wait
import time
import logging
import random
import argparse
import configparser
from martypy import RICCommsWiFi, RICInterface, RICProtocols, RICCommsSerial

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

def decodedMsg(msg: RICProtocols.DecodedMsg, ricIf):
    print(msg.payload, ricIf)

def logLine(msg):
    print(msg)

# Handle arguments
argparser = argparse.ArgumentParser(description='BusRaider Debug Explorer')
argparser.add_argument('--configfile', help='Config ini file', default='devconfig.ini')
argparser.add_argument('--configsection', help='Config file section', default='DEFAULT')
args, _ = argparser.parse_known_args()

# Handle config
config = configparser.ConfigParser()
config.read(args.configfile)

# Extract config
print(config.sections)
fullConf = config[args.configsection]
logConfig = {"logfolder": fullConf.get("logfolder", "./log")}
ifConfig = {
    "ifType": fullConf.get("ifType", "websocket"),
    "wsURL": fullConf.get("wsURL", ""),
    "ipAddrOrHostname": fullConf.get("ipAddr", ""),
    "serialPort": fullConf.get("serialPort", ""),
    "serialBaud": fullConf.get("serialBaud", ""),
}

# Connection
if ifConfig.get("ifType","") == "websocket":
    ricComms = RICCommsWiFi()
else:
    ricComms = RICCommsSerial()
ric = RICInterface.RICInterface(ricComms)
ric.open(ifConfig)
ric.setDecodedMsgCB(decodedMsg)
ric.setLogLineCB(logLine)

def waitUntilHeld():
    resp = {}
    for i in range(500):
        resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"debugStatus"}')
        if resp.get('rslt',"") != "ok":
            logger.error(f"Invalid response {resp}")
        elif resp.get('held', 0) != 0:
            break
    if resp.get('held', 0) == 0:
        logger.error(f"Failed to hold Z80 processor")
        exit()

def validateRegsAndRam(stepIdx, regsResp):
    # waitUntilHeld()
    # regsResp = ric.sendRICRESTCmdFrameSync('{"cmdName":"debugRegsJSON"}')
    # if regsResp.get('rslt',"") != "ok":
    #     logger.error(f"Invalid response {regsResp}")
    # logger.info(regsResp)
    addr = 0x0100
    length = 2
    memResp = ric.sendRICRESTCmdFrameSync("{" + f'"cmdName":"Rd","addr":"{addr:04x}","lenDec":{length},"isIo":0' + "}")
    if memResp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {memResp}")
    # logger.info(memResp)

    # Validate based on step
    pc = int(regsResp.get("PC", 0), base=16)
    expPC = 0 if stepIdx == 0 else 3 if (stepIdx-1)%3 == 0 else 6 if (stepIdx-1)%3 == 1 else 7
    if expPC != pc:
        print(f"ERROR IN PC expected {expPC} got {pc}")
    hl = int(regsResp.get("HL", 0), base=16)
    expHL = -1 if stepIdx == 0 else (stepIdx)//3+6
    if expHL != -1 and expHL != hl:
        print(f"ERROR IN HL expected {expHL} got {hl} stepIdx {stepIdx}")
    expMem = -1 if stepIdx < 2 else (stepIdx-2)//3+6
    memVal = int(memResp.get("data","0000"), 16)
    memVal = (memVal % 256) * 256 + (memVal // 256) % 256
    if expMem != -1 and expMem != memVal:
        print(f"ERROR IN Memory expected {expMem} got {memVal} stepIdx {stepIdx}")


# Test data - ld hl,0006 / ld (0100),hl / inc hl / jmp 0003
testWriteData = b"\x21\x06\x00\x22\x00\x01\x23\xc3\x03\x00"
testWriteLen = bytes(str(len(testWriteData)),'utf-8')
testWriteAddr = 0

# Clear programmer (so that reset and debug will occur without writing anything else)
resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"imagerClear"' + "}")
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

# Send program to programmer
resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"imagerAdd","addr":"{testWriteAddr:04x}"' + "}",
        testWriteData)
assert(resp.get("rslt","") == "ok")

# # Exec and turn on debugging
# resp = ric.sendRICRESTCmdFrameSync(
#         "{" + f'"cmdName":"imagerWrite","exec":1' + "}")
# if resp.get('rslt',"") != "ok":
#     logger.error(f"Invalid response {resp}")

# Exec and turn on debugging
resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"imagerWrite","exec":1,"debug":1}')
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

waitUntilHeld()
regsResp = ric.sendRICRESTCmdFrameSync('{"cmdName":"debugRegsJSON"}')
if regsResp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {regsResp}")

validateRegsAndRam(0, regsResp)

for i in range(100):
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"debugStepIn"' + "}")
    if resp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {resp}")
    validateRegsAndRam(i+1, resp)
    # logger.info(resp)

time.sleep(1)

resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"debugContinue"' + "}")
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

