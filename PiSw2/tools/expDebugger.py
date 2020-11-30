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

# Turn debugging on and off repeatedly
for i in range(100000):
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"debugBreak"' + "}")
    if resp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {resp}")
    resp = ric.sendRICRESTCmdFrameSync(
            "{" + f'"cmdName":"debugRun"' + "}")
    if resp.get('rslt',"") != "ok":
        logger.error(f"Invalid response {resp}")

