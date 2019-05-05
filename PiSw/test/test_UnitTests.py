from CommonTestCode import CommonTest
import time
import logging

# This is a program for testing the BusRaider firmware
# It requires either:
# a) a BusRaider supporting the CommandHandler protocol (HDLC) and operating on a serial port
#   (BusRaider jumpers configured to connect Pi serial to FTDI)
# b) a BusRaider with ESP32 firmware for uploading (normal firmware) and configured
#    with ESP Serial 0 to Pi
# c) most tests also require Clock from BusRaider to be enabled, 64K Ram board in place

def setupTests(testName):
    global useIP, serialPort, serialSpeed, baseFolder
    global logTextFileName, logMsgDataFileName, ipAddrOrHostName
    global commonTest
    useIP = True
    serialPort = "COM7"
    serialSpeed = 921600
    ipAddrOrHostName = "192.168.86.192"
    baseFolder = "."
    logTextFileName = testName + ".log"
    logMsgDataFileName = testName + ".bin"
    commonTest = CommonTest()

def test_Comms():

    def frameCallback(frameJson):
        assert(frameJson['cmdName'] == "validatorStatusResp")
        if frameJson['cmdName'] == "validatorStatusResp":
            logger.info(f"isrCount {frameJson['isrCount']} errors {frameJson['errors']}")
            testStats["framesRx"] += 1
            testStats["msgRx"][frameJson['msgIdx']] = True

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("Comms")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    msgIdx = 0
    testRepeatCount = 100
    testStats = {"framesRx": 0, "msgRx":[False]*testRepeatCount}
    for i in range(testRepeatCount):
        commonTest.sendFrame("statusReq", b"{\"cmdName\":\"validatorStatus\",\"msgIdx\":\"" + bytes(str(msgIdx),'utf-8') + b"\"}\0")
        msgIdx += 1
        time.sleep(0.2)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["framesRx"] == testRepeatCount)
    for i in range(testRepeatCount):
        assert(testStats["msgRx"][i] == True)

def test_MemRW():

    def frameCallback(msgContent):
        logger.info(f"Frame::::{msgContent}")
        if msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % testWriteData[i]) for i in range(len(testWriteData)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "WrResp":
            testStats["msgWrRespCount"] += 1
            try:
                if msgContent['err'] != 'ok':
                    testStats["msgWrRespErrCount"] += 1
                    logger.error(f"WrResp err not ok {msgContent}")
            except:
                logger.error(f"WrResp doesn't contain err {msgContent}")
                testStats["msgWrRespErrMissingCount"] += 1
        elif msgContent['cmdName'] == "busResetResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "clockSetHzResp":
            testStats["clockSetOk"] = msgContent['err'] == "ok"
        else:
            testStats["unknownMsgCount"] += 1

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("MemRW")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    testRepeatCount = 1
    # Test data
    testWriteData = b"\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55"
    testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0, "unknownMsgCount":0, "clockSetOk":False}
    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
    time.sleep(1)
    # Processor clock
    commonTest.sendFrame("clockSetHz", b"{\"cmdName\":\"clockSetHz\",\"clockHz\":250000}\0")
    time.sleep(1)
    for i in range(testRepeatCount):
        commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
        time.sleep(0.1)
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":32768,\"len\":10,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.1)
        commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":32768,\"len\":10,\"isIo\":0}\0")
        time.sleep(0.1)
        commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
        time.sleep(0.5)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgRdRespCount"] == testRepeatCount)
    assert(testStats["msgWrRespCount"] == testRepeatCount)
    assert(testStats["msgWrRespErrCount"] == 0)
    assert(testStats["msgWrRespErrMissingCount"] == 0)
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["clockSetOk"] == True)

