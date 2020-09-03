from CommonTest import CommonTest
import time
import logging
import random
import os
from datetime import datetime
import json

# This is a program for testing the BusRaider firmware
# It requires either:
# a) a BusRaider supporting the CommandHandler protocol (HDLC) and operating on a serial port
#   (BusRaider jumpers configured to connect Pi serial to FTDI)
# b) a BusRaider with ESP32 firmware for uploading (normal firmware) and configured
#    with ESP Serial 0 to Pi
# c) most tests also require Clock from BusRaider to be enabled, Ram board in place

readDataExpected = b""

def setupTests(testName, frameCallback):
    curWorkingFolder = os.getcwd()
    testBaseFolder = curWorkingFolder
    if not curWorkingFolder.endswith("test"):
        testBaseFolder = os.path.join(curWorkingFolder, "test")
    return CommonTest(
            testName = testName,
            testBaseFolder = testBaseFolder,
            useIP = True, 
            serialPort = "COM6", 
            serialBaud = 921600, 
            ipAddrOrHostName = "192.168.86.40",
            dumpTextFileName = testName + ".log",
            dumpBinFileName = testName + ".bin",
            frameCallback = frameCallback
            )

def test_Comms():
    def frameCallback(frameJson, logger):
        assert(frameJson['cmdName'] == "comtestResp")
        if frameJson['cmdName'] == "comtestResp":
            testStats["framesRx"] += 1
            # print(frameJson['msgIdx'])
            testStats["msgRx"][frameJson['msgIdx']] = True

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("Comms", frameCallback)
    msgIdx = 0
    testRepeatCount = 100
    testStats = {"framesRx": 0, "msgRx":[False]*testRepeatCount}
    for i in range(testRepeatCount):
        commonTest.sendFrame("statusReq", b"{\"cmdName\":\"comtest\",\"msgIdx\":\"" + bytes(str(msgIdx),'utf-8') + b"\"}\0")
        msgIdx += 1
        # time.sleep(0.001)
    time.sleep(1)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["framesRx"] == testRepeatCount)
    for i in range(testRepeatCount):
        assert(testStats["msgRx"][i] == True)

def test_MemRW():

    def frameCallback(msgContent, logger):
        logger.info(f"FrameRx:{msgContent}")
        if msgContent['cmdName'] == "RdResp":
            curReadPos = len(readData)
            requiredResp = ''.join(('%02x' % writtenData[curReadPos][i]) for i in range(len(writtenData[curReadPos])))
            readData.append(msgContent['data'])
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "WrResp":
            testStats["msgWrRespCount"] += 1
            try:
                if msgContent['err'] != 'ok':
                    testStats["msgWrRespErrCount"] += 1
                    logger.error(f"WrResp err not ok {msgContent}")
            except:
                logger.error(f"WrResp doesn't contain err {msgContent}")
                testStats["msgWrRespErrMissingCount"] += 1
        elif msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "clockHzSetResp":
            testStats["clockSetOk"] = True
        else:
            testStats["unknownMsgCount"] += 1

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("MemRW", frameCallback)
    testRepeatCount = 20
    # Test data
    writtenData = []
    readData = []
    testWriteData = bytearray(b"\xaa\x55\xaa\x55\xaa\x55\xaa\x55\xaa\x55")
    testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0, "unknownMsgCount":0, "clockSetOk":False}
    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal ANSI"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
    time.sleep(1)
    # Processor clock
    commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")
    time.sleep(1)
    # Bus init
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    time.sleep(0.01)
    for i in range(testRepeatCount):
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":8000,\"lenDec\":10,\"isIo\":0}\0" + testWriteData)
        writtenData.append(testWriteData)
        time.sleep(0.01)
        commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":8000,\"lenDec\":10,\"isIo\":0}\0")
        time.sleep(0.01)
        if not testStats["msgRdOk"]:
            break
        testWriteData = bytearray(len(testWriteData))
        for i in range(len(testWriteData)):
            testWriteData[i] = random.randint(0,255)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgRdRespCount"] == testRepeatCount)
    assert(testStats["msgWrRespCount"] == testRepeatCount)
    assert(testStats["msgWrRespErrCount"] == 0)
    assert(testStats["msgWrRespErrMissingCount"] == 0)
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["clockSetOk"] == True)

