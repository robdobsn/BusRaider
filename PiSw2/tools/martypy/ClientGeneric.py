from abc import ABC, abstractmethod
from typing import Callable, Dict, List, Optional, Union

class ClientGeneric(ABC):

    SIDE_CODES = {
        'left'    : 1,
        'right'   : 0,
        'forward' : 3,
        'back'    : 2,
        'auto'    : 0,
    }

    EYE_POSES = {
        'angry'   : 'eyesAngry',
        'excited' : 'eyesExcited',
        'normal'  : 'eyesNormal',
        'wide'    : 'eyesWide',
        'wiggle'  : 'wiggleEyes'
    }

    NOT_IMPLEMENTED = "Unfortunately this Marty doesn't do that"

    def __init__(self):
        super().__init__()

    @classmethod
    def dict_merge(cls, *dicts):
        '''
        Merge all provided dicts into one dict
        '''
        merged = {}
        for d in dicts:
            if not isinstance(d, dict):
                raise ValueError('Value should be a dict')
            else:
                merged.update(d)
        return merged

    @abstractmethod
    def start(self):
        pass

    @abstractmethod
    def close(self):
        pass

    @abstractmethod
    def hello(self) -> bool:
        return False

    @abstractmethod
    def get_ready(self) -> bool:
        return False

    @abstractmethod
    def discover(self) -> List[str]:
        return []

    @abstractmethod
    def stop(self, stop_type: str, stopCode: int) -> bool:
        return False

    @abstractmethod
    def resume(self) -> bool:
        return False
    
    @abstractmethod
    def hold_position(self, hold_time: int) -> bool:
        return False

    @abstractmethod
    def move_joint(self, joint_id: int, position: int, move_time: int) -> bool:
        return False

    @abstractmethod
    def get_joint_position(self, joint_id: Union[int, str]) -> float:
        return 0

    @abstractmethod
    def get_joint_current(self, joint_id: Union[int, str]) -> float:
        return 0

    @abstractmethod
    def get_joint_status(self, joint_id: Union[int, str]) -> int:
        return 0

    @abstractmethod
    def lean(self, direction: str, amount: int, move_time: int) -> bool:
        return False

    @abstractmethod
    def walk(self, num_steps: int = 2, start_foot:str = 'auto', turn: int = 0, 
                step_length:int = 40, move_time: int = 1500) -> bool:
        return False

    @abstractmethod
    def eyes(self, joint_id: int, pose_or_angle: Union[str, int], move_time: int = 100) -> bool:
        return False

    @abstractmethod
    def kick(self, side: str = 'right', twist: int = 0, move_time: int = 2000) -> bool:
        return False

    @abstractmethod
    def arms(self, left_angle: int, right_angle: int, move_time: int) -> bool:
        return False

    @abstractmethod
    def celebrate(self, move_time: int = 4000) -> bool:
        return False

    @abstractmethod
    def circle_dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        return False

    @abstractmethod
    def dance(self, side: str = 'right', move_time: int = 1500) -> bool:
        return False

    @abstractmethod
    def wiggle(self, move_time: int = 1500) -> bool:
        return False

    @abstractmethod
    def sidestep(self, side: str, steps: int = 1, step_length: int = 100, 
            move_time: int = 2000) -> bool:
        return False

    @abstractmethod
    def play_sound(self, name_or_freq_start: Union[str,float], 
            freq_end: Optional[float] = None, 
            duration: Optional[int] = None) -> bool:
        return False

    @abstractmethod
    def pinmode_gpio(self, gpio: int, mode: str) -> bool:
        return False

    @abstractmethod
    def write_gpio(self, gpio: int, value: int) -> bool:
        return False

    @abstractmethod
    def digitalread_gpio(self, gpio: int) -> bool:
        return False

    @abstractmethod
    def i2c_write(self, *byte_array: int) -> bool:
        return False

    @abstractmethod
    def i2c_write_to_ric(self, address: int, byte_array: bytes) -> bool:
        return False

    @abstractmethod
    def get_battery_voltage(self) -> float:
        return 0

    @abstractmethod
    def get_battery_remaining(self) -> float:
        return 0

    @abstractmethod
    def get_distance_sensor(self) -> float:
        return 0

    @abstractmethod
    def get_accelerometer(self, axis: Optional[str] = None, axisCode: int = 0) -> float:
        return 0

    @abstractmethod
    def enable_motors(self, enable: bool = True, clear_queue: bool = True) -> bool:
        return False
                  
    @abstractmethod
    def enable_safeties(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def fall_protection(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def motor_protection(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def battery_protection(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def buzz_prevention(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def lifelike_behaviour(self, enable: bool = True) -> bool:
        return False

    @abstractmethod
    def set_parameter(self, *byte_array: int) -> bool:
        return False

    @abstractmethod
    def save_calibration(self) -> bool:
        return False

    @abstractmethod
    def clear_calibration(self) -> bool:
        return False

    @abstractmethod
    def is_calibrated(self) -> bool:
        return False

    @abstractmethod
    def ros_command(self, *byte_array: int) -> bool:
        return False

    @abstractmethod
    def keyframe (self, time: float, num_of_msgs: int, msgs) -> List[bytes]:
        return False

    @abstractmethod
    def get_chatter(self) -> bytes:
        return False

    @abstractmethod
    def get_firmware_version(self) -> bool:
        return False

    @abstractmethod
    def _mute_serial(self) -> bool:
        return False

    @abstractmethod
    def ros_serial_formatter(self, topicID: int, send: bool = False, *message: int) -> List[int]:
        return False

    @abstractmethod
    def is_moving(self) -> bool:
        return False

    @abstractmethod
    def is_paused(self) -> bool:
        return False

    @abstractmethod
    def get_robot_status(self) -> Dict:
        return {}

    @abstractmethod
    def get_joints(self) -> Dict:
        return {}

    @abstractmethod
    def get_power_status(self) -> Dict:
        return {}

    @abstractmethod
    def get_add_ons_status(self) -> Dict:
        return {}

    @abstractmethod
    def get_add_on_status(self, add_on_name_or_id: Union[int, str]) -> Dict:
        return {}

    @abstractmethod
    def get_system_info(self) -> Dict:
        return {}

    @abstractmethod
    def set_marty_name(self, name: str) -> bool:
        return False

    @abstractmethod
    def get_marty_name(self) -> str:
        return ""

    @abstractmethod
    def is_marty_name_set(self) -> bool:
        return False

    @abstractmethod
    def get_hw_elems_list(self) -> List:
        return []

    @abstractmethod
    def send_ric_rest_cmd(self, ricRestCmd: str) -> None:
        pass

    @abstractmethod
    def send_ric_rest_cmd_sync(self, ricRestCmd: str) -> Dict:
        return {}

    @abstractmethod
    def register_logging_callback(self, loggingCallback: Callable[[str],None]) -> None:
        pass

    @abstractmethod
    def get_interface_stats(self) -> Dict:
        return {}
        
    @abstractmethod
    def preException(self, isFatal: bool) -> None:
        pass

    @abstractmethod
    def get_test_output(self) -> dict:
        return ""
