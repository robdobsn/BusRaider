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
argparser = argparse.ArgumentParser(description='ReadRam from BusRaider')
argparser.add_argument('address', help='Address to read from, hex number')
argparser.add_argument('length', help='Number of bytes to read, hex number')
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

# Address and length
addr = int(args.address, 16)
length = int(args.length, 16)

# Read from BusRaider
resp = ric.sendRICRESTCmdFrameSync(
        "{" + f'"cmdName":"Rd","addr":"{addr:04x}","lenDec":{length},"isIo":0' + "}")
    # expectedResp = ''.join(f'{x:02x}' for x in testWriteData)
    # assert(expectedResp == resp.get("data",""))
    # # Change test data for next time
    # testWriteData = bytearray(len(testWriteData))
    # for i in range(len(testWriteData)):
    #     testWriteData[i] = random.randint(0,255)
print(resp)
