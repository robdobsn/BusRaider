from .setupTests import setupTests
import json

def test_stepSingle_RequiresPaging():
    def frameCallback(frameJson, logger):
        commonTest.logger.info("Frame callback unexpected" + json.dumps(frameJson))

    # Setup
    commonTest = setupTests("StepSingle", frameCallback)
    testRepeatCount = 1

    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal ANSI"
    commonTest.logger.debug(f"Setting machine {mc}")
    resp = commonTest.sendFrameSync("SetMcJson", '{"cmdName":"SetMcJson"}\0{"name":"' + mc + '"}\0')
    if resp.get("cmdName","") == "SetMcJsonResp" and resp.get("rslt","") == "ok":
        commonTest.logger.debug(f"SetMcJson failed {resp}")
    assert(resp.get("rslt","") == "ok")

    # Test data - load A,6; inc A * 3 and jump to 0002
    testWriteData = b"\x3e\x06\x3c\x3c\x3c\xc3\x02\x00"
    testWriteLen = bytes(str(len(testWriteData)),'utf-8')
    testWriteAddr = 0

    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqRd":0, "iorqWr":0, "mreqRd":0, "mreqWr":0, "AFOK": False, "regsOk": False, "stepCount":0}

    # Send program
    resp = commonTest.sendFrameSync("Set Program",
            "{" + f'"cmdName":"Wr","addr":"{testWriteAddr:04x}","lenDec":{len(testWriteData)},"isIo":0' + "}",
            testWriteData)
    if resp.get("rslt", "") == "ok":
        a = resp.get("data", "")
    assert(resp.get("rslt","") == "ok")

    # Reset
    resp = commonTest.sendFrameSync("Reset", "{" + f'"cmdName":"targetReset"' + "}")
    if resp.get('rslt',"") != "ok":
        commonTest.logger.error(f"Invalid response {resp}")

    # Turn debugging on
    resp = commonTest.sendFrameSync("Debug on", "{" + f'"cmdName":"debugBreak"' + "}")
    if resp.get('rslt',"") != "ok":
        commonTest.logger.error(f"Invalid response {resp}")

    # Step
    resp = commonTest.sendFrameSync("Debug step", "{" + f'"cmdName":"debugStepIn"' + "}")
    if resp.get('rslt',"") != "ok":
        commonTest.logger.error(f"Invalid response {resp}")

    # Turn debugging off
    resp = commonTest.sendFrameSync("Debug off", "{" + f'"cmdName":"debugContinue"' + "}")
    if resp.get('rslt',"") != "ok":
        commonTest.logger.error(f"Invalid response {resp}")

    # Setup Test
    # commonTest.sendFrame("targetTrackerOn", b"{\"cmdName\":\"targetTrackerOn\",\"reset\":1}\0", "targetTrackerOnDone")
    # commonTest.awaitResponse(2000)
    # numTestLoops = 20
    # expectedAFValue = 5
    # codePos = 0
    # for _ in range(numTestLoops):
    #     # Run a step
    #     commonTest.sendFrame("stepInto", b"{\"cmdName\":\"stepInto\"}\0", "stepIntoDone")
    #     commonTest.awaitResponse(2000)
    #     if codePos > 0 and codePos <=3:
    #         expectedAFValue += 1
    #     codePos += 1
    #     if codePos >= 5:
    #         codePos = 1

    # # Expected regs
    # regsExpectedContent = f"AF={expectedAFValue:02x}"
    # time.sleep(.1)

    # # Check status
    # commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
    # commonTest.sendFrame("getRegs", b"{\"cmdName\":\"getRegs\"}\0")
    # time.sleep(.2)

    # # Clear Test
    # commonTest.sendFrame("targetTrackerOff", b"{\"cmdName\":\"targetTrackerOff\"}\0")

    # # Wait for test end and cleardown
    # commonTest.cleardown()
    # assert(testStats["msgRdOk"])
    # assert(testStats["AFOK"])
    # assert(testStats["regsOk"])
    # assert(testStats["stepCount"] == numTestLoops)
    # assert(testStats["unknownMsgCount"] == 0)
    # commonTest.logger.debug(f"StepCount {testStats['stepCount']}")

    # Wait for test end and cleardown
    commonTest.cleardown()
    # assert(rsltOkCount == testRepeatCount * len(mcList))

# def test_stepSingle_RequiresPaging():

#     def frameCallback(msgContent, logger):
#         global readDataExpected
#         if msgContent['cmdName'] == "busStatusResp":
#             testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
#             testStats['iorqWr'] = msgContent['iorqWr']
#             testStats['iorqRd'] = msgContent['iorqRd']
#             testStats['mreqWr'] = msgContent['mreqWr']
#             testStats['mreqRd'] = msgContent['mreqRd']
#             commonTest.logger.debug(msgContent)
#         elif "SetMcJsonResp" in msgContent['cmdName'] or \
#                     msgContent['cmdName'] == 'ClearTargetResp' or \
#                     msgContent['cmdName'] == 'targetResetResp' or \
#                     msgContent['cmdName'] == 'targetTrackerOnResp' or \
#                     msgContent['cmdName'] == 'targetTrackerOffResp':                    
#             assert(msgContent["rslt"] == 'ok')
#         elif msgContent['cmdName'] == 'stepIntoResp':
#             testStats['stepCount'] += 1
#         elif msgContent['cmdName'] == 'ProgramAndResetResp' or \
#                     msgContent['cmdName'] == "ProgramAndExecResp":
#             assert(msgContent["rslt"] == 'ok')
#             testStats['programAndResetCount'] += 1
#         elif msgContent['cmdName'] == "clockHzSetResp":
#             pass
#         elif msgContent['cmdName'] == "WrResp":
#             pass
#         elif msgContent['cmdName'] == "RdResp":
#             requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
#             respOk = requiredResp == msgContent['data']
#             testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
#             testStats["msgRdRespCount"] += 1
#             if not respOk:
#                 logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
#         elif msgContent['cmdName'] == "busStatusClearResp" or \
#                     msgContent['cmdName'] == "busInitResp":
#             pass
#         elif msgContent['cmdName'] == "getRegsResp":
#             commonTest.logger.info(f"Expected register value {regsExpectedContent}")
#             commonTest.logger.info(f"{msgContent['regs']}")
#             testStats["AFOK"] = (regsExpectedContent in msgContent['regs'])
#             if not testStats["AFOK"]:
#                 commonTest.logger.error(f"AFOK not ok! {msgContent}")
#             testStats["regsOk"] = True
#         elif msgContent['cmdName'] == "stepIntoDone":
#             pass
#         elif msgContent['cmdName'] == "targetTrackerOnDone":
#             pass
#         else:
#             testStats["unknownMsgCount"] += 1
#             commonTest.logger.info(f"Unknown message {msgContent}")

#     commonTest = setupTests("StepSingle", frameCallback)
