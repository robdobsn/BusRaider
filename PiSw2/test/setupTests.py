from .CommonTest import CommonTest
import os

def setupTests(testName, frameCallback):
    curWorkingFolder = os.getcwd()
    testBaseFolder = curWorkingFolder
    if not curWorkingFolder.endswith("test"):
        testBaseFolder = os.path.join(curWorkingFolder, "test")
    return CommonTest(
            testName = testName,
            testBaseFolder = testBaseFolder,
            dumpTextFileName = testName + ".log",
            dumpBinFileName = testName + ".bin",
            frameCallback = frameCallback
            )