from typing import Callable, Dict, List, Optional, Union
import logging
import time
from .ClientGeneric import ClientGeneric
from .RICCommsSerial import RICCommsSerial
from .RICCommsWiFi import RICCommsWiFi
from .RICCommsTest import RICCommsTest
from .RICProtocols import DecodedMsg, RICProtocols
from .RICROSSerial import RICROSSerial
from .RICInterface import RICInterface
from .RICHWElems import RICHWElems
from .Exceptions import (MartyConnectException,
                         MartyCommandException)

logger = logging.getLogger(__name__)

class ClientMV2(ClientGeneric):
    '''
    Lower level connector to Marty V2
    '''
    def __init__(self, 
                method: str, 
                locator: str,
                serialBaud: int = None, 
                port = 80,
                wsPath = "/ws",
                subscribeRateHz = 10.0,
                *args, **kwargs):
        '''
        Initialise connection to remote Marty
        Args:
            client_type: 'wifi' (for WiFi), 'usb' (for usb serial), 'exp' (for expansion serial),
                'test' (output is available via get_test_output())
            locator: str, ipAddress, hostname, serial-port, name of test file etc 
                    depending on method
            serialBaud: serial baud rate
            port: IP port for websockets
            wsPath: path to use for websocket connection
            subscribeRateHz: rate of fastest subscription to events
        Raises:
            MartyConnectException if the connection to the host failed
        '''
        # Initialise vars
        self.subscribeRateHz = subscribeRateHz
        self.lastSubscribedMsgTime = None
        self.maxTimeBetweenPubs = 10
        self.ricHardware = RICHWElems()
        self.isClosing = False
        self.ricSystemInfo = {}
        self.ricHwElemsInfoByIDNo = {}
        self.ricHwElemsList = []
        self.loggingCallback = None
        self._initComplete = False

        # Handle the method of connection
        if method == "usb" or method == "exp":
            ifType = "overascii" if method == "usb" else "plain"
            if serialBaud is None:
                serialBaud = 115200 if method == "usb" else 921600
            rifConfig = {
                "serialPort": locator,
                "serialBaud": serialBaud,
                "ifType": ifType,
            }
            self.ricIF = RICInterface(RICCommsSerial())
        elif method == "test":
            rifConfig = {
                "testFileName": locator
            }
            self.ricIF = RICInterface(RICCommsTest())
        else:
            rifConfig = {
                "ipAddrOrHostname": locator,
                "ipPort": port,
                "wsPath": wsPath,
                "ifType": "plain"
            }
            self.ricIF = RICInterface(RICCommsWiFi())

        # Open comms
        try:
            openOk = self.ricIF.open(rifConfig)
            if not openOk:
                raise MartyConnectException("Failed to open connection")
        except Exception as excp:
            raise MartyConnectException(str(excp))

        # Callbacks
        self.ricIF.setDecodedMsgCB(self._rxDecodedMsg)
        self.ricIF.setTimerCB(self._msgTimerCallback)
        self.ricIF.setLogLineCB(self._logDebugMsg)

    def start(self):
        self.ricSystemInfo = self.ricIF.cmdRICRESTURLSync("v")
        self._updateHwElemsInfo()
        self._initComplete = True

    def close(self):
        if self.isClosing:
            return
        self.isClosing = True
        # Stop any publishing messages
        self.ricIF.sendRICRESTCmdFrame('{"cmdName":"subscription","action":"update",' + \
                '"pubRecs":[' + \
                    '{"name":"MultiStatus","rateHz":0},' + \
                    '{"name":"PowerStatus","rateHz":0},' + \
                    '{"name":"AddOnStatus","rateHz":0}' + \
                ']}\0')
        # Allow message to be sent
        time.sleep(0.5)
        # Close the RIC interface
        self.ricIF.close()

    def hello(self) -> bool:
        return self.ricIF.cmdRICRESTRslt("traj/getReady")

    def get_ready(self) -> bool:
        return self.ricIF.cmdRICRESTRslt("traj/getReady")

    def discover(self) -> List[str]:
        return []

    def stop(self, stop_type: str, stopCode: int) -> bool:
        robotRestCmd = "stop"
        trajCmd = ""
        if stopCode == 0: robotRestCmd = "stopAfterMove"
        elif stopCode == 2: robotRestCmd = "panic"
        elif stopCode == 3: trajCmd = "getReady"
        elif stopCode == 4 or stopCode == 5: robotRestCmd = "pause"
        isOk = self.ricIF.cmdRICRESTRslt("robot/" + robotRestCmd)
        if len(trajCmd) > 0:
            self.ricIF.cmdRICRESTRslt("traj/" + trajCmd)
        return isOk

    def resume(self) -> bool:
        return self.ricIF.cmdRICRESTRslt("robot/resume")
    
    def hold_position(self, hold_time: int) -> bool:
        return self.ricIF.cmdRICRESTRslt(f"traj/hold?move_time={hold_time}")

    def move_joint(self, joint_id: int, position: int, move_time: int) -> bool:
        return self.ricIF.cmdRICRESTRslt(f"traj/joint?jointID={joint_id}&angle={position}&moveTime={move_time}")

    def get_joint_position(self, joint_id: Union[int, str]) -> float:
        return self.ricHardware.getServoPos(joint_id, self.ricHwElemsInfoByIDNo)

    def get_joint_current(self, joint_id: Union[int, str]) -> float:
        return self.ricHardware.getServoCurrent(joint_id, self.ricHwElemsInfoByIDNo)

    def get_joint_status(self, joint_id: Union[int, str]) -> int:
        return self.ricHardware.getServoFlags(joint_id, self.ricHwElemsInfoByIDNo)

    def lean(self, direction: str, amount: int, move_time: int) -> bool:
        try:
            directionNum = ClientGeneric.SIDE_CODES[direction]
        except KeyError:
            self.preException(True)
            raise MartyCommandException("Direction must be one of {}, not '{}'"
                                        "".format(set(ClientGeneric.SIDE_CODES.keys()), direction))
        return self.ricIF.cmdRICRESTRslt(f"traj/lean?side={directionNum}&leanAngle={amount}&moveTime={move_time}")

    def walk(self, num_steps: int = 2, start_foot:str = 'auto', turn: int = 0, 
                step_length:int = 40, move_time: int = 1500) -> bool:
        try:
            sideNum = ClientGeneric.SIDE_CODES[start_foot]
        except KeyError:
            raise MartyCommandException("Direction must be one of {}, not '{}'"
                                        "".format(set(ClientGeneric.SIDE_CODES.keys()), start_foot))
        return self.ricIF.cmdRICRESTRslt(f"traj/step/{num_steps}?side={sideNum}&stepLength={step_length}&turn={turn}&moveTime={move_time}")

    def eyes(self, joint_id: int, pose_or_angle: Union[str, int], move_time: int = 100) -> bool:
        if type(pose_or_angle) is str:
            try:
                eyesTrajectory = ClientGeneric.EYE_POSES[pose_or_angle]
            except KeyError:
                raise MartyCommandException("pose must be one of {}, not '{}'"
                                            "".format(set(ClientGeneric.EYE_POSES.keys()), pose_or_angle))
            return self.ricIF.cmdRICRESTRslt(f"traj/{eyesTrajectory}")
        return self.move_joint(joint_id, pose_or_angle, move_time)

    def kick(self, side: str = 'right', twist: int = 0, move_time: int = 2000) -> bool:
        if side != 'right' and side != 'left':
            raise MartyCommandException("side must be one of 'right' or 'left', not '{}'"
                                        "".format(side))
        return self.ricIF.cmdRICRESTRslt(f"traj/kick?side={ClientGeneric.SIDE_CODES[side]}&moveTime={move_time}")

    def arms(self, left_angle: int, right_angle: int, move_time: int) -> bool:
        self.move_joint(6, left_angle, move_time)
        return self.move_joint(7, right_angle, move_time)

    def celebrate(self, move_time: int = 4000) -> bool:

        # TODO - add celebrate trajectory to Marty V2
        
        return self.ricIF.cmdRICRESTRslt("traj/celebrate")

    def circle_dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        if side != 'right' and side != 'left':
            raise MartyCommandException("side must be one of 'right' or 'left', not '{}'"
                                        "".format(side))
        return self.ricIF.cmdRICRESTRslt(f"traj/circle?side={ClientGeneric.SIDE_CODES[side]}&moveTime={move_time}")

    def dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        if side != 'right' and side != 'left':
            raise MartyCommandException("side must be one of 'right' or 'left', not '{}'"
                                        "".format(side))
        return self.ricIF.cmdRICRESTRslt(f"traj/dance?side={ClientGeneric.SIDE_CODES[side]}&moveTime={move_time}")

    def wiggle(self, move_time: int = 1500) -> bool:
        return self.ricIF.cmdRICRESTRslt(f"traj/wiggle?moveTime={move_time}")

    def sidestep(self, side: str, steps: int = 1, step_length: int = 100, 
            move_time: int = 2000) -> bool:
        if side != 'right' and side != 'left':
            raise MartyCommandException("side must be one of 'right' or 'left', not '{}'"
                                        "".format(side))
        return self.ricIF.cmdRICRESTRslt("traj/sidestep?side={ClientGeneric.SIDE_CODES[side]}&stepLength={step_length}&moveTime={move_time}")

    def play_sound(self, name_or_freq_start: Union[str,float], 
            freq_end: Optional[float] = None, 
            duration: Optional[int] = None) -> bool:
        if not name_or_freq_start.lower().endswith(".raw"):
                name_or_freq_start += ".raw"
        return self.ricIF.cmdRICRESTRslt(f"filerun/{name_or_freq_start}")

    def pinmode_gpio(self, gpio: int, mode: str) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def write_gpio(self, gpio: int, value: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def digitalread_gpio(self, gpio: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def i2c_write(self, *byte_array: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def i2c_write_to_ric(self, address: int, byte_array: bytes) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_battery_voltage(self) -> float:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_battery_remaining(self) -> float:
        powerStatus = self.ricHardware.getPowerStatus()
        return powerStatus.get("remCapPC", 0)

    def get_distance_sensor(self) -> float:
        # TODO NotImplemented
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_accelerometer(self, axis: Optional[str] = None, axisCode: int = 0) -> float:
        if axis is None:
            return self.ricHardware.getIMUAll()
        return self.ricHardware.getIMUAxisValue(axisCode)

    def enable_motors(self, enable: bool = True, clear_queue: bool = True) -> bool:
        return True
                  
    def enable_safeties(self, enable: bool = True) -> bool:
        return True

    def fall_protection(self, enable: bool = True) -> bool:
        return True

    def motor_protection(self, enable: bool = True) -> bool:
        return True

    def battery_protection(self, enable: bool = True) -> bool:
        return True

    def buzz_prevention(self, enable: bool = True) -> bool:
        return True

    def lifelike_behaviour(self, enable: bool = True) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def set_parameter(self, *byte_array: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def save_calibration(self) -> bool:
        return self.ricIF.cmdRICRESTRslt(f"calibrate/setFlag/1")

    def clear_calibration(self) -> bool:
        return self.ricIF.cmdRICRESTRslt(f"calibrate/setFlag/0")

    def is_calibrated(self) -> bool:
        result = self.ricIF.cmdRICRESTURLSync("calibrate")
        if result.get("rslt", "") == "ok":
            return result.get("calDone", 0) != 0
        return False

    def ros_command(self, *byte_array: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def keyframe (self, time: float, num_of_msgs: int, msgs) -> List[bytes]:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_chatter(self) -> bytes:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_firmware_version(self) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def _mute_serial(self) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def ros_serial_formatter(self, topicID: int, send: bool = False, *message: int) -> List[int]:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def is_moving(self) -> bool:
        return self.ricHardware.getIsMoving()

    def is_paused(self) -> bool:
        return self.ricHardware.getIsPaused()

    def get_robot_status(self) -> Dict:
        return self.ricHardware.getRobotStatus()

    def get_joints(self) -> Dict:
        return self.ricHardware.getServos(self.ricHwElemsInfoByIDNo)

    def get_power_status(self) -> Dict:
        return self.ricHardware.getPowerStatus()

    def get_add_ons_status(self) -> Dict:
        return self.ricHardware.getAddOns(self.ricHwElemsInfoByIDNo)

    def get_add_on_status(self, add_on_name_or_id: Union[int, str]) -> Dict:
        return self.ricHardware.getAddOn(add_on_name_or_id, self.ricHwElemsInfoByIDNo)

    def get_system_info(self) -> Dict:
        return self.ricSystemInfo

    def set_marty_name(self, name: str) -> bool:
        escapedName = name.replace('"', '').replace('\n','')
        return self.ricIF.cmdRICRESTRslt(f"friendlyname/{name}")

    def get_marty_name(self) -> str:
        result = self.ricIF.cmdRICRESTURLSync("friendlyname")
        if result.get("rslt", "") == "ok":
            return result.get("friendlyName", "Marty")
        return "Marty"

    def is_marty_name_set(self) -> bool:
        result = self.ricIF.cmdRICRESTURLSync("friendlyname")
        if result.get("rslt", "") == "ok":
            return result.get("friendlyNameIsSet", 0) != 0
        return False

    def get_hw_elems_list(self) -> List:
        self._updateHwElemsInfo()
        return self.ricHwElemsList

    def send_ric_rest_cmd(self, ricRestCmd: str) -> None:
        self.ricIF.sendRICRESTURL(ricRestCmd)

    def send_ric_rest_cmd_sync(self, ricRestCmd: str) -> Dict:
        return self.ricIF.cmdRICRESTURLSync(ricRestCmd)

    def register_logging_callback(self, loggingCallback: Callable[[str],None]) -> None:
        self.loggingCallback = loggingCallback

    def get_interface_stats(self) -> Dict:
        return self.ricIF.getStats()

    def preException(self, isFatal: bool) -> None:
        if isFatal:
            self.ricIF.close()
        logger.debug(f"Pre-exception isFatal {isFatal}")

    def _rxDecodedMsg(self, decodedMsg: DecodedMsg, interface: RICInterface):
        if decodedMsg.protocolID == RICProtocols.PROTOCOL_ROSSERIAL:
            # logger.debug(f"ROSSERIAL message received {len(decodedMsg.payload)}")
            self.lastSubscribedMsgTime = time.time()
            if decodedMsg.payload:
                RICROSSerial.decode(decodedMsg.payload, 0, self.ricHardware.updateWithROSSerialMsg)
        elif decodedMsg.protocolID == RICProtocols.PROTOCOL_RICREST:
            # logger.debug(f"RIC REST message received {decodedMsg.payload}")
            pass

    def _logDebugMsg(self, logMsg: str) -> None:
        if self.loggingCallback:
            self.loggingCallback(logMsg)

    def _msgTimerCallback(self) -> None:
        if self.isClosing:
            return
        if (self._initComplete and self.subscribeRateHz != 0) and \
                 (self.lastSubscribedMsgTime is None or \
                    time.time() > self.lastSubscribedMsgTime + self.maxTimeBetweenPubs):
            # Subscribe for publication messages
            self.ricIF.sendRICRESTCmdFrame('{"cmdName":"subscription","action":"update",' + \
                            '"pubRecs":[' + \
         '{' + f'"name":"MultiStatus","rateHz":{self.subscribeRateHz},' + '}' + \
                                '{"name":"PowerStatus","rateHz":1.0},' + \
                                '{' + f'"name":"AddOnStatus","rateHz":{self.subscribeRateHz}' + '}' + \
                            ']}\0')
            self.lastSubscribedMsgTime = time.time()

    def _updateHwElemsInfo(self):
        hwElemsInfo = self.ricIF.cmdRICRESTURLSync("hwstatus")
        if hwElemsInfo.get("rslt", "") == "ok":
            self.ricHwElemsList = hwElemsInfo.get("hw", [])
            self.ricHwElemsInfoByIDNo = {}
            for el in self.ricHwElemsList:
                if "IDNo" in el:
                    self.ricHwElemsInfoByIDNo[el["IDNo"]] = el

    def get_test_output(self) -> dict:
        return self.ricIF.getTestOutput()
