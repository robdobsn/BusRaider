from .setupTests import setupTests
import random

def test_MemWriteAndRead():

    def frameCallback(frameJson, logger):
        commonTest.logger.info("Frame callback unexpected" + json.dumps(frameJson))

    # Setup
    commonTest = setupTests("MemRW", frameCallback)
    testRepeatCount = 20

    # Test data
    testWriteAddr = 0x8000
    testWriteData = bytearray(b"\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55")

    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal ANSI"
    commonTest.logger.debug(f"Setting machine {mc}")
    resp = commonTest.sendFrameSync("SetMcJson", '{"cmdName":"SetMcJson"}\0{"name":"' + mc + '"}\0')
    if resp.get("cmdName","") == "SetMcJsonResp" and resp.get("err","") == "ok":
        commonTest.logger.debug(f"SetMcJson failed {resp}")
    assert(resp.get("err","") == "ok")

    # Set processor clock
    commonTest.logger.debug(f"Setting processor clock")
    resp = commonTest.sendFrameSync("ClockSetHz", '{"cmdName":"clockHzSet","clockHz":250000}')
    commonTest.logger.debug(f"Set processor clock resp {resp}")
    assert(resp.get("err","") == "ok")

    # Memory tests
    for i in range(testRepeatCount):
        resp = commonTest.sendFrameSync("testWrRd",
                "{" + f'"cmdName":"testWrRd","addr":"{testWriteAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}",
                testWriteData)
        if resp.get("err", "") == "ok":
            a = resp.get("data", "")
        assert(resp.get("err","") == "ok")
        # Check data
        expectedResp = ''.join(f'{x:02x}' for x in testWriteData)
        assert(expectedResp == resp.get("data",""))
        # Change test data for next time
        testWriteData = bytearray(len(testWriteData))
        for i in range(len(testWriteData)):
            testWriteData[i] = random.randint(0,255)

    # Wait for test end and cleardown
    commonTest.cleardown()
