import requests

url = "http://192.168.86.192/targetcmd/"
ramRomPageWrBase = 0x70
ramRomPgenWrBase = 0x74

def takeControl():
    req = requests.request("get", url + "rawBusControlOn")
    req = requests.request("get", url + "rawBusWaitDisable")
    req = requests.request("get", url + "rawBusWaitClear")
    req = requests.request("get", url + "rawBusTake")

def enablePaging():
    req = requests.request("get", url + "rawBusSetAddress/" + f"{ramRomPageWrBase:04x}")
    req = requests.request("get", url + "rawBusSetData/01")
    req = requests.request("get", url + "rawBusSetLine/IORQ/0")
    req = requests.request("get", url + "rawBusSetLine/WR/0")
    req = requests.request("get", url + "rawBusSetLine/WR/1")
    req = requests.request("get", url + "rawBusSetLine/IORQ/1")

def writeRegister(regIdx, val):
    req = requests.request("get", url + "rawBusSetAddress/" + f"{(ramRomPgenWrBase + regIdx):04x}")
    req = requests.request("get", url + "rawBusSetData/" + f"{val:02x}")
    req = requests.request("get", url + "rawBusSetLine/IORQ/0")
    req = requests.request("get", url + "rawBusSetLine/WR/0")
    req = requests.request("get", url + "rawBusSetLine/WR/1")
    req = requests.request("get", url + "rawBusSetLine/IORQ/1")

def writeData(addr, data):
    print(f"Writing addr {addr:02x} data {data:02x}")
    req = requests.request("get", url + "rawBusSetAddress/" + f"{addr:04x}")
    req = requests.request("get", url + "rawBusSetData/" + f"{data:02x}")
    req = requests.request("get", url + "rawBusSetLine/MREQ/0")
    req = requests.request("get", url + "rawBusSetLine/WR/0")
    req = requests.request("get", url + "rawBusSetLine/WR/1")
    req = requests.request("get", url + "rawBusSetLine/MREQ/1")

def readData(addr):
    req = requests.request("get", url + "rawBusSetAddress/" + f"{addr:04x}")
    req = requests.request("get", url + "rawBusGetData")
    req = requests.request("get", url + "rawBusSetLine/MREQ/0")
    req = requests.request("get", url + "rawBusSetLine/RD/0")
    dataVal = req.json()["pib"]
    req = requests.request("get", url + "rawBusSetLine/RD/1")
    req = requests.request("get", url + "rawBusSetLine/MREQ/1")
    return dataVal

takeControl()
enablePaging()
for i in range(4):
    writeRegister(i, i + 1 + 0x20)
    dataToWrite = (0x44 + i * 57) % 0xff
    # print(f"Writing {dataToWrite:02x}")
    writeData(0x0000 + i * 0x4000, dataToWrite)

for i in range(4):
    print("Read", readData(0x0000 + i * 0x4000))

