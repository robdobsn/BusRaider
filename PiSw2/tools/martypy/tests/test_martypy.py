import pytest
import time
from martypy import Marty

def test_close() -> None:
    marty = Marty("test", "")
    time.sleep(6)
    marty.close()
    testOutput = marty.get_test_output()
    testExpected = {"buf":
            [
                bytearray(b'\x01\x02\x00v\x00'), 
                bytearray(b'\x02\x02\x00hwstatus\x00'),
                bytearray(b'\x03\x02\x03{"cmdName":"subscription","action":"update","pubRecs":[{"name":"MultiStatus","rateHz":10.0,}{"name":"PowerStatus","rateHz":1.0},{"name":"AddOnStatus","rateHz":10.0}]}\x00\x00'), 
                bytearray(b'\x04\x02\x03{"cmdName":"subscription","action":"update","pubRecs":[{"name":"MultiStatus","rateHz":0},{"name":"PowerStatus","rateHz":0},{"name":"AddOnStatus","rateHz":0}]}\x00\x00')
            ]
        }
    assert(testOutput==testExpected)

def test_circle_dance() -> None:
    marty = Marty("test","",subscribeRateHz=0)
    marty.circle_dance()
    testOutput = marty.get_test_output()
    testExpected = {"buf":
            [
                bytearray(b'\x01\x02\x00v\x00'),
                bytearray(b'\x02\x02\x00hwstatus\x00'),
                bytearray(b'\x03\x02\x00traj/circle?side=0&moveTime=1500\x00')
            ]
        }
    assert(testOutput==testExpected)

