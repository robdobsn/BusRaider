from .setupTests import setupTests
from pathlib import Path
import time
import json
import os

def test_TRS80Level1Rom():

    def frameCallback(frameJson, logger):
        commonTest.logger.info("Frame callback unexpected" + json.dumps(frameJson))

    # Setup
    commonTest = setupTests("TRS80Level1RomDirect", frameCallback)
    testRepeatCount = 2

    # Test data
    TRS80ScreenAddr = 0x3c40
    romAddr = 0x0000

    # Set TRS80 machine
    mc = "TRS80"
    commonTest.logger.debug(f"Setting machine {mc}")
    resp = commonTest.sendFrameSync("SetMcJson", '{"cmdName":"SetMcJson"}', '{' + f'"name":"{mc}"' + '}')
    if resp.get("cmdName","") == "SetMcJsonResp" and resp.get("err","") == "ok":
        commonTest.logger.debug(f"SetMcJson failed {resp}")
    assert(resp.get("err","") == "ok")

    # Loop through tests
    for _ in range(testRepeatCount):

        # Remove test valid check text
        testWriteData = "TESTING IN PROGRESS"
        readDataExpected = testWriteData
        resp = commonTest.sendFrameSync("writeTestMsg",
                "{" + f'"cmdName":"Wr","addr":"{TRS80ScreenAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}",
                testWriteData)
        assert(resp.get("err","") == "ok")

        # Clear target buffer
        resp = commonTest.sendFrameSync("ClearTarget", '{"cmdName":"ClearTarget"}')
        assert(resp.get("err","") == "ok")

        # Send ROM
        targetCodeFolder = os.path.join(Path(__file__).parent, "testdata")
        targetCodeFilename = r"level1.rom"
        romCmdHeader, romFrame = commonTest.formFileFrame(targetCodeFolder, targetCodeFilename)
        assert(not(romFrame is None))
        if not romFrame is None:
            # Send ROM data
            commonTest.logger.debug(f"ROM data len {len(romFrame)} start {romFrame[0:60]}")
            resp = commonTest.sendFrameSync("Level1Rom", romCmdHeader, romFrame)
            assert(resp.get("err","") == "ok")

            # Program and reset
            resp = commonTest.sendFrameSync("ProgramAndReset", '{"cmdName":"ProgramAndReset"}')
            assert(resp.get("err","") == "ok")
            time.sleep(1.5)

            # Test memory at screen location
            readDataExpected = "READY"
            rdLen = len(readDataExpected)
            resp = commonTest.sendFrameSync("blockRead", "{" + f'"cmdName":"Rd","addr":"{TRS80ScreenAddr}","lenDec":{rdLen},"isIo":0')
            assert(resp.get("err","") == "ok")

            # Check data
            expectedResp = ''.join(f'{ord(x):02x}' for x in readDataExpected)
            assert(expectedResp == resp.get("data",""))

    # Wait for test end and cleardown
    commonTest.cleardown()


    # # Wait for test end and cleardown
    # commonTest.cleardown()

    # assert(testStats["unknownMsgCount"] == 0)
    # assert(testStats["programAndResetCount"] == testRepeatCount)
    # assert(testStats["msgRdOk"] == True)
    # assert(testStats["msgRdRespCount"] == testRepeatCount * 2)


    # def frameCallback(msgContent, logger):
    #     if msgContent['cmdName'] == "busStatusResp":
    #         testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
    #     elif "SetMcJsonResp" in msgContent['cmdName'] or \
    #                 msgContent['cmdName'] == 'ClearTargetResp' or \
    #                 msgContent['cmdName'] == 'FileTargetResp':
    #         assert(msgContent['err'] == 'ok')
    #     elif msgContent['cmdName'] == 'ProgramAndResetResp':
    #         assert(msgContent['err'] == 'ok')
    #         testStats['programAndResetCount'] += 1
    #     elif msgContent['cmdName'] == "clockHzSetResp":
    #         pass
    #     elif msgContent['cmdName'] == "WrResp":
    #         pass
    #     elif msgContent['cmdName'] == "RdResp":
    #         requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
    #         respOk = requiredResp == msgContent['data']
    #         testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
    #         testStats["msgRdRespCount"] += 1
    #         if not respOk:
    #             commonTest.logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
    #     elif msgContent['cmdName'] == "busInitResp":
    #         pass
    #     else:
    #         testStats["unknownMsgCount"] += 1
    #         commonTest.logger.info(f"Unknown message {msgContent}")

    # commonTest = setupTests("TRS80Level1RomExec", frameCallback)
    # testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0}

    # # Bus init
    # commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")

    # # Set TRS80
    # mc = "TRS80"
    # TRS80ScreenAddr = '3c40'
    # commonTest.sendFrame("SetMcJson", b"{\"cmdName\":\"SetMcJson\"}\0{\"name\":\"" + bytes(mc,'utf-8') + b"\"}\0")
    # time.sleep(1)

    # # Processor clock
    # commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")