def test_BankedMemRW():

    def frameCallback(msgContent, logger):
        logger.info(f"FrameRx:{msgContent}")
        if msgContent['cmdName'] == "RdResp":
            print("RdResp", msgContent)
            addr = int(msgContent["addr"], 0)
            dataLen = int(msgContent["len"])
            writtenData = testWriteData[addr:addr+dataLen]
            requiredResp = ''.join(('%02x' % writtenData[i]) for i in range(len(writtenData)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "WrResp":
            testStats["msgWrRespCount"] += 1
            try:
                if msgContent['err'] != 'ok':
                    testStats["msgWrRespErrCount"] += 1
                    logger.error(f"WrResp err not ok {msgContent}")
            except:
                logger.error(f"WrResp doesn't contain err {msgContent}")
                testStats["msgWrRespErrMissingCount"] += 1
        elif msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'] == "hwListResp" or \
            msgContent['cmdName'] == "hwEnableResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "ResetTargetResp":
            pass
        elif msgContent['cmdName'] == "clockHzSetResp":
            testStats["clockSetOk"] = True
        else:
            testStats["unknownMsgCount"] += 1
            print("UnknownResponse", msgContent)

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("BankedMemRW", frameCallback)
    testRepeatCount = 5
    numTestBlocks = 20
    # Test data
    writtenData = []
    testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0, "unknownMsgCount":0, "clockSetOk":False}
    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal ANSI"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
    time.sleep(1)
    # Processor clock
    commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")
    time.sleep(1)
    # Hardware
    commonTest.sendFrame("hwList", b"{\"cmdName\":\"hwList\"}\0")
    commonTest.sendFrame("hwEnable", b"{\"cmdName\":\"hwEnable\",\"hwName\":\"RAMROM\",\"enable\":1,\"pageOut\":\"busPAGE\",\"bankHw\":\"LINEAR\",\"memSizeK\":\"1024\"}\0")
    # Bus init
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    # Send JMP 0000 instructions to avoid Z80 messing with memory while we test it
    z80Jmp0000 = b"\xc3\x00\x00"
    commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"lenDec\":3,\"isIo\":0}\0" + z80Jmp0000)
    # Reset processor
    commonTest.sendFrame("Z80Reset", b"{\"cmdName\":\"ResetTarget\"}\0")
    time.sleep(0.01)
    testMemorySize = 512 * 1024
    blocksWritten = 0
    blocksRead = 0
    for i in range(testRepeatCount):
        testBlockLen = 1024
        testWriteData = bytearray(os.urandom(testMemorySize))
        print("Last byte", hex(testWriteData[-1]))
        curAddr = 0x010000
        testBlockInc = (testMemorySize - curAddr) // numTestBlocks
        for testBlockNum in range(numTestBlocks):
            apiCmd = {"cmdName":"Wr", "isIo":0}
            apiCmd["addr"] = hex(curAddr)
            apiCmd["lenDec"] = str(testBlockLen)
            writtenData = testWriteData[curAddr:curAddr+testBlockLen]
            cmdFrame = json.dumps(apiCmd).encode() + b"\0" + writtenData
            commonTest.sendFrame("blockWrite", cmdFrame, "WrResp")
            commonTest.awaitResponse(1000)
            curAddr += testBlockInc
            writtenDataHex = ''.join(('%02x' % writtenData[i]) for i in range(len(writtenData)))
            print("Written addr", apiCmd["addr"], "len", str(len(writtenData)), "data", writtenDataHex)
            blocksWritten += 1
        
        time.sleep(1)
        # Now read blocks back in reverse order
        curAddr -= testBlockInc
        for testBlockNum in range(numTestBlocks):
            apiCmd = {"cmdName":"Rd", "isIo":0}
            apiCmd["addr"] = hex(curAddr)
            apiCmd["lenDec"] = str(testBlockLen)
            commonTest.sendFrame("blockRead", json.dumps(apiCmd).encode() + b"\0", "RdResp")
            commonTest.awaitResponse(1000)
            if not testStats["msgRdOk"]:
                break
            curAddr -= testBlockInc
            blocksRead += 1

        time.sleep(1)
    
    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgRdRespCount"] == blocksRead)
    assert(testStats["msgWrRespCount"] == blocksWritten+1)
    assert(testStats["msgWrRespErrCount"] == 0)
    assert(testStats["msgWrRespErrMissingCount"] == 0)
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["clockSetOk"] == True)

