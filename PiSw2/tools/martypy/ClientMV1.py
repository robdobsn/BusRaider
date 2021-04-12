from typing import Callable, Dict, List, Tuple, Union
import sys
import time
import socket
import struct
from .ClientGeneric import ClientGeneric
from .Exceptions import (MartyConnectException,
                         MartyCommandException,
                         ArgumentOutOfRangeException,
                         UnavailableCommandException)

class ClientMV1(ClientGeneric):
    '''
    Lower level interface class between the `Marty` abstracted
    control class and the RIC socket interface
    '''
    SOCKET_PORT = 24

    SIDE_CODES = {
        'left'    : '\x00',
        'right'   : '\x01',
        'forward' : '\x02',
        'back'    : '\x03',
        'auto'    : '\xff',
    }

    EYE_POSES = {
        'angry': -15,
        'excited': 30,
        'normal': 0,
        'wide': 90
    }

    def __init__(self, method: str, locator: str,
                port:str = None, timeout:float = 5.0, 
                debug:bool = False,
                default_lifelike: bool = True,                
                *args, **kwargs):
        '''
        Initialise connection to remote Marty over a IPv4 socket by name 'loc' over port 24
        Args:
            loc, type str, must either resolve to an IP or be an IP address
        Raises:
            MartyConnectException if the socket failed to make the connection to the host
        '''
        if port is None:
            self.port = self.SOCKET_PORT
        else:
            self.port = port

        self.debug = debug

        self.loc = locator
        self.timeout = timeout
        self.default_lifelike = default_lifelike

        self.sock = self._socket_factory()

        self.COMMANDS_LUT = dict()
        self._init_commands()


    def start(self):
        # To be able to do anything:
        self.enable_safeties(True)
        self.enable_motors(True)

        if self.default_lifelike:
            self.lifelike_behaviour(True)
        
    def close(self):
        if self.sock:
            self.sock.close()
        self.sock = None

    def hello(self):
        return self._execute('hello')

    def get_ready(self):
        return self._execute('hello')

    def discover(self):
        return self._discover()

    def stop(self, stop_type: str, stopCode: int) -> bool:
        M1_STOP_TYPE = ['\x00','\x01','\x02','\x03','\x04','\x05']
        return self._execute('stop', M1_STOP_TYPE[stopCode])

    def resume(self) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def hold_position(self, hold_time: int) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def move_joint(self, joint_id: int, position: int, move_time: int) -> bool:
        '''
        Move a specific joint to a position
        Args:
            move_time: how long this movement should last, in milliseconds
        '''
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('move_joint',
                                   self._pack_uint8(joint_id),
                                   self._pack_int8(position),
                                   dur_lsb, dur_msb)

    def get_joint_position(self, joint_id: Union[int, str]) -> float:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_joint_current(self, joint_name_or_num: Union[int, str]) -> float:
        if type(joint_name_or_num) is str:
            raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)
        return self.get_motor_current(int(joint_name_or_num))

    def get_joint_status(self, joint_name_or_num: Union[int, str]) -> int:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def lean(self, direction: str, amount: int, move_time: int) -> bool:
        try:
            directionNum = self.SIDE_CODES[direction]
        except KeyError:
            raise MartyCommandException("Direction must be one of {}, not '{}'"
                                        "".format(set(self.SIDE_CODES.keys()), direction))
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('lean', directionNum,
                                   self._pack_int8(amount),
                                   dur_lsb, dur_msb)

    def walk(self, num_steps: int = 2, start_foot:str = 'auto', turn: int = 0, 
                step_length:int = 40, move_time: int = 1500) -> bool:
        try:
            side_c = self.SIDE_CODES[start_foot]
        except KeyError:
            raise MartyCommandException("Direction must be one of {}, not '{}'"
                                        "".format(set(self.SIDE_CODES.keys()), start_foot))
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('walk',
                                   self._pack_uint8(num_steps),
                                   self._pack_int8(turn),
                                   dur_lsb, dur_msb,
                                   self._pack_int8(step_length),
                                   side_c)

    def eyes(self, joint_id: int, pose_or_angle: Union[str, int], move_time: int = 100) -> bool:
        if type(pose_or_angle) is str:
            try:
                angle = self.EYE_POSES[pose_or_angle]
            except KeyError:
                raise MartyCommandException("pose must be one of {}, not '{}'"
                                            "".format(set(self.EYE_POSES.keys()), pose_or_angle))
        else:
            angle = pose_or_angle
        return self.move_joint(joint_id, angle, move_time)

    def kick(self, side: str = 'right', twist: int = 0, move_time: int = 2000) -> bool:
        side_c = self.SIDE_CODES[side]
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('kick', side_c,
                                   self._pack_int8(twist),
                                   dur_lsb, dur_msb)

    def arms(self, left_angle: int, right_angle: int, move_time: int) -> bool:
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('arms',
                                   self._pack_int8(right_angle),
                                   self._pack_int8(left_angle),
                                   dur_lsb, dur_msb)

    def celebrate(self, move_time: int = 4000) -> bool:
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('celebrate', dur_lsb, dur_msb)

    def circle_dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        side_c = self.SIDE_CODES[side]
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('circle_dance',
                                   side_c,
                                   dur_lsb, dur_msb)

    def dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def wiggle(self, move_time: int = 1500) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def sidestep(self, side: str, steps: int = 1, step_length: int = 100, 
            move_time: int = 2000) -> bool:
        side_c = self.SIDE_CODES[side]
        dur_lsb, dur_msb = self._pack_uint16(move_time)
        return self._execute('sidestep', side_c,
                                   self._pack_int8(steps),
                                   dur_lsb, dur_msb,
                                   self._pack_int8(step_length))

    def play_sound(self, freq_start: Union[float, str], 
            freq_end: float, 
            duration: int) -> bool:
        if type(freq_start) is str:
            raise MartyCommandException("first parameter must be a number for Marty V1")
        f_start_lsb, f_start_msb = self._pack_uint16(int(freq_start))
        f_end_lsb, f_end_msb = self._pack_uint16(int(freq_end))
        dur_lsb, dur_msb = self._pack_uint16(5000 if duration is None else duration)
        return self._execute('play_sound',
                                   f_start_lsb, f_start_msb,
                                   f_end_lsb, f_end_msb,
                                   dur_lsb, dur_msb)

    GPIO_PIN_MODES = {
        'digital in'  : '\x00',
        'analog in'   : '\x01',
        'digital out' : '\x02',
        #'servo'       : '\x03',
        #'pwm'         : '\x04',
    }

    def pinmode_gpio(self, gpio: int, mode: str) -> bool:
        return self._execute('gpio_mode', self._pack_uint8(gpio), self.GPIO_PIN_MODES[mode])

    def write_gpio(self, gpio: int, value: int) -> bool:
        byte_array = (self._pack_uint8(gpio),) + self._pack_float(value)
        return self._execute('gpio_write', *byte_array)

    def digitalread_gpio(self, gpio: int) -> bool:
        return bool(self._execute('gpio', self._pack_uint8(gpio)))

    def i2c_write(self, *byte_array: int) -> bool:
        return self._execute('i2c_write', *byte_array)

    def i2c_write_to_ric(self, address: int, byte_array: bytes) -> bool:
        data = ['\x1B']         #i2c_write opcode
        data.append(address)    #i2c address
        data.append(byte_array) #message
        data += self.ros_serial_formatter(111, *byte_array) #ros serial format
        return self._execute('i2c_write', *data)

    def get_battery_voltage(self) -> float:
        return self._execute('battery')

    def get_battery_remaining(self) -> float:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_distance_sensor(self) -> float:
        return self._execute('distance')

    def get_accelerometer(self, axis: str, axisCode: int) -> float:
        M1_ACCEL_AXES = ['\x00','\x01','\x02']
        if axis is None:
            raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)
        return self._execute('accel', M1_ACCEL_AXES[axisCode])

    def get_motor_current(self, motor_id: int) -> float:
        return self._execute('motorcurrent', str(motor_id))

    def enable_motors(self, enable: bool = True, clear_queue: bool = True) -> bool:
        if clear_queue:
            self.stop('clear queue', 0)
        if enable:
            return self._execute('enable_motors') and True
        else:
            return self._execute('disable_motors') and False

    def enable_safeties(self, enable: bool = True) -> bool:
        return self._execute('enable_safeties')

    def fall_protection(self, enable: bool = True) -> bool:
        return self._execute('fall_protection', enable)

    def motor_protection(self, enable: bool = True) -> bool:
        return self._execute('motor_protection', enable)

    def battery_protection(self, enable: bool = True) -> bool:
        return self._execute('battery_protection', enable)

    def buzz_prevention(self, enable: bool = True) -> bool:
        return self._execute('buzz_prevention', enable)

    def lifelike_behaviour(self, enable: bool = True) -> bool:
        return self._execute('lifelike_behaviour', enable)

    def set_parameter(self, *byte_array: int) -> bool:
        return self._execute('set_param', '\x1F', *byte_array)

    def save_calibration(self) -> bool:
        return self._execute('save_calibration')

    def clear_calibration(self) -> bool:
        return self._execute('clear_calibration')

    def is_calibrated(self) -> bool:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def ros_command(self,  *byte_array: int) -> bool:
        return self._execute('ros_command', *byte_array)

    def keyframe (self, time: float, num_of_msgs: int, msgs: List) -> List[bytes]:
        processed_keyframe = []

        #Number of key frames
        len_byte0, len_byte1, len_byte2, len_byte3 = self._pack_int32(1)
        processed_keyframe.append(len_byte0)
        processed_keyframe.append(len_byte1)
        processed_keyframe.append(len_byte2)
        processed_keyframe.append(len_byte3)

        #Time (in seconds) to excute keyframe. This is float encoded.
        time_byte0, time_byte1, time_byte2, time_byte3 = self._pack_float(time)
        processed_keyframe.append(time_byte0)
        processed_keyframe.append(time_byte1)
        processed_keyframe.append(time_byte2)
        processed_keyframe.append(time_byte3)

        if(len(msgs) != num_of_msgs):
            raise MartyCommandException('Number of messages do not match entered messages')
        #Array length
        arr_len_byte0, arr_len_byte1, arr_len_byte2, arr_len_byte3 = self._pack_int32(num_of_msgs)
        processed_keyframe.append(arr_len_byte0)
        processed_keyframe.append(arr_len_byte1)
        processed_keyframe.append(arr_len_byte2)
        processed_keyframe.append(arr_len_byte3)

        #Messages
        for items in msgs:
            for values in items:
                processed_keyframe.append(self._pack_int8(values))

        return(processed_keyframe)

    def get_chatter(self) -> bytes:
        return self._execute('chatter')

    def get_firmware_version(self) -> bool:
        return self._execute('firmware_version')

    def _mute_serial(self) -> bool:
        return self._execute('mute_serial')

    def ros_serial_formatter(self, topicID: int, send: bool = False, *message: int) -> List[int]:
        msg = message

        msg_length = len(msg)
        #Message length in little endian format
        msg_length_LB = msg_length & 0xFF                           #3rd byte
        msg_length_HB = (msg_length >> 8) & 0xFF                    #4th byte

        checksum1 = 255 - ((msg_length_LB + msg_length_HB) % 256)   #5th byte

        #Topic ID in little endian format
        topic_ID_LB = topicID & 0xFF                                #6th byte
        topic_ID_HB = (topicID >> 8) & 0xFF                         #7th byte

        data_values_sum = 0
        for i in msg:
            data_values_sum += ord(i)

        checksum2 = 255 - ((topic_ID_LB + topic_ID_HB + data_values_sum) % 256) #final byte

        #encode into bytes
        command_to_be_sent = []
        command_to_be_sent += ('\xff',) #Sync Flag. Check ROS Wiki
        command_to_be_sent += ('\xfe',) #Protocol version. Check ROS Wiki
        command_to_be_sent += (chr(msg_length_LB),)
        command_to_be_sent += (chr(msg_length_HB),)
        command_to_be_sent += (chr(checksum1),)
        command_to_be_sent += (chr(topic_ID_LB),)
        command_to_be_sent += (chr(topic_ID_HB),)
        command_to_be_sent += msg
        command_to_be_sent += (chr(checksum2),)

        if(send == True):
            self.ros_command(*command_to_be_sent)

        return(command_to_be_sent)

    def _init_commands(self) -> None:

        # Extend basis LUT with specific handlers
        self._register_commands({
            'discover'           : self._discover,
            'battery'            : self._simple_sensor,
            'distance'           : self._simple_sensor,
            'accel'              : self._select_sensor,
            'motorcurrent'       : self._current_sensor,
            'gpio'               : self._select_sensor,
            'hello'              : self._fixed_command,
            'lean'               : self._fixed_command,
            'walk'               : self._fixed_command,
            'kick'               : self._fixed_command,
            'celebrate'          : self._fixed_command,
            'arms'               : self._fixed_command,
            'sidestep'           : self._fixed_command,
            'circle_dance'       : self._fixed_command,
            'play_sound'         : self._fixed_command,
            'stop'               : self._fixed_command,
            'move_joint'         : self._fixed_command,
            'enable_motors'      : self._fixed_command,
            'disable_motors'     : self._fixed_command,
            'enable_safeties'    : self._fixed_command,
            'fall_protection'    : self._toggle_command,
            'motor_protection'   : self._toggle_command,
            'battery_protection' : self._toggle_command,
            'buzz_prevention'    : self._toggle_command,
            'lifelike_behaviour' : self._toggle_command,
            'clear_calibration'  : self._fixed_command,
            'save_calibration'   : self._fixed_command,
            'ros_command'        : self._command,
            'chatter'            : self._chatter,
            'set_param'          : self._command,
            'firmware_version'   : self._fixed_command,
            'mute_serial'        : self._fixed_command,
            'i2c_write'          : self._command,
            'gpio_write'         : self._fixed_command,
            'gpio_mode'          : self._fixed_command,
        })

        # for opcode, _ in self.COMMANDS_LUT.items():
        #     if opcode not in self.CMD_OPCODES.keys():
        #         print('Missing {}'.format(opcode))

    def _socket_factory(self) -> socket.socket:
        '''
        Generate a new socket connection
        '''
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            loc_ip = socket.gethostbyname(self.loc)
            sock.connect((loc_ip, self.port))
        except Exception as e:
            raise MartyConnectException(e)

        sock.settimeout(self.timeout)

        if sys.platform == 'linux':
            # Configure socket in keepalive mode
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            # Time to wait before sending a keepalive
            sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPIDLE, 1)
            # Interval between keepalives
            sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPINTVL, 1)
            # Number of keepalive fails before declaring the connection broken
            sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPCNT, 5)

        elif sys.platform == 'windows':
            # Keep alive, 5 sec timeout, 1 sec interval
            sock.ioctl(socket.SIO_KEEPALIVE_VALS, (1, 5000, 1000))

        return sock

    def __repr__(self):
        return '{}.{}{}'.format(self.__module__,
                             self.__class__.__name__,
                             self.sock.getsockname())

    @staticmethod
    def _discover(timeout: float = 10, *args: int, **kwargs: int) -> List[str]:
        '''
        Search for Marties on the network using a UDB multicast to port 4000
        '''
        socket_addr = "224.0.0.1"
        socket_port = 4000
        magic_command = b"AA"

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32)
        sock.settimeout(timeout)
        sock.sendto(magic_command, (socket_addr, socket_port))

        found = []
        start = time.time()
        try:
            while (time.time() - start) < timeout:
                data, addr = sock.recvfrom(1000)
                found.append({addr: data})
        except socket.timeout:
            return found

    def is_moving(self) -> bool:
        '''
        Check if Marty is moving

        Args:
            none
        Returns:
            True if Marty is moving
        '''
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)
        
    def is_paused(self) -> bool:
        '''
        Check if Marty is paused

        Args:
            none
        Returns:
            True if Marty is paused
        '''
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_robot_status(self) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)
   
    def get_joints(self) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_power_status(self) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_add_ons_status(self) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_add_on_status(self, add_on_name_or_id: Union[int, str]) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def get_system_info(self) -> Dict:
        return {"HardwareVersion":"1.0", "SystemName":"MartyV1","SystemVersion":"1.0.0","SerialNo":"000001","MAC":"000000000000"}

    def set_marty_name(self, name: str) -> bool:
        return False

    def get_marty_name(self) -> str:
        return "Marty"

    def is_marty_name_set(self) -> bool:
        return False

    def get_hw_elems_list(self) -> List:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def send_ric_rest_cmd(self, ricRestCmd: str) -> None:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    def send_ric_rest_cmd_sync(self, ricRestCmd: str) -> Dict:
        raise MartyCommandException(ClientGeneric.NOT_IMPLEMENTED)

    # Encodes Command Type flag, LSB size, MSB size, Data
    CMD_OPCODES = {
        'battery'            : ['\x01', '\x01', '\x00'],         # OK
        'distance'           : ['\x01', '\x08', '\x00'],         #
        'accel'              : ['\x01', '\x02'],                 # OK
        'motorcurrent'       : ['\x01', '\x03'],                 # OK
        'gpio'               : ['\x01', '\x04'],                 #
        'chatter'            : ['\x01', '\x05', '\x00',],        # Variable Length
        'hello'              : ['\x02', '\x01', '\x00', '\x00'], # OK
        'lean'               : ['\x02', '\x05', '\x00', '\x02'], #
        'walk'               : ['\x02', '\x07', '\x00', '\x03'], # OK
        'kick'               : ['\x02', '\x05', '\x00', '\x05'], # OK
        'celebrate'          : ['\x02', '\x03', '\x00', '\x08'], # OK
        'arms'               : ['\x02', '\x05', '\x00', '\x0B'], #
        'sidestep'           : ['\x02', '\x06', '\x00', '\x0E'], #
        'play_sound'         : ['\x02', '\x07', '\x00', '\x10'], #
        'stop'               : ['\x02', '\x02', '\x00', '\x11'], # OK
        'move_joint'         : ['\x02', '\x05', '\x00', '\x12'], #
        'enable_motors'      : ['\x02', '\x01', '\x00', '\x13'], # Has optional args now
        'disable_motors'     : ['\x02', '\x01', '\x00', '\x14'], # Has optional args now
        'circle_dance'       : ['\x02', '\x04', '\x00', '\x1C'], #
        'enable_safeties'    : ['\x02', '\x01', '\x00', '\x1E'], #
        'fall_protection'    : ['\x02', '\x02', '\x00', '\x15'], #
        'motor_protection'   : ['\x02', '\x02', '\x00', '\x16'], #
        'battery_protection' : ['\x02', '\x02', '\x00', '\x17'], #
        'buzz_prevention'    : ['\x02', '\x02', '\x00', '\x18'], #
        'lifelike_behaviour' : ['\x02', '\x02', '\x00', '\x1D'], #
        'clear_calibration'  : ['\x02', '\x01', '\x00', '\xFE'], #
        'save_calibration'   : ['\x02', '\x01', '\x00', '\xFF'], #
        'ros_command'        : ['\x03'],                         # Variable Length
        'set_param'          : ['\x02', '\x1F'], #
        'firmware_version'   : ['\x02', '\x01', '\x00', '\x20'], #
        'mute_serial'        : ['\x02', '\x01', '\x00', '\x21'], # OK
        'i2c_write'          : ['\x02'],                         #
        'gpio_write'         : ['\x02', '\x06', '\x00', '\x1A'], #
        'gpio_mode'          : ['\x02', '\x03', '\x00', '\x19'], #
    }

    def _pack(self, characters: List[str]) -> bytes:
        '''
        Pack characters list into a byte string
        Expects pre-packed chars, use chr or struct.pack to do this
        '''
        if self.debug:
            print(list(map(lambda x: '{}:{}'.format(x, type(x)), characters)))
        try:
            return ("".join(characters)).encode("latin-1")
        except UnicodeEncodeError:
            raise ArgumentOutOfRangeException('Argument(s) overflowed int')

    def _fixed_command(self, *args: int, **kwargs: int) -> bool:
        '''
        Send a command with a fixed (preknown) number of arguments
        that does not expect a response.
        NB!: Assumes data (args[2:]) is broken into 8-bit bytes
        '''
        cmd = args[1]
        opcode = self.CMD_OPCODES[cmd]
        datalen = ord(opcode[1]) + (ord(opcode[2]) << 8) - 1
        data = list(args[2:])
        if len(data) != datalen:
            raise TypeError('{} takes {} arguments but {} were given'
                            ''.format(cmd, datalen, len(data)))
        self.sock.send(self._pack(opcode + data))
        return True

    def _command(self, *args: int, **kwargs: int) -> bool:
        '''
        Pipes args down the socket whilst calculating the payload length
        Args:
            *args of length at least 3
        '''
        cmd = args[1]
        opcode = self.CMD_OPCODES[cmd][0]
        data = list(args[2:])
        datalen_lsb, datalen_msb = struct.pack('<H', len(data))
        payload = [opcode,
                   chr(datalen_lsb[0]),
                   chr(datalen_msb[0])] + data
        self.sock.send(self._pack(payload))
        return True

    def _toggle_command(self, *args: int, **kwargs: int) -> bool:
        '''
        Takes a python Boolean and toggles a switch on the board
        '''
        cmd = args[1]
        toggle = '\x01' if args[2] == True else '\x00'
        opcode = self.CMD_OPCODES[cmd]
        self.sock.send(self._pack(opcode + [toggle]))
        return args[2]

    def _simple_sensor(self, *args: int, **kwargs: int) -> int:
        '''
        Read a simple sensor and give its value
        Args:
            cmd
        '''
        cmd = args[1]
        self.sock.send(self._pack(self.CMD_OPCODES[cmd]))
        data = self.sock.recv(4)
        return struct.unpack('f', data)[0]

    def _select_sensor(self, *args: int, **kwargs: int) -> float:
        '''
        Read a sensor that takes an argument and give its value
        Args:
            cmd, index
        '''
        cmd = args[1]
        index = args[2]
        self.sock.send(self._pack(self.CMD_OPCODES[cmd] + [index]))
        data = self.sock.recv(4)
        return struct.unpack('f', data)[0]

    def _current_sensor(self, *args: int, **kwargs: int) -> float:
        '''
        Read current sensor that takes an argument and give its value
        Args:
            cmd, index
        '''
        cmd = args[1]
        index = args[2]
        pakd = self._pack(self.CMD_OPCODES[cmd] + [chr(int(index))])
        self.sock.send(pakd)
        data = self.sock.recv(4)
        return struct.unpack('f', data)[0]

    def _chatter(self, *args: int, **kwargs: int) -> bytes:
        '''
        Return chatter topic data (variable length)
        '''
        cmd = args[1]
        self.sock.send(self._pack(self.CMD_OPCODES[cmd]))
        data_length = self.sock.recv(1)[0]
        data = self.sock.recv(data_length)
        return data

    def _pack_uint16(self, num: int) -> Tuple[str,str]:
        '''
        Pack an unsigned 16 bit int into two 8 bit bytes, little-endian
        Returns:
            tuple(least-sig-byte, most-sig-byte)

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            h      short/uint16           integer        2
        '''
        try:
            data = struct.pack('<H', num)
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return chr(data[0]), chr(data[1])

    def _pack_int16(self, num: int) -> Tuple[str,str]:
        '''
        Pack a signed 16 bit int into two 8 bit bytes, little-endian
        Returns:
            tuple(least-sig-byte, most-sig-byte)

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            h      short/uint16           integer        2
        '''
        try:
            data = struct.pack('<h', num)
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return chr(data[0]), chr(data[1])

    def _pack_int32(self, num: int) -> Tuple[str,str,str,str]:
        '''
        Pack a signed 32 bit int into four 8 bit bytes, little-endian
        Returns:
            tuple(least-sig-byte, less-sig-byte, more-sig-byte, most-sig-byte)

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            i      int                    integer        4
        '''
        try:
            data = struct.pack('<i', num)
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return (chr(data[0]),
                chr(data[1]),
                chr(data[2]),
                chr(data[3]))

    def _pack_uint8(self, num: int) -> str:
        '''
        Pack an unsigned 8 bit int into one 8 bit byte, little-endian
        Returns:
            bytes

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            B      unsigned char/uint8    integer        1
        '''
        try:
            data = struct.pack('<B', num)
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return chr(data[0])

    def _pack_int8(self, num: int) -> str:
        '''
        Pack a signed 8 bit int into one 8 bit unsigned byte, little-endian
        Returns:
            bytes

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            b      signed char/int8       integer        1
        '''
        try:
            data = struct.pack('<b', num)
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return chr(data[0])

    def _pack_float(self, num: float) -> Tuple[str,str,str,str]:
        '''
        Pack a float into four bit unsigned byte, little-endian
        Returns:
            tuple(least-sig-byte, less-sig-byte, more-sig-byte, most-sig-byte)

        Struct:
            Fmt    C Type                 Python Type    Standard Size
            f      float                  float          4
        '''
        try:
            data = struct.pack('<f', float(num))
        except struct.error as e:
            raise ArgumentOutOfRangeException(e)
        return (chr(data[0]),
                chr(data[1]),
                chr(data[2]),
                chr(data[3]))

    def _execute(self, *args, **kwargs):
        '''
        Execute the command prescribed by the commands lookup table
        '''
        try:
            return self.COMMANDS_LUT[args[0]](self, *args, **kwargs)
        except KeyError:
            raise UnavailableCommandException("The command '{}' is not available "
                                              "from the {} client type"
                                              "".format(args[0], str(self.__class__.__name__)))
        except Exception as e:
            raise MartyCommandException(e)

    def _register_commands(self, handlers):
        '''
        Take {str:func} command names & handlers in a dict
        and register them with the COMMANDS_LUT
        '''
        self.COMMANDS_LUT = ClientGeneric.dict_merge(self.COMMANDS_LUT, handlers)

    def register_logging_callback(self, loggingCallback: Callable[[str],None]) -> None:
        return
        
    def get_interface_stats(self) -> Dict:
        return {}

    def get_test_output(self) -> dict:
        return ""