def test_SetMc():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
        elif msgContent['cmdName'] == "getStatusResp":
            testStats['mcList'] = msgContent['machineList']
            if curMachine != "":
                assert(msgContent['machineCur'] == curMachine)
                testStats['mcCount'] += 1
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("SetMc")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    testRepeatCount = 2
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "mcList":[], "mcCount":0}

    # Get list of machines
    curMachine = ""
    commonTest.sendFrame("getStatus", b"{\"cmdName\":\"getStatus\"}\0")
    time.sleep(1)

    # Set machines alternately
    for i in range(testRepeatCount):
        for mc in testStats['mcList']:
            curMachine = mc
            logger.debug(f"Setting machine {mc}")
            commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
            time.sleep(3)
            commonTest.sendFrame("getStatus", b"{\"cmdName\":\"getStatus\"}\0")
            time.sleep(1)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["mcCount"] >= 4)

def test_StepValidateJMP000():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % testWriteData[i]) for i in range(len(testWriteData)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "WrResp":
            testStats["msgWrRespCount"] += 1
            try:
                if msgContent['err'] != 'ok':
                    testStats["msgWrRespErrCount"] += 1
                    logger.error(f"WrResp err not ok {msgContent}")
            except:
                logger.error(f"WrResp doesn't contain err {msgContent}")
                testStats["msgWrRespErrMissingCount"] += 1
        elif msgContent['cmdName'] == "busResetResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "clockSetHzResp":
            testStats["clockSetOk"] = msgContent['err'] == "ok"
        elif msgContent['cmdName'] == "validatorPrimeFromMemResp" or \
                msgContent['cmdName'] == "validatorStopResp" or \
                msgContent['cmdName'] == "validatorStartResp":
            pass
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                msgContent['cmdName'] == "busResetResp":
            pass
        elif msgContent['cmdName'] == "validatorStatusResp":
            logger.info(f"isrCount {msgContent['isrCount']} errors {msgContent['errors']}")
            testStats['stepValErrCount'] = msgContent['errors']
            testStats['isrCount'] = msgContent['isrCount']
            testStats['stepValRespCount'] += 1
            logger.info(f"StepValStatus ISRCount {msgContent['isrCount']} errors {msgContent['errors']}")
        elif msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
        elif msgContent['cmdName'] == "hwListResp" or \
            msgContent['cmdName'] == "hwEnableResp":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("StepValidateJMP000")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    # Test data - jump to 0000
    testWriteData = b"\xc3\x00\x00"
    testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0,
                 "unknownMsgCount":0, "isrCount":0, "stepValErrCount":0, "clrMaxUs":0, "stepValRespCount":0}
    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
    time.sleep(1)
    # Processor clock
    commonTest.sendFrame("clockSetHz", b"{\"cmdName\":\"clockSetHz\",\"clockHz\":250000}\0")
    time.sleep(1)
    # Check hardware list and set 64K RAM
    commonTest.sendFrame("hwList", b"{\"cmdName\":\"hwList\"}\0")
    commonTest.sendFrame("hwEnable", b"{\"cmdName\":\"hwEnable\",\"hwName\":\"64KRAM\",\"enable\":1}\0")
    time.sleep(1)

    # Repeat tests
    testRepeatCount = 10
    testValStatusCount = 10
    # Bus reset
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
    for i in range(testRepeatCount):

        # Send program
        time.sleep(0.1)
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"len\":3,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.1)
        # Start validator
        commonTest.sendFrame("valStop", b"{\"cmdName\":\"validatorStop\"}\0")
        commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":0,\"len\":3,\"isIo\":0}\0")
        time.sleep(0.5)
        commonTest.sendFrame("valPrime", b"{\"cmdName\":\"validatorPrimeFromMem\"}\0")   
        commonTest.sendFrame("valStart", b"{\"cmdName\":\"validatorStart\"}\0")   
        commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")   

        msgIdx = 0
        for j in range(10):
            commonTest.sendFrame("statusReq", b"{\"cmdName\":\"validatorStatus\",\"msgIdx\":\"" + bytes(str(msgIdx),'utf-8') + b"\"}\0")
            msgIdx += 1
            commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
            time.sleep(0.2)
    
        # Send messages to stop
        commonTest.sendFrame("valStop", b"{\"cmdName\":\"validatorStop\"}\0")
    # Bus reset
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgWrRespCount"] == testRepeatCount)
    assert(testStats["msgRdRespCount"] == testRepeatCount)
    assert(testStats["msgWrRespErrCount"] == 0)
    assert(testStats["msgWrRespErrMissingCount"] == 0)
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["clockSetOk"] == True)
    assert(testStats["isrCount"] > 0)
    assert(testStats["stepValRespCount"] == testRepeatCount * testValStatusCount)
    assert(testStats["stepValErrCount"] == 0)