def test_SetMc():

    def frameCallback(msgContent, logger):
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
    commonTest = setupTests("SetMc", frameCallback)
    testRepeatCount = 2
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "mcList":[], "mcCount":0}

    # Get list of machines
    curMachine = ""
    commonTest.sendFrame("getStatus", b"{\"cmdName\":\"getStatus\"}\0")
    time.sleep(1)

    # Set machines alternately
    for i in range(testRepeatCount):
        for mc in testStats['mcList']:
            logger.debug(f"Setting machine {mc}")
            commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
            time.sleep(2)
            curMachine = mc
            commonTest.sendFrame("getStatus", b"{\"cmdName\":\"getStatus\"}\0")
            time.sleep(.1)
    time.sleep(1)

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["mcCount"] >= 4)

def test_TraceJMP000():

    def frameCallback(msgContent, logger):
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
        elif msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'][:10] == "SetMachine":
            pass
        elif msgContent['cmdName'] == "clockHzSetResp":
            testStats["clockSetOk"] = True
        elif msgContent['cmdName'] == "tracerPrimeFromMemResp" or \
                msgContent['cmdName'] == "tracerStopResp" or \
                msgContent['cmdName'] == "tracerStartResp":
            pass
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'] == "tracerStatusResp":
            logger.info(f"isrCount {msgContent['isrCount']} errors {msgContent['errors']}")
            testStats['tracerErrCount'] += msgContent['errors']
            testStats['isrCount'] += msgContent['isrCount']
            testStats['tracerRespCount'] += 1
            logger.info(f"TracerStatus ISRCount {msgContent['isrCount']} errors {msgContent['errors']}")
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
    commonTest = setupTests("TraceJMP000", frameCallback)
    # Test data - jump to 0000
    testWriteData = b"\xc3\x00\x00"
    testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0,
                 "unknownMsgCount":0, "isrCount":0, "tracerErrCount":0, "clrMaxUs":0, "tracerRespCount":0}
    # Set serial terminal machine - to avoid conflicts with display updates, etc
    mc = "Serial Terminal ANSI"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
    time.sleep(1)
    # Processor clock
    commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")
    time.sleep(0.1)
    # Check hardware list and set RAM enabled
    commonTest.sendFrame("hwList", b"{\"cmdName\":\"hwList\"}\0")
    commonTest.sendFrame("hwEnable", b"{\"cmdName\":\"hwEnable\",\"hwName\":\"RAMROM\",\"enable\":1}\0")
    time.sleep(0.1)

    # Repeat tests
    testRepeatCount = 10
    testValStatusCount = 1
    # Bus init
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    commonTest.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\",\"logging\":1}\0")
    time.sleep(0.1)
    for i in range(testRepeatCount):

        testStats['isrCount'] = 0

        # Send program
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"lenDec\":3,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.01)
        # Start tracer
        commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":0,\"lenDec\":3,\"isIo\":0}\0")
        commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")
        time.sleep(0.01)
        commonTest.sendFrame("tracerStart", b"{\"cmdName\":\"tracerStart\",\"logging\":1,\"compare\":1,\"primeFromMem\":1}\0")

        # Run for some seconds
        time.sleep(1)

        # Get status
        msgIdx = 0
        for j in range(testValStatusCount):
            commonTest.sendFrame("statusReq", b"{\"cmdName\":\"tracerStatus\",\"msgIdx\":\"" + bytes(str(msgIdx),'utf-8') + b"\"}\0")
            msgIdx += 1
            commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
            time.sleep(0.1)

        # Stop tracer
        commonTest.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")
        time.sleep(0.1)

        # Breakout early if failing
        if not testStats["msgRdOk"] or testStats['tracerErrCount'] > 0:
            break
    
    # Bus init
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    time.sleep(1)
    
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
    assert(testStats["tracerRespCount"] == testRepeatCount * testValStatusCount)
    assert(testStats["tracerErrCount"] == 0)

# def test_TraceManic():

