import requests

url = "http://192.168.86.192/targetcmd/"
req = requests.request("get", url + "rawBusControlOn")
print(req.json())
req = requests.request("get", url + "rawBusWaitDisable")
req = requests.request("get", url + "rawBusWaitClear")
req = requests.request("get", url + "rawBusTake")

ramRomPageWrBase = '70'
ramRomPgenWrBase = '74'

req = requests.request("get", url + "rawBusSetAddress/" + ramRomPageWrBase)
print(req.json())
for i in range(8):
    if i % 2 == 0:
        req = requests.request("get", url + "rawBusSetData/01")
    else:
        req = requests.request("get", url + "rawBusSetData/00")
    req = requests.request("get", url + "rawBusSetLine/IORQ/0")
    req = requests.request("get", url + "rawBusSetLine/WR/0")
    req = requests.request("get", url + "rawBusSetLine/WR/1")
    req = requests.request("get", url + "rawBusSetLine/IORQ/1")
