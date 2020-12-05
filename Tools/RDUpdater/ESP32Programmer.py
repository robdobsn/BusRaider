from typing import Dict, List, Tuple
from esptool import ESPLoader
from gen_esp32part import PartitionTable

class ESP32Programmer():

    def handleFileSend(self, esp, fileDict: Dict) -> Tuple[bool, str]:
        # Check partition info
        if "partitions.bin" in fileDict:
            partFileName = fileDict["partitions.bin"].get("tmpFileName","")
            if len(partFileName) != 0:
                with open(partFileName, "rb") as partFile:
                    partFileContents = partFile.read()
                    partTable = PartitionTable.from_binary(partFileContents)
                    partTable.verify()
                    print(partTable.to_csv())

    def program(self, port: str, fileDefs: List[Dict]) -> Tuple[bool, str]:
        # Put files in dict
        fileDict = {}
        for fdef in fileDefs:
            fileDict[fdef.get("name","")] = fdef
        esp = None
        try:
            esp = ESPLoader.detect_chip(port, ESPLoader.ESP_ROM_BAUD)
        except PermissionError as excp:
            esp = None
            return False, str(excp)
        except Exception as excp:
            esp = None
            return False, str(excp)
        try:
            if esp.CHIP_NAME != "ESP32":
                esp._port.close()
                esp = None
                return False, "ESP chip detected is not ESP32"
            rslt, errStr = self.handleFileSend(esp, fileDict)
            esp._port.close()
            esp = None
            return rslt, errStr
        except Exception as excp:
            esp._port.close()
            esp = None
            return False, str(excp)
        return False, ""