#     def frameCallback(msgContent, logger):
#         if msgContent['cmdName'] == "RdResp":
#             requiredResp = ''.join(('%02x' % testWriteData[i]) for i in range(len(testWriteData)))
#             respOk = requiredResp == msgContent['data']
#             testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
#             testStats["msgRdRespCount"] += 1
#             if not respOk:
#                 logger.debug(f"Read {msgContent['data']} != expected {requiredResp}")
#         elif msgContent['cmdName'] == "WrResp":
#             testStats["msgWrRespCount"] += 1
#             try:
#                 if msgContent['err'] != 'ok':
#                     testStats["msgWrRespErrCount"] += 1
#                     logger.error(f"WrResp err not ok {msgContent}")
#             except:
#                 logger.error(f"WrResp doesn't contain err {msgContent}")
#                 testStats["msgWrRespErrMissingCount"] += 1
#         elif msgContent['cmdName'] == "busInitResp":
#             pass
#         elif msgContent['cmdName'][:10] == "SetMachine":
#             pass
#         elif msgContent['cmdName'] == "clockHzSetResp":
#             testStats["clockSetOk"] = True
#         elif msgContent['cmdName'] == "tracerPrimeFromMemResp" or \
#                 msgContent['cmdName'] == "tracerStopResp" or \
#                 msgContent['cmdName'] == "tracerStartResp":
#             pass
#         elif msgContent['cmdName'] == "busStatusClearResp" or \
#                 msgContent['cmdName'] == "busInitResp":
#             pass
#         elif msgContent['cmdName'] == "tracerStatusResp":
#             logger.info(f"isrCount {msgContent['isrCount']} errors {msgContent['errors']}")
#             testStats['tracerErrCount'] += msgContent['errors']
#             testStats['isrCount'] += msgContent['isrCount']
#             testStats['tracerRespCount'] += 1
#             logger.info(f"TracerStatus ISRCount {msgContent['isrCount']} errors {msgContent['errors']}")
#         elif msgContent['cmdName'] == "busStatusResp":
#             testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
#         elif msgContent['cmdName'] == "hwListResp" or \
#             msgContent['cmdName'] == "hwEnableResp":
#             pass
#         else:
#             testStats["unknownMsgCount"] += 1
#             logger.info(f"Unknown message {msgContent}")

#     logger = logging.getLogger(__name__)
#     logger.setLevel(logging.DEBUG)
#     setupTests("TraceManic")
#     commonTest.setup(useIP, serialPort, serialSpeed, ipAddrOrHostName, logMsgDataFileName, logTextFileName, frameCallback)

#     # Stats
#     testStats = {"msgRdOk": True, "msgRdRespCount":0, "msgWrRespCount": 0, "msgWrRespErrCount":0, "msgWrRespErrMissingCount":0,
#                 "unknownMsgCount":0, "isrCount":0, "tracerErrCount":0, "clrMaxUs":0, "tracerRespCount":0}

#     # Set ZXSpectrum
#     mc = "ZX Spectrum"
#     commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\" }\0")
#     time.sleep(1)

#     # Check hardware list and set RAM
#     commonTest.sendFrame("hwList", b"{\"cmdName\":\"hwList\"}\0")
#     commonTest.sendFrame("hwEnable", b"{\"cmdName\":\"hwEnable\",\"hwName\":\"RAMROM\",\"enable\":1}\0")
#     time.sleep(0.1)

#     # Bus init
#     commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")

#     # Send ROM
#     targetCodeFolder = r"./test/testdata"
#     zxSpectrum48RomFilename = r"48.rom"
#     romFrame = commonTest.formFileFrame(targetCodeFolder, zxSpectrum48RomFilename)
#     assert(not(romFrame is None))
#     if not romFrame is None:
#         # Send ROM data
#         logger.debug(f"ROM frame len {len(romFrame)} start {romFrame[0:60]}")
#         commonTest.sendFrame("ZXSpectrum40KROM", romFrame)

#         # Program and reset
#         commonTest.sendFrame("ProgramAndReset", b"{\"cmdName\":\"ProgramAndReset\"}\0")
#         time.sleep(2)

#     # Test
#     testValStatusCount = 1

#     # Start tracer
#     commonTest.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")
#     time.sleep(0.1)
#     commonTest.sendFrame("tracerPrime", b"{\"cmdName\":\"tracerPrimeFromMem\"}\0")   
#     commonTest.sendFrame("tracerStart", b"{\"cmdName\":\"tracerStart\",\"logging\":1,\"compare\":1}\0")
#     commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")   