def test_TRS80Level1RomExec():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
        elif "SetMachine=" in msgContent['cmdName'] or \
                    msgContent['cmdName'] == 'ClearTargetResp' or \
                    msgContent['cmdName'] == 'FileTargetResp':
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'ProgramAndResetResp':
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockSetHzResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busResetResp":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("TRS80Level1RomExec")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0}

    # Reset bus
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")

    # Set TRS80
    mc = "TRS80"
    TRS80ScreenAddr = 0x3c00 + 64
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Processor clock
    commonTest.sendFrame("clockSetHz", b"{\"cmdName\":\"clockSetHz\",\"clockHz\":250000}\0")

    # Loop through tests
    testRepeatCount = 5
    for i in range(testRepeatCount):

        # Remove test valid check text
        testWriteData = b"TESTING IN PROGRESS"
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":" + bytes(str(TRS80ScreenAddr),'utf-8') + b",\"len\":19,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.2)

        # Clear target
        commonTest.sendFrame("ClearTarget", b"{\"cmdName\":\"ClearTarget\"}\0")

        # Send ROM
        targetCodeFolder = r"./test/testdata"
        targetCodeFilename = r"level1.rom"
        romFrame = commonTest.formFileFrame(targetCodeFolder, targetCodeFilename)
        assert(not(romFrame is None))
        if not romFrame is None:
            # Send ROM data
            logger.debug(f"ROM frame len {len(romFrame)} start {romFrame[0:60]}")
            commonTest.sendFrame("Level1Rom", romFrame)

            # Program and reset
            commonTest.sendFrame("ProgramAndReset", b"{\"cmdName\":\"ProgramAndReset\"}\0")
            time.sleep(2)

            # Test memory at screen location
            readDataExpected = b"READY"
            rdLen = len(readDataExpected)
            commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":" + bytes(str(TRS80ScreenAddr),'utf-8') + b",\"len\":" + bytes(str(rdLen),'utf-8') + b",\"isIo\":0}\0")
            time.sleep(0.2)

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["programAndResetCount"] == testRepeatCount)
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgRdRespCount"] == testRepeatCount)

