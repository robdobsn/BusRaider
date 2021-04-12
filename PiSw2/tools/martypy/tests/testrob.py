import time
import logging
import os
import sys
import json
from martypy import Marty

jointNames = [
    'left hip',
    'left twist',
    'left knee',
    'right hip',
    'right twist',
    'right knee',
    'left arm',
    'right arm', 
    'eyes'   
]

def betweenCommands():
    time.sleep(3)

def testBoolCmd(cmdStr: str, cmdRslt: bool):
    logger.info(f"{cmdStr}, rslt = {cmdRslt}")
    betweenCommands()

def loggingCB(logStr: str) -> None:
    logger.debug(logStr)

# Check the log folder exists
logsFolder = "logs"
if not os.path.exists(logsFolder):
    os.mkdir(logsFolder)

# Log file name
logFileName = "MartyPyLogTest_" + time.strftime("%Y%m%d-%H%M%S") + ".log"
logFileName = os.path.join(logsFolder, logFileName)
print("Logging to file " + logFileName)

# Setup logging
logging.basicConfig(filename=logFileName, format='%(levelname)s: %(asctime)s %(funcName)s(%(lineno)d) -- %(message)s', level=logging.DEBUG)
logger = logging.getLogger("MartyPyTest")
handler = logging.StreamHandler(sys.stdout)
handler.setLevel(logging.DEBUG)
logger.addHandler(handler)

mymarty = None
try:
    # mymarty = Marty('wifi', '192.168.86.11')
    # mymarty = Marty('wifi', '192.168.86.11', subscribeRateHz=0)
    mymarty = Marty('socket://192.168.86.41')
    # mymarty = Marty("usb", "COM9", debug=True)
    # mymarty = Marty('usb:///dev/tty.SLAB_USBtoUART', debug=True)
except Exception as excp:
    logger.debug(f"Couldn't connect to marty {excp}")
    exit()

mymarty.register_logging_callback(loggingCB)

martySysInfo = mymarty.get_system_info()
martyVersion2 = martySysInfo.get("HardwareVersion", "1.0") == "2.0"

if martyVersion2:
    logger.info(f"Marty has {len(mymarty.get_hw_elems_list())} hardware parts")

if martyVersion2:
    logger.info(f"Calibration flag {mymarty.is_calibrated()}")
testBoolCmd("Calibration flag clear", mymarty.clear_calibration())
if martyVersion2:
    logger.info(f"Calibration flag should be False ... {mymarty.is_calibrated()}")
    assert not mymarty.is_calibrated()
testBoolCmd("Calibration flag set", mymarty.save_calibration())
if martyVersion2:
    logger.info(f"Calibration flag should be True ... {mymarty.is_calibrated()}")
time.sleep(0.1)
if martyVersion2:
    assert mymarty.is_calibrated()

logger.info(f"Marty interface stats {json.dumps(mymarty.get_interface_stats())}")

testBoolCmd("Get ready", mymarty.get_ready())
testBoolCmd("Circle Dance", mymarty.circle_dance())
testBoolCmd("Eyes excited", mymarty.eyes('excited'))
testBoolCmd("Eyes wide", mymarty.eyes('wide'))
testBoolCmd("Eyes angry", mymarty.eyes('angry'))
testBoolCmd("Eyes normal", mymarty.eyes('normal'))
testBoolCmd("Kick left", mymarty.kick('left'))
testBoolCmd("Kick right", mymarty.kick('right'))
testBoolCmd("Stop", mymarty.stop())
testBoolCmd("Arms 45", mymarty.arms(45, 45, 500))
testBoolCmd("Arms 0", mymarty.arms(0, 0, 500))

testBoolCmd("Arms 0", mymarty.play_sound("disbelief"))
testBoolCmd("Arms 0", mymarty.play_sound("excited"))
testBoolCmd("Arms 0", mymarty.play_sound("screenfree"))

logger.info(f"Marty interface stats {json.dumps(mymarty.get_interface_stats())}")

for i in range(9): 
    testBoolCmd(f"Move joint {i}", mymarty.move_joint(i, i * 10, 500))
for jointName in jointNames: 
    testBoolCmd(f"Move joint {jointName}", mymarty.move_joint(jointName, 123, 500))

logger.info(f"Accelerometer x {mymarty.get_accelerometer('x')}")
logger.info(f"Accelerometer y { mymarty.get_accelerometer('y')}")
logger.info(f"Accelerometer z { mymarty.get_accelerometer('z')}")

if martyVersion2:
    testBoolCmd("Dance", mymarty.dance())
    testBoolCmd("Eyes wiggle", mymarty.eyes('wiggle'))
    testBoolCmd("Hold position", mymarty.hold_position(6000))
    testBoolCmd("is_moving", mymarty.is_moving())
    logger.info("Joint positions: ", [mymarty.get_joint_position(pos) for pos in range(9)])

time.sleep(5)

mymarty.close()