#     # Run for a long time
#     for i in range(3600):

#         # Get status
#         commonTest.sendFrame("statusReq", b"{\"cmdName\":\"tracerStatus\",\"msgIdx\":0}\0")
#         commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
#         time.sleep(10)

#         # Breakout early if failing
#         if not testStats["msgRdOk"] or testStats['tracerErrCount'] > 0:
#             break
        
#     # Send messages to stop
#     commonTest.sendFrame("tracerStop", b"{\"cmdName\":\"tracerStop\"}\0")

#     # Bus init
#     commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
#     time.sleep(1)
    
#     # Wait for test end and cleardown
#     commonTest.cleardown()
#     assert(testStats["msgRdOk"] == True)
#     assert(testStats["msgWrRespCount"] == testRepeatCount)
#     assert(testStats["msgRdRespCount"] == testRepeatCount)
#     assert(testStats["msgWrRespErrCount"] == 0)
#     assert(testStats["msgWrRespErrMissingCount"] == 0)
#     assert(testStats["unknownMsgCount"] == 0)
#     assert(testStats["clockSetOk"] == True)
#     assert(testStats["isrCount"] > 0)
#     assert(testStats["tracerRespCount"] == testRepeatCount * testValStatusCount)
#     assert(testStats["tracerErrCount"] == 0)

def test_TRS80Level1RomExec():

    def frameCallback(msgContent, logger):
        if msgContent['cmdName'] == "busStatusResp":
            testStats['clrMaxUs'] = max(testStats['clrMaxUs'], msgContent['clrMaxUs'])
        elif "SetMachine=" in msgContent['cmdName'] or \
                    msgContent['cmdName'] == 'ClearTargetResp' or \
                    msgContent['cmdName'] == 'FileTargetResp':
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'ProgramAndResetResp':
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockHzSetResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busInitResp":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("TRS80Level1RomExec", frameCallback)
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0}

    # Bus init
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")

    # Set TRS80
    mc = "TRS80"
    TRS80ScreenAddr = '3c40'
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Processor clock
    commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":250000}\0")

    # Loop through tests
    testRepeatCount = 5
    for i in range(testRepeatCount):

        # Remove test valid check text
        testWriteData = b"TESTING IN PROGRESS"
        readDataExpected = testWriteData
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":" + bytes(TRS80ScreenAddr,'utf-8') + b",\"lenDec\":19,\"isIo\":0}\0" + testWriteData)
        time.sleep(0.2)
        rdLen = len(readDataExpected)
        commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":" + bytes(TRS80ScreenAddr,'utf-8') + b",\"lenDec\":" + bytes(str(rdLen),'utf-8') + b",\"isIo\":0}\0")
        time.sleep(0.5)

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
            readDataExpected = b"READY"
            rdLen = len(readDataExpected)
            time.sleep(1.5)

            # Test memory at screen location
            commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":" + bytes(TRS80ScreenAddr,'utf-8') + b",\"lenDec\":" + bytes(str(rdLen),'utf-8') + b",\"isIo\":0}\0")
            time.sleep(0.5)

            # Breakout early if failing
            if not testStats["msgRdOk"]:
                break

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["programAndResetCount"] == testRepeatCount)
    assert(testStats["msgRdOk"] == True)
    assert(testStats["msgRdRespCount"] == testRepeatCount * 2)

def test_GalaxiansExec():

    def frameCallback(msgContent, logger):
        global readDataExpected
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
        elif msgContent['cmdName'] == "clockHzSetResp":
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
                    msgContent['cmdName'] == "busInitResp":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("GalaxiansExec", frameCallback)
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqWr":0}

    # Init bus
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    commonTest.sendFrame("busStatusClear", b"{\"cmdName\":\"busStatusClear\"}\0")   

    # Set TRS80
    mc = "TRS80"
    TRS80ScreenAddr = '3c40'
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1)

    # Processor clock
    commonTest.sendFrame("clockHzSet", b"{\"cmdName\":\"clockHzSet\",\"clockHz\":5000000}\0")

    # Loop through tests
    testRepeatCount = 1
    for i in range(testRepeatCount):

        # Remove test valid check text
        testWriteData = b"TESTING IN PROGRESS"
        commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":" + bytes(TRS80ScreenAddr,'utf-8') + b",\"lenDec\":19,\"isIo\":0}\0" + testWriteData)
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
            # commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":" + bytes(str(TRS80ScreenAddr),'utf-8') + b",\"lenDec\":" + bytes(str(rdLen),'utf-8') + b",\"isIo\":0}\0")
            # time.sleep(0.2)

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["programAndResetCount"] == testRepeatCount * 2)
    assert(testStats["msgRdOk"] == True)
    assert(testStats["iorqWr"] > 0)