def test_GalaxiansExec():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
            testStats['iorqWr'] = msgContent['iorqWr']
            logger.debug(msgContent)
        elif "SetMachine=" in msgContent['cmdName'] or \
                    msgContent['cmdName'] == 'ClearTargetResp' or \
                    msgContent['cmdName'] == 'FileTargetResp':
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'ProgramAndResetResp' or \
                    msgContent['cmdName'] == "ProgramAndExecResp":
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockSetHzResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                    msgContent['cmdName'] == "busResetResp":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("GalaxiansExec")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqWr":0}

    # Reset bus
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
    commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")   

    # Set TRS80
    mc = "TRS80"
    TRS80ScreenAddr = 0x3c00 + 64
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Processor clock
    commonTest.sendFrame("clockSetHz", b"{\"cmdName\":\"clockSetHz\",\"clockHz\":250000}\0")

    # Loop through tests
    testRepeatCount = 1
    for i in range(testRepeatCount):

        # Remove test valid check text
        testWriteData = b"TESTING IN PROGRESS"
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":" + bytes(str(TRS80ScreenAddr),'utf-8') + b",\"len\":19,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.2)

        # Clear target
        commonTest.sendFrame("ClearTarget", b"{\"cmdName\":\"ClearTarget\"}\0")

        # Send ROM
        targetCodeFolder = r"./test/testdata"
        trs80L1RomFilename = r"level1.rom"
        romFrame = commonTest.formFileFrame(targetCodeFolder, trs80L1RomFilename)
        assert(not(romFrame is None))
        if not romFrame is None:
            # Send ROM data
            logger.debug(f"ROM frame len {len(romFrame)} start {romFrame[0:60]}")
            commonTest.sendFrame("Level1Rom", romFrame)

            # Program and reset
            commonTest.sendFrame("ProgramAndReset", b"{\"cmdName\":\"ProgramAndReset\"}\0")
            time.sleep(2)

            # Clear target
            commonTest.sendFrame("ClearTarget", b"{\"cmdName\":\"ClearTarget\"}\0")

            # Send Galaxians
            galaxiansFilename = r"galinv1d.cmd"
            programFrame = commonTest.formFileFrame(targetCodeFolder, galaxiansFilename)
            assert(not(programFrame is None))
            if not programFrame is None:
                # Send program data
                logger.debug(f"Galaxians frame len {len(programFrame)} start {programFrame[0:60]}")
                commonTest.sendFrame("Galaxians", programFrame)
                time.sleep(2)

                # Program and exec
                commonTest.sendFrame("ProgramAndExec", b"{\"cmdName\":\"ProgramAndExec\"}\0")
                time.sleep(2)

                # Check for IORQs
                commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
                time.sleep(2)

            # # Test memory at screen location
            # readDataExpected = b"READY"
            # rdLen = len(readDataExpected)
            # commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":" + bytes(str(TRS80ScreenAddr),'utf-8') + b",\"len\":" + bytes(str(rdLen),'utf-8') + b",\"isIo\":0}\0")
            # time.sleep(0.2)

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["programAndResetCount"] == testRepeatCount * 2)
    assert(testStats["msgRdOk"] == True)
    assert(testStats["iorqWr"] > 0)

def test_stepSingle():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
            testStats['iorqWr'] = msgContent['iorqWr']
            testStats['iorqRd'] = msgContent['iorqRd']
            testStats['mreqWr'] = msgContent['mreqWr']
            testStats['mreqRd'] = msgContent['mreqRd']
            logger.debug(msgContent)
        elif "SetMachine=" in msgContent['cmdName'] or \
                    msgContent['cmdName'] == 'ClearTargetResp' or \
                    msgContent['cmdName'] == 'targetResetResp' or \
                    msgContent['cmdName'] == 'targetTrackerOnResp' or \
                    msgContent['cmdName'] == 'stepIntoResp' or \
                    msgContent['cmdName'] == 'targetTrackerOffResp':
                    
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'ProgramAndResetResp' or \
                    msgContent['cmdName'] == "ProgramAndExecResp":
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockSetHzResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                    msgContent['cmdName'] == "busResetResp":
            pass
        elif msgContent['cmdName'] == "getRegsResp":
            testStats["AFOK"] = ("AF=0b" in msgContent['regs'])
            testStats["regsOk"] = True
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("StepSingle")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    # Test data - load A,6; inc A * 3 and jump to 0002
    testWriteData = b"\x3e\x06\x3c\x3c\x3c\xc3\x02\x00"
    testWriteLen = bytes(str(len(testWriteData)),'utf-8')
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqRd":0, "iorqWr":0, "mreqRd":0, "mreqWr":0, "AFOK": False, "regsOk": False}

    mc = "Serial Terminal"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Send program
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
    commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"len\":" + testWriteLen  + b",\"isIo\":0}\0" + testWriteData)

    # Setup Test
    commonTest.sendFrame("targetReset", b"{\"cmdName\":\"targetReset\"}\0")
    commonTest.sendFrame("targetTrackerOn", b"{\"cmdName\":\"targetTrackerOn\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
    time.sleep(1)

    # Check status
    commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
    commonTest.sendFrame("getRegs", b"{\"cmdName\":\"getRegs\"}\0")
    time.sleep(2)

    # Clear Test
    commonTest.sendFrame("targetTrackerOff", b"{\"cmdName\":\"targetTrackerOff\"}\0")

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["AFOK"])
    assert(testStats["regsOk"])

