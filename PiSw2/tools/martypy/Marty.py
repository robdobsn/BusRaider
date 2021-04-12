'''
Python library to communicate with Marty the Robot V1 and V2 by Robotical

Getting started:  
1) Import Marty from the martypy library  
2) Create a Marty object that connects the way you want  
3) Tell your Marty to dance  

```python
from martypy import Marty  
my_marty = Marty("wifi","192.168.0.53")  
my_marty.dance()
```

The emojis :one: and :two: indicate when the method is available for Marty V1 :one: and Marty V2 :two:
'''
from typing import Callable, Dict, List, Optional, Union
from .ClientGeneric import ClientGeneric
from .ClientMV2 import ClientMV2
from .ClientMV1 import ClientMV1
from .Exceptions import (MartyCommandException,
                         MartyConfigException)

class Marty(object):

    CLIENT_TYPES = {
        'socket' : ClientMV1,
        'exp'    : ClientMV2,
        'usb'    : ClientMV2,
        'wifi'   : ClientMV2,
        'test'   : ClientMV2,
    }

    STOP_TYPE = {
        'clear queue'       : 0, # clear movement queue only (so finish the current movement)
        'clear and stop'    : 1, # clear movement queue and servo queues (freeze in-place)
        'clear and disable' : 2, # clear everything and disable motors
        'clear and zero'    : 3, # clear everything, and make robot return to zero
        'pause'             : 4, # pause, but keep servo and movequeue intact and motors enabled
        'pause and disable' : 5, # as 4, but disable motors too
    }

    JOINT_IDS = {
        'left hip'       : 0,
        'left twist'     : 1,
        'left knee'      : 2,
        'right hip'      : 3,
        'right twist'    : 4,
        'right knee'     : 5,
        'left arm'       : 6,
        'right arm'      : 7,
        'eyes'          : 8        
    }

    JOINT_STATUS = {
        'enabled'           : 0x01,
        'current limit now' : 0x02,
        'current limit long': 0x04,
        'busy'              : 0x08,
        'pos restricted'    : 0x10,
        'paused'            : 0x20,
        'comms ok'          : 0x80
    }

    ACCEL_AXES = {
        'x' : 0,
        'y' : 1,
        'z' : 2,
    }

    HW_ELEM_TYPES = [
        "SmartServo",
        "IMU",
        "I2SOut",
        "BusPixels",
        "GPIO",
        "FuelGauge",
        "PowerCtrl"
        ]

    ADD_ON_TYPE_NAMES = [
        "IRFoot"
    ]

    def __init__(self, 
                method: str,
                locator: str = "",
                extra_client_types: dict = dict(),
                *args, **kwargs) -> None:
        '''
        Start a connection to Marty :one: :two:  

        For example:  

            * ```Marty("wifi", "192.168.86.53")``` to connect to Marty via WiFi on IP Address 192.168.0.53  
            * ```Marty("usb", "COM2")``` on a Windows computer with Marty connected by USB cable to COM2  
            * ```Marty("usb", "/dev/tty.SLAB_USBtoUART")``` on a Mac computer with Marty connected by USB cable to /dev/tty.SLAB_USBtoUART  
            * ```Marty("exp", "/dev/ttyAMA0")``` on a Raspberry Pi computer with Marty connected by expansion cable to /dev/ttyAMA0  

        Args:
            method: method of connecting to Marty - it may be: "usb",
                "wifi", "socket" (Marty V1) or "exp" (expansion port used to connect
                to a Raspberry Pi, etc)  
            locator: location to connect to, depending on the method of connection this 
                is the serial port name, network (IP) Address or network name (hostname) of Marty 
                that the computer should use to communicate with Marty

        Raises:
            * MartyConfigException if the parameters are invalid  
            * MartyConnectException if Marty couldn't be contacted
        '''
        # Merge in any extra clients that have been added and check valid
        self.client: ClientGeneric = None
        self.CLIENT_TYPES = ClientGeneric.dict_merge(self.CLIENT_TYPES, extra_client_types)

        # Get and check connection parameters
        if type(method) is not str:
            raise MartyConfigException(f'Method must be one of {self.CLIENT_TYPES.keys()}')
        if '://' in method:
            method, _, locator = method.partition('://')
        if method.lower() not in self.CLIENT_TYPES.keys():
            raise MartyConfigException(f'Unrecognised method "{method}"')

        # Initialise the client class used to communicate with Marty
        self.client = self.CLIENT_TYPES[method.lower()](method.lower(), locator, *args, **kwargs)

        # Get Marty details
        self.client.start()

    def dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        '''
        Boogie, Marty! :one: :two:
        Args:
            side: 'left' or 'right', which side to start on
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.dance(side, move_time)

    def celebrate(self, move_time: int = 4000) -> bool:
        '''
        Do a small celebration :one: :two:
        Args:
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.celebrate(move_time)

    def wiggle(self, move_time: int = 1500) -> bool:
        '''
        Wiggle :two:
        Args:
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.wiggle(move_time)

    def circle_dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        '''
        Circle Dance :two:
        Args:
            side: 'left' or 'right', which side to start on
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.circle_dance(side, move_time)

    def walk(self, num_steps: int = 2, start_foot:str = 'auto', turn: int = 0, 
                step_length:int = 40, move_time: int = 1500) -> bool:
        '''
        Make Marty walk :one: :two:
        Args:
            num_steps: how many steps to take
            start_foot: 'left', 'right' or 'auto', start walking with this foot
            turn: How much to turn (-100 to 100 in degrees), 0 is straight.
            step_length: How far to step (approximately in mm)
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.walk(num_steps, start_foot, turn, step_length, move_time)

    def get_ready(self) -> bool:
        '''
        Move Marty to the normal standing position :one: :two:
        Returns:
            True if Marty accepted the request
        '''
        return self.client.get_ready()

    def eyes(self, pose_or_angle: Union[str, int], move_time: int = 100) -> bool:
        '''
        Move the eyes to a pose or an angle :one: :two:
        Args:
            pose_or_angle: 'angry', 'excited', 'normal', 'wide', or 'wiggle' :two: - alternatively
                           this can be an angle in degrees (which can be a negative number)
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.eyes(Marty.JOINT_IDS['eyes'], pose_or_angle, move_time)

    def kick(self, side: str = 'right', twist: int = 0, move_time: int = 2000) -> bool:
        '''
        Kick one of Marty's feet :one: :two:
        Args:
            side: 'left' or 'right', which foot to use
            twist: the amount of twisting do do while kicking (in degrees)
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.kick(side, twist, move_time)

    def arms(self, left_angle: int, right_angle: int, move_time: int) -> bool:
        '''
        Move both of Marty's arms to angles you specify :one: :two:
        Args:
            left_angle: Angle of the left arm (degrees -100 to 100)
            right_angle: Position of the right arm (degrees -100 to 100)
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.arms(left_angle, right_angle, move_time)

    def lean(self, direction: str, amount: int, move_time: int) -> bool:
        '''
        Lean over in a direction :one: :two:
        Args:
            direction: 'left', 'right', 'forward', 'back', or 'auto'
            amount: percentage amount to lean
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.lean(direction, amount, move_time)

    def sidestep(self, side: str, steps: int = 1, step_length: int = 100, 
            move_time: int = 2000) -> bool:
        '''
        Take sidesteps :one: :two:
        Args:
            side: 'left' or 'right', direction to step in
            steps: number of steps to take
            step_length: how broad the steps are (up to 127)
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        '''
        return self.client.sidestep(side, steps, step_length, move_time)

    def play_sound(self, name_or_freq_start: Union[str,int], 
            freq_end: Optional[int] = None, 
            duration: Optional[int] = None) -> bool:
        '''
        Play a named sound (Marty V2 :two:) or make a tone (Marty V1 :one:)
        Args:
            name_or_freq_start: name of the sound, e.g. 'excited' or 'no_way' :two:
            name_or_freq_start: starting frequency, Hz :one:
            freq_end:  ending frequency, Hz :one:
            duration: milliseconds, maximum 5000 :one:
        Returns:
            True if Marty accepted the request
        '''
        return self.client.play_sound(name_or_freq_start, freq_end, duration)

    def get_accelerometer(self, axis: Optional[str] = None) -> float:
        '''
        Get the latest value from the Marty's accelerometer :one: :two:
        Args:
            axis: (optional) 'x', 'y' or 'z' OR no parameter at all (see returns below)
        Returns:  
            * The acceleration value from the axis (if axis specified)  
            * A tuple containing x, y and z values (if no axis) :two:
            Note that the returned value will be 0 if no value is available
        Raises:
            MartyCommandException if the axis is unknown
        '''
        axisCode = 0
        if (axis is not None) and (type(axis) is str):
            if axis not in self.ACCEL_AXES:
                self.client.preException(True)
                raise MartyCommandException("Axis must be one of {}, not '{}'"
                                            "".format(set(self.ACCEL_AXES.keys()), axis))
            axisCode = self.ACCEL_AXES.get(axis, 0)
        return self.client.get_accelerometer(axis, axisCode)

    def is_moving(self) -> bool:
        '''
        Check if Marty is moving :two:
        Args:
            none
        Returns:
            True if Marty is moving
        '''
        return self.client.is_moving()

    def stop(self, stop_type: Optional[str] = None) -> bool:
        '''
        Stop Marty's movement  :one: :two:  

        You can also control what way to "stop" you want with the parameter stop_type. For instance:  
        
        * 'clear queue' to finish the current movement before stopping (clear any queued movements)
        * 'clear and stop' stop immediately (and clear queues)
        * 'clear and disable' :one: stop and disable the robot
        * 'clear and zero' stop and move back to get_ready pose
        * 'pause' pause motion
        * 'pause and disable' :one: pause motion and disable the robot

        Args:
            stop_type: the way to stop - see the options above

        Raises:
            MartyCommandException if the stop_type is unknown
        '''
        # Default to plain "stop"
        stopCode = 1 
        if stop_type is not None:
            if stop_type not in self.STOP_TYPE:
                self.client.preException(True)
                raise MartyCommandException("Unknown stop_type '{}', not in Marty.STOP_TYPE"
                                            "".format(stop_type))
            stopCode = self.STOP_TYPE.get(stop_type, stopCode)
        return self.client.stop(stop_type, stopCode)

    def resume(self) -> bool:
        '''
        Resume Marty's movement after a pause :two:
        Returns:
            True if Marty accepted the request
        '''
        return self.client.resume()

    def hold_position(self, hold_time: int) -> bool:
        '''
        Hold Marty at its current position :two:
        Args:
            hold_time, time to hold position in milli-seconds
        Returns:
            True if Marty accepted the request
        '''
        # Default to plain "stop"
        return self.client.hold_position(hold_time)

    def is_paused(self) -> bool:
        '''
        Check if Marty is paused :two:
        Returns:
            True if Marty is paused
        '''
        return self.client.is_paused()

    def move_joint(self, joint_name_or_num: Union[int, str], position: int, move_time: int) -> bool:
        '''
        Move a specific joint to a position :one: :two:
        Args:
            joint_name_or_num: joint to move, see the Marty.JOINT_IDS dictionary (can be name or number)
            position: angle in degrees
            move_time: how long this movement should last, in milliseconds
        Returns:
            True if Marty accepted the request
        Raises:
            MartyCommandException if the joint_name_or_num is unknown
        '''
        jointIDNo = joint_name_or_num
        if type(joint_name_or_num) is str:
            if joint_name_or_num not in self.JOINT_IDS:
                self.client.preException(True)
                raise MartyCommandException("Joint must be one of {}, not '{}'"
                                            "".format(set(self.JOINT_IDS.keys()), joint_name_or_num))
            jointIDNo = self.JOINT_IDS.get(joint_name_or_num, 0)
        return self.client.move_joint(jointIDNo, position, move_time)

    def get_joint_position(self, joint_name_or_num: Union[int, str]) -> float:
        '''
        Get the position (angle in degrees) of a joint :two:
        Args:
            joint_name_or_num: see the Marty.JOINT_IDS dictionary (can be name or number)
        Returns:
            Angle of the joint in degrees
        Raises:
            MartyCommandException if the joint_name_or_num is unknown            
        '''
        jointIDNo = joint_name_or_num
        if type(joint_name_or_num) is str:
            if joint_name_or_num not in self.JOINT_IDS:
                self.client.preException(True)
                raise MartyCommandException("Joint must be one of {}, not '{}'"
                                            "".format(set(self.JOINT_IDS.keys()), joint_name_or_num))
            jointIDNo = self.JOINT_IDS.get(joint_name_or_num, 0)
        return self.client.get_joint_position(jointIDNo)

    def get_joint_current(self, joint_name_or_num: Union[int, str]) -> float:
        '''
        Get the current (in milli-Amps) of a joint :one: :two:
        This can be useful in detecting when the joint is working hard and is related
        to the force which the joint's motor is exerting to stay where it is
        Args:
            joint_name_or_num: see the Marty.JOINT_IDS dictionary (can be name or number)
        Returns:
            the current of the joint in milli-Amps (this will be 0 if the joint current is unknown)
        Raises:
            MartyCommandException if the joint_name_or_num is unknown
        '''
        jointIDNo = joint_name_or_num
        if type(joint_name_or_num) is str:
            if joint_name_or_num not in self.JOINT_IDS:
                self.client.preException(True)
                raise MartyCommandException("Joint must be one of {}, not '{}'"
                                            "".format(set(self.JOINT_IDS.keys()), joint_name_or_num))
            jointIDNo = self.JOINT_IDS.get(joint_name_or_num, 0)
        return self.client.get_joint_current(jointIDNo)

    def get_joint_status(self, joint_name_or_num: Union[int, str]) -> int:
        '''
        Get information about a joint :two:
        This can be helpful to find out if the joint is working correctly and if it is
        moving at the moment, etc
        Args:
            joint_name_or_num: see the Marty.JOINT_IDS dictionary (can be name or number)
        Returns:
            a code number which is the sum of codes in the Marty.JOINT_STATUS dictionary
            will be 0 if the joint status is unknown
        Raises:
            MartyCommandException if the joint_name_or_num is unknown
        '''
        jointIDNo = joint_name_or_num
        if type(joint_name_or_num) is str:
            if joint_name_or_num not in self.JOINT_IDS:
                self.client.preException(True)
                raise MartyCommandException("Joint must be one of {}, not '{}'"
                                            "".format(set(self.JOINT_IDS.keys()), joint_name_or_num))
            jointIDNo = self.JOINT_IDS.get(joint_name_or_num, 0)
        return self.client.get_joint_status(jointIDNo)

    def get_distance_sensor(self) -> float:
        '''
        Get the latest value from the distance sensor :one: :two:
        Returns:
            The distance sensor reading (will return 0 if no distance sensor is found)
        '''
        return self.client.distance()

    def get_battery_remaining(self) -> float:
        '''
        Get the battery remaining percentage :two:
        Returns:
            The battery remaining capacity in percent
        '''
        return self.client.get_battery_remaining()

    def save_calibration(self) -> bool:
        '''
        Set the current motor positions as the zero positions :one: :two:
        BE CAREFUL, this can cause unexpected movement or self-interference
        '''
        return self.client.save_calibration()

    def clear_calibration(self) -> bool:
        '''
        Tell Marty to forget it's calibration :one: :two:
        BE CAREFUL, this can cause unexpected movement or self-interference
        '''
        return self.client.clear_calibration()

    def is_calibrated(self) -> bool:
        '''
        Check if Marty is calibrated :two:
        '''
        return self.client.is_calibrated()

    def get_robot_status(self) -> Dict:
        '''
        Get status of Marty the Robot :two:
        Args:
            none
        Returns:
            Dictionary containing:
                "workQCount" number of work items (movements) that are queued up
                "isMoving": True if Marty is moving
                "isPaused": True if Marty is paused
                "isFwUpdating": True if Marty is doing an update
        '''
        return self.client.get_robot_status()

    def get_joints(self) -> Dict: 
        '''
        Get information on all of Marty's joints :two:
        Args:
            none
        Returns:
            Dictionary containing dictionaries (one for each joint) each of which contain:
                "IDNo": the joint identification number (see Marty.JOINT_IDS)
                "name": the name of the joint
                "pos": the angle of the joint
                "current": the joint current (in milli-Amps)
                "enabled": True if the servo is enabled
                "commsOK": True if the servo is communicating ok
                "flags": joint status flags (see Marty.JOINT_STATUS)
        '''
        return self.client.get_joints()

    def get_power_status(self) -> Dict:
        '''
        Get information on Marty's battery and power supply :two:
        Args:
            none
        Returns:
            Dictionary containing:
                "remCapPC" remaining battery capacity in percent
                "tempDegC": battery temperature in degrees C
                "remCapMAH": remaining battery capacity in milli-Amp-Hours
                "fullCapMAH": capacity of the battery when full in milli-Amp-Hours
                "currentMA": current the battery is supplying (or being charged with) milli-Amps
                "power5VOnTimeSecs": number of seconds the power to joints and add-ons has been on
                "isOnUSBPower": True if Marty is running on power from the USB connector
                "is5VOn": True if power to the joints and add-ons is turned on
        '''
        return self.client.get_power_status()
    
    def get_add_ons_status(self) -> Dict:
        '''
        Get latest information for all add-ons :two:
        Args:
            none
        Returns:
            Dictionary containing dictionaries (one for each add-on) each of which contain:
                "IDNo": the add-on identification number
                "name": the name of the add-on
                "type": the type of the add-on (see Marty.ADD_ON_TYPE_NAMES but it may not be in this list)
                "whoAmITypeCode": a code which can be used for further add-on identification
                "valid": True if the data is valid
                "data": 10 bytes of data from the add-on - the format of this data depends
                        on the type of add-on
        '''
        return self.client.get_add_ons_status()

    def get_add_on_status(self, add_on_name_or_id: Union[int, str]) -> Dict:
        '''
        Get latest information for a single add-on :two:
        Args:
            add_on_name_or_id: either the name or the id (number) of an add-on
        Returns:
            Dictionary containing:
                "IDNo": the add-on identification number
                "valid": True if the data is valid
                "data": 10 bytes of data from the add-on - the format of this data depends
                        on the type of add-on
        '''
        return self.client.get_add_on_status(add_on_name_or_id)

    def get_system_info(self) -> Dict:
        '''
        Get information about Marty :two:
        Args:
            none
        Returns:
            Dictionary containing:
                "HardwareVersion": string containing the version of Marty hardware
                                "1.0" for Marty V1
                                "2.0" for Marty V2
                                other values for later versions of Marty
                "SystemName": the name of the physical hardware in Marty - this will be 
                              RicFirmwareESP32 for Marty V2 and
                              MartyV1 for Marty V1
                "SystemVersion": a string in semantic versioning format with the version
                                 of Marty firmware (e.g. "1.2.3")
                "SerialNo": serial number of this Marty
                "MAC": the base MAC address of the Marty
        '''
        return self.client.get_system_info()

    def set_marty_name(self, name: str) -> bool:
        '''
        Set Marty's name :two:
        Args:
            name to call Marty
        Returns:
            True if successful in setting the name
        '''
        return self.client.set_marty_name(name)

    def get_marty_name(self) -> str:
        '''
        Get Marty's name :two:
        Args:
            none
        Returns:
            the name given to Marty
        '''
        return self.client.get_marty_name()

    def is_marty_name_set(self) -> bool:
        '''
        Check if Marty's name is set :two:
        Args:
            none
        Returns:
            True if Marty's name is set
        '''
        return self.client.is_marty_name_set()

    def get_hw_elems_list(self) -> List:
        '''
        Get a list of all of the hardware elements on Marty :two:
        Args:
            none
        Returns:
            List containing a dictionary for each hardware element, each element is in the form:
                "name": name of the hardware element
                "type": type of element, see Marty.HW_ELEM_TYPES, other types may appear as add-ons
                "busName": name of the bus the element is connected to
                "addr": address of the element if it is connected to a bus
                "addrValid": 1 if the address is valid, else 0
                "IDNo": identification number of the element
                "whoAmI": string from the hardware which may contain additional identification
                "whoAmITypeCode": string indicating the type of hardware
                "SN": serial number of the hardware element
                "versionStr": version string of hardware element in semantic versioning (semver) format
                "commsOK": 1 if the element is communicating ok, 0 if not
        '''
        return self.client.get_hw_elems_list()

    def send_ric_rest_cmd(self, ricRestCmd: str) -> None:
        '''
        Send a command in RIC REST format to Marty :two:  

        This is a special purpose command which you can use to do advanced
        control of Marty
        Args:
            ricRestCmd: string containing the command to send to Marty
        Returns:
            None
        '''
        self.client.send_ric_rest_cmd(ricRestCmd)

    def send_ric_rest_cmd_sync(self, ricRestCmd: str) -> Dict:
        '''
        Send a command in RIC REST format to Marty and wait for reply :two:  

        This is a special purpose command which you can use to do advanced
        control of Marty
        Args:
            ricRestCmd: string containing the command to send to Marty
        Returns:
            Dictionary containing the response received from Marty
        '''
        return self.client.send_ric_rest_cmd_sync(ricRestCmd)

    def get_motor_current(self, motor_id: int) -> float:
        '''
        Get current flowing through a joint motor :one: :two:
        Args:
            motor_id, integer >= 0 (non-negative) selects which motor to query
        Returns:
            Instantaneous current sense reading from motor `motor_id`
        '''
        return self.get_joint_current(motor_id)

    ''' 
    ============================================================
    The following commands are for Marty V1 Only
    ============================================================
    '''

    def enable_motors(self, enable: bool = True, clear_queue: bool = True) -> bool:
        '''
        Toggle power to motors :one:
        Args:
            enable: True/False toggle
            clear_queue: Default True, prevents unfinished but 'muted' motions
                         from jumping as soon as motors are enabled
        '''
        return self.client.enable_motors(enable, clear_queue)

    def enable_safeties(self, enable: bool = True) -> bool:
        '''
        Tell the board to turn on 'normal' safeties :one:
        '''
        return self.client.enable_safeties(enable)

    def fall_protection(self, enable: bool = True) -> bool:
        '''
        Toggle fall protections :one:
        Args:
            enable: True/False toggle
        '''
        return self.client.fall_protection(enable)

    def motor_protection(self, enable: bool = True) -> bool:
        '''
        Toggle motor current protections :one:
        Args:
            enable: True/False toggle
        '''
        return self.client.motor_protection(enable)

    def battery_protection(self, enable: bool = True) -> bool:
        '''
        Toggle low battery protections :one:
        Args:
            enable: True/False toggle
        '''
        return self.client.battery_protection(enable)

    def buzz_prevention(self, enable: bool = True) -> bool:
        '''
        Toggle motor buzz prevention :one:
        Args:
            enable: True/False toggle
        '''
        return self.client.buzz_prevention(enable)

    def lifelike_behaviour(self, enable: bool = True) -> bool:
        '''
        Tell the robot whether it can or can't move now and then in a lifelike way when idle. :one:
        Args:
            enable: True/False toggle
        '''
        return self.client.lifelike_behaviour(enable)

    def ros_command(self, *byte_array: int) -> bool:
        '''
        Low level proxied access to the ROS Serial API between 
        the modem and main controller :one:
        '''
        return self.client.ros_command(*byte_array)

    def keyframe (self, time: float, num_of_msgs: int, msgs) -> List[bytes]:
        '''
        Takes in information about movements and generates keyframes
        returns a list of bytes :one:
        Args:
            time: time (in seconds) taken to complete movement
            num_of_msgs: number of commands sent
            msgs: commands sent in the following format [(ID CMD), (ID CMD), etc...]
        '''
        return self.client.keyframe(time, num_of_msgs, msgs)

    def get_chatter(self) -> bytes:
        '''
        Return chatter topic data (variable length) :one:
        '''
        return self.client.get_chatter()

    def get_firmware_version(self) -> bool:
        '''
        Ask the board to print the firmware version over chatter :one:
        '''
        return self.client.get_firmware_version()

    def _mute_serial(self) -> bool:
        '''
        Mutes the internal serial line on RIC. Depends on platform and API :one:
        NOTE: Once you've done this, the Robot will ignore you until you cycle power.
        '''
        return self.client.mute_serial()

    def ros_serial_formatter(self, topicID: int, send: bool = False, *message: int) -> List[int]:
        '''
        Formats message into ROS serial format and
        returns formatted message as a list :one:  

        Calls ros_command with the processed message if send is True.
        More information about the ROS serial format can be
        found here: http://wiki.ros.org/rosserial/Overview/Protocol
        '''
        return self.client.ros_serial_formatter(topicID, send, message)

    def pinmode_gpio(self, gpio: int, mode: str) -> bool:
        '''
        Configure a GPIO pin :one:
        Args:
            gpio: pin number between 0 and 7
            mode: choose from: 'digital in','analog in' or 'digital out'
        '''
        return self.client.pinmode_gpio(gpio, mode)

    def write_gpio(self, gpio:int, value: int) -> bool:
        '''
        Write a value to a GPIO port :one:
        '''
        return self.client.write_gpio(gpio, value)

    def digitalread_gpio(self, gpio: int) -> bool:
        '''
        Read from GPIO :one:
        Args:
            GPIO pin number, >= 0 (non-negative)
        Returns:
            Returns High/Low state of a GPIO pin
        '''
        return self.client.digitalread_gpio(gpio)

    def set_parameter(self, *byte_array: int) -> bool:
        '''
        Set board parameters :one:
        Args:
            byte_array: a list in the following format [paramID, params]
        '''
        return self.client.set_parameter(byte_array)

    def i2c_write(self, *byte_array: int) -> bool:
        '''
        Write a bytestream to the i2c port. :one:  

        The first byte should be the address, following from that
        the datagram folows standard i2c spec
        '''
        return self.client.i2c_write(*byte_array)

    def i2c_write_to_ric(self, address: int, byte_array: bytes) -> bool:
        '''
        Write a formatted bytestream to the i2c port. :one:  

        The bytestream is formatted in the ROS serial format.

        address: the other device's address
        '''
        return self.client.i2c_write_to_ric(address, byte_array)

    def i2c_write_to_rick(self, address: int, byte_array: bytes) -> bool:
        '''
        Write a formatted bytestream to the i2c port. :one:  

        The bytestream is formatted in the ROS serial format.
        address: the other device's address
        '''
        return self.client.i2c_write_to_ric(address, byte_array)

    def get_battery_voltage(self) -> float:
        '''
        Get the voltage of the battery :one:
        Returns:
            The battery voltage reading as a float in Volts
        '''
        return self.client.get_battery_voltage()

    def hello(self) -> bool:
        '''
        Zero joints and wiggle eyebrows :one:
        '''
        return self.client.hello()

    def discover(self) -> List[str]:
        '''
        Try and find us some Martys! :one:
        '''
        return self.client.discover()

    ''' 
    ============================================================
    Helper commands that you probably won't need to use directly
    ============================================================
    '''

    def __del__(self) -> None:
        '''
        Marty is stopping
        '''
        self.close()

    def close(self) -> None:
        '''
        Close connection to Marty
        '''
        if self.client:
            self.client.close()

    def register_logging_callback(self, loggingCallback: Callable[[str], None]) -> None:
        if self.client:
            self.client.register_logging_callback(loggingCallback)

    def get_interface_stats(self) -> Dict:
        return self.client.get_interface_stats()

    def get_test_output(self) -> str:
        return self.client.get_test_output()