def test_stepSingle_RequiresPaging():

    def frameCallback(msgContent, logger):
        global readDataExpected
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
                    msgContent['cmdName'] == 'targetTrackerOffResp':                    
            assert(msgContent['err'] == 'ok')
        elif msgContent['cmdName'] == 'stepIntoResp':
            testStats['stepCount'] += 1
        elif msgContent['cmdName'] == 'ProgramAndResetResp' or \
                    msgContent['cmdName'] == "ProgramAndExecResp":
            assert(msgContent['err'] == 'ok')
            testStats['programAndResetCount'] += 1
        elif msgContent['cmdName'] == "clockHzSetResp":
            pass
        elif msgContent['cmdName'] == "WrResp":
            pass
        elif msgContent['cmdName'] == "RdResp":
            requiredResp = ''.join(('%02x' % readDataExpected[i]) for i in range(len(readDataExpected)))
            respOk = requiredResp == msgContent['data']
            testStats["msgRdOk"] = testStats["msgRdOk"] and respOk
            testStats["msgRdRespCount"] += 1
            if not respOk:
                logger.error(f"Read {msgContent['data']} != expected {requiredResp}")
        elif msgContent['cmdName'] == "busStatusClearResp" or \
                    msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'] == "getRegsResp":
            logger.info(f"Expected register value {regsExpectedContent}")
            logger.info(f"{msgContent['regs']}")
            testStats["AFOK"] = (regsExpectedContent in msgContent['regs'])
            if not testStats["AFOK"]:
                logger.error(f"AFOK not ok! {msgContent}")
            testStats["regsOk"] = True
        elif msgContent['cmdName'] == "stepIntoDone":
            pass
        elif msgContent['cmdName'] == "targetTrackerOnDone":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("StepSingle", frameCallback)
    # Test data - load A,6; inc A * 3 and jump to 0002
    testWriteData = b"\x3e\x06\x3c\x3c\x3c\xc3\x02\x00"
    testWriteLen = bytes(str(len(testWriteData)),'utf-8')
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqRd":0, "iorqWr":0, "mreqRd":0, "mreqWr":0, "AFOK": False, "regsOk": False, "stepCount":0}

    mc = "Serial Terminal ANSI"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(.2)

    # Send program and check it
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    time.sleep(.1)
    commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"lenDec\":" + testWriteLen  + b",\"isIo\":0}\0" + testWriteData)
    readDataExpected = testWriteData
    time.sleep(.1)
    commonTest.sendFrame("blockRead", b"{\"cmdName\":\"Rd\",\"addr\":0,\"lenDec\":" + testWriteLen  + b",\"isIo\":0}\0")
    time.sleep(0.1)

    # Setup Test
    commonTest.sendFrame("targetTrackerOn", b"{\"cmdName\":\"targetTrackerOn\",\"reset\":1}\0", "targetTrackerOnDone")
    commonTest.awaitResponse(2000)
    numTestLoops = 20
    expectedAFValue = 5
    codePos = 0
    for i in range(numTestLoops):
        # Run a step
        commonTest.sendFrame("stepInto", b"{\"cmdName\":\"stepInto\"}\0", "stepIntoDone")
        commonTest.awaitResponse(2000)
        if codePos > 0 and codePos <=3:
            expectedAFValue += 1
        codePos += 1
        if codePos >= 5:
            codePos = 1

    # Expected regs
    regsExpectedContent = f"AF={expectedAFValue:02x}"
    time.sleep(.1)

    # Check status
    commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
    commonTest.sendFrame("getRegs", b"{\"cmdName\":\"getRegs\"}\0")
    time.sleep(.2)

    # Clear Test
    commonTest.sendFrame("targetTrackerOff", b"{\"cmdName\":\"targetTrackerOff\"}\0")

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["msgRdOk"])
    assert(testStats["AFOK"])
    assert(testStats["regsOk"])
    assert(testStats["stepCount"] == numTestLoops)
    assert(testStats["unknownMsgCount"] == 0)
    logger.debug(f"StepCount {testStats['stepCount']}")