def test_regGetTest():

    def frameCallback(msgContent):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
            testStats['iorqWr'] = msgContent['iorqWr']
            testStats['iorqRd'] = msgContent['iorqRd']
            testStats['mreqWr'] = msgContent['mreqWr']
            testStats['mreqRd'] = msgContent['mreqRd']
            logger.debug(msgContent)
        elif "SetMachine=" in msgContent['cmdName'] or \
                    msgContent['cmdName'] == 'ClearTargetResp' or \
                    msgContent['cmdName'] == 'targetResetResp' or \
                    msgContent['cmdName'] == 'targetTrackerOnResp' or \
                    msgContent['cmdName'] == 'stepIntoResp' or \
                    msgContent['cmdName'] == 'targetTrackerOffResp':
                    
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'ProgramAndResetResp' or \
                    msgContent['cmdName'] == "ProgramAndExecResp":
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockSetHzResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                    msgContent['cmdName'] == "busResetResp":
            pass
        elif msgContent['cmdName'] == "getRegsResp":
            testStats["regsOk"] = (expectedRegs in msgContent['regs'])
            logger.debug(f"REGS {msgContent['regs']}")
            logger.debug(f"EXPECTED {expectedRegs}")
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    setupTests("regGetTest")
    commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)
    # Test data - sets all registers to known values
    # jump to 0002
    expectedRegs = "PC=002e SP=9b61 BC=3a2b AF=0330 HL=7a4e DE=5542 IX=9187 IY=f122 AF'=6123 BC'=83b2 HL'=2334 DE'=a202 I=03 R=75  F=---H---- F'=-------NC"
    testWriteData = b"\x3e\x25" \
                    b"\x01\xb2\x83" \
                    b"\x11\x02\xa2" \
                    b"\x21\x34\x23" \
                    b"\x92" \
                    b"\x08" \
                    b"\xd9" \
                    b"\x3e\xb1" \
                    b"\x01\x2b\x3a" \
                    b"\x11\x42\x55" \
                    b"\x21\x4e\x7a" \
                    b"\x31\x61\x9b" \
                    b"\x93" \
                    b"\x3c" \
                    b"\xdd\x21\x87\x91" \
                    b"\xfd\x21\x22\xf1" \
                    b"\xed\x4f" \
                    b"\x3e\x03" \
                    b"\xed\x47" \
                    b"\xed\x5e" \
                    b"\xc3\x02\x00"
    testWriteLen = bytes(str(len(testWriteData)),'utf-8')
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqRd":0, "iorqWr":0, "mreqRd":0, "mreqWr":0, "regsOk": False}

    mc = "Serial Terminal"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Send program
    commonTest.sendFrame("busReset", b"{\"cmdName\":\"busReset\"}\0")
    commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"len\":" + testWriteLen  + b",\"isIo\":0}\0" + testWriteData)

    # Setup Test
    commonTest.sendFrame("targetReset", b"{\"cmdName\":\"targetReset\"}\0")
    commonTest.sendFrame("targetTrackerOn", b"{\"cmdName\":\"targetTrackerOn\"}\0")
    time.sleep(.2)
    for i in range(35):
        commonTest.sendFrame("stepRun", b"{\"cmdName\":\"stepInto\"}\0")
        time.sleep(.2)

    # Check status
    commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
    commonTest.sendFrame("getRegs", b"{\"cmdName\":\"getRegs\"}\0")
    time.sleep(2)

    # Clear Test
    commonTest.sendFrame("targetTrackerOff", b"{\"cmdName\":\"targetTrackerOff\"}\0")

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["regsOk"])
