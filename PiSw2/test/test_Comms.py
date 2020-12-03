from .setupTests import setupTests
import json

def test_Comms():
    def frameCallback(frameJson, logger):
        commonTest.logger.info("Frame callback unexpected" + json.dumps(frameJson))

    commonTest = setupTests("Comms", frameCallback)
    msgIdx = 0
    testRepeatCount = 100
    testStats = {"msgRx":[False]*testRepeatCount}
    for i in range(testRepeatCount):
        cmdResp = commonTest.sendFrameSync("", 
                "{" + f'"cmdName":"comtest","msgIdx":"{str(msgIdx)}"' + "}")
        if cmdResp.get("cmdName","") == "comtestResp" and cmdResp.get("rslt","") == "ok":
            testStats["msgRx"][i] = True
    commonTest.cleardown()
    if False in testStats["msgRx"]:
        failedMsgs = [i for i,x in enumerate(testStats["msgRx"]) if not x]
        commonTest.logger.error(f"CommsTest failed to get valid response to one or more msg, failed msg idxs {failedMsgs}")
    else:
        commonTest.logger.info(f"Passed the test")
    for i in range(testRepeatCount):
        assert(testStats["msgRx"][i] == True)
