from .setupTests import setupTests

def test_SetMc():
    def frameCallback(frameJson, logger):
        commonTest.logger.info("Frame callback unexpected" + json.dumps(frameJson))

    # Setup
    commonTest = setupTests("SetMc", frameCallback)
    testRepeatCount = 2

    # Get list of machines
    mcListResp = commonTest.sendFrameSync("SetMc getStatus", '{"cmdName":"getStatus"}')
    mcList = mcListResp.get('machineList',[])

    # Set machines alternately
    rsltOkCount = 0
    for _ in range(testRepeatCount):
        for mc in mcList:
            commonTest.logger.debug(f"Setting machine {mc}")
            setMcResp = commonTest.sendFrameSync("SetMcJson", '{"cmdName":"SetMcJson"}','{"name":"' + mc + '"}')
            if setMcResp.get("cmdName","") == "SetMcJsonResp" and setMcResp.get("err","") == "ok":
                rsltOkCount += 1
            else:
                commonTest.logger.error(f"Failed setMc {setMcResp}")
            # commonTest.sendFrameSync("getStatus", '{"cmdName":"getStatus"}')

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(rsltOkCount == testRepeatCount * len(mcList))
