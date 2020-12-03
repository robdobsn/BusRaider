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

# Test data - load A,6; inc A * 3 and jump to 0002
testWriteData = b"\x3e\x06\x3c\x3c\x3c\xc3\x02\x00"
testWriteLen = bytes(str(len(testWriteData)),'utf-8')
testWriteAddr = 0

# Clear programmer (so that reset and debug will occur without writing anything else)
resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"progClear"' + "}")
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

# Send program to programmer
resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"progAdd","addr":"{testWriteAddr:04x}"' + "}",
        testWriteData)
assert(resp.get("rslt","") == "ok")

# # Exec and turn on debugging
# resp = ric.sendRICRESTCmdFrameSync(
#         "{" + f'"cmdName":"progWrite","exec":1' + "}")
# if resp.get('rslt',"") != "ok":
#     logger.error(f"Invalid response {resp}")

# Exec and turn on debugging
resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"progWrite","exec":1,"debug":1}')
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

waitUntilHeld()

resp = ric.sendRICRESTCmdFrameSync('{"cmdName":"debugRegsFormatted"}')
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")
logger.info(resp)

for i in range(100):
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"debugStepIn"' + "}")
    if resp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {resp}")
    waitUntilHeld()
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"debugRegsFormatted"' + "}")
    if resp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {resp}")
    logger.info(resp)

time.sleep(1)

resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"debugContinue"' + "}")
if resp.get('rslt',"") != "ok":
    logger.error(f"Invalid response {resp}")