def test_regGetTest_requiresPaging():

    def frameCallback(msgContent, logger):
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
        elif msgContent['cmdName'] == "clockHzSetResp":
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
                    msgContent['cmdName'] == "busInitResp":
            pass
        elif msgContent['cmdName'] == "getRegsResp":
            regsCount = len(regsGot)
            regsGot.append(msgContent['regs'])
            newRegsOk = (regsExpected[regsCount] in msgContent['regs'])
            if not newRegsOk:
                logger.error(f"Regs not as expected at pos {regsCount} {regsExpected[regsCount]} != {msgContent['regs']}")
            testStats["regsOk"] = testStats["regsOk"] and newRegsOk
        elif msgContent['cmdName'] == "stepIntoDone":
            pass
        elif msgContent['cmdName'] == "targetTrackerOnDone":
            pass
        else:
            testStats["unknownMsgCount"] += 1
            logger.info(f"Unknown message {msgContent}")

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    commonTest = setupTests("RegGet", frameCallback)
    # Test data - sets all registers to known values
    # jump to 0000
    expectedRegs = "PC=0002 SP=9b61 BC=3a2b AF=2530 HL=7a4e DE=5542 IX=9187 IY=f122 AF'=8387 BC'=83b2 HL'=2334 DE'=a202 I=03 R=77  F=---H---- F'=S-----PNC"
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
                    b"\xc3\x00\x00"
    testInstrLens = [2,3,3,3,1,1,1,2,3,3,3,3,1,1,4,4,2,2,2,2,3]
    testWriteLen = bytes(str(len(testWriteData)),'utf-8')
    testStats = {"unknownMsgCount":0, "clrMaxUs":0, "programAndResetCount":0, "msgRdOk":True, "msgRdRespCount":0,
            "iorqRd":0, "iorqWr":0, "mreqRd":0, "mreqWr":0, "regsOk": True}

    mc = "Serial Terminal ANSI"
    commonTest.sendFrame("SetMachine", b"{\"cmdName\":\"SetMachine=" + bytes(mc,'utf-8') + b"\"}\0")
    time.sleep(1.5)

    # Send program
    commonTest.sendFrame("busInit", b"{\"cmdName\":\"busInit\"}\0")
    time.sleep(.2)
    commonTest.sendFrame("blockWrite", b"{\"cmdName\":\"Wr\",\"addr\":0,\"lenDec\":" + testWriteLen  + b",\"isIo\":0}\0" + testWriteData)

    # Setup Test
    commonTest.sendFrame("targetTrackerOn", b"{\"cmdName\":\"targetTrackerOn\",\"reset\":1}\0", "targetTrackerOnDone")
    commonTest.awaitResponse(2000)
    regsExpected = []
    regsGot = []
    addr = 0
    for i in range(50):
        # logger.error(f"i={i}")
        commonTest.sendFrame("stepInto", b"{\"cmdName\":\"stepInto\"}\0", "stepIntoDone")
        commonTest.awaitResponse(2000)
        regsStr = f"PC={addr:04x}"
        regsExpected.append(regsStr)
        commonTest.sendFrame("getRegs", b"{\"cmdName\":\"getRegs\"}\0", "getRegsResp")
        # print(str(datetime.now()))
        commonTest.awaitResponse(1000)
        addr += testInstrLens[i % len(testInstrLens)]
        if i % len(testInstrLens) == len(testInstrLens) - 1:
            addr = 0
    
    # Check status
    commonTest.sendFrame("busStatus", b"{\"cmdName\":\"busStatus\"}\0")
    time.sleep(2)

    # Clear Test
    commonTest.sendFrame("targetTrackerOff", b"{\"cmdName\":\"targetTrackerOff\"}\0")

    # Wait for test end and cleardown
    commonTest.cleardown()
    assert(testStats["unknownMsgCount"] == 0)
    assert(testStats["regsOk"])

