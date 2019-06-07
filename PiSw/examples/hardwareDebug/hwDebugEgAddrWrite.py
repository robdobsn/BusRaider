# Import the requests library
import requests

url = "http://192.168.86.192/targetcmd/"
# Disable BusRaider's managed control of the bus
req = requests.request("get", url + "rawBusControlOn")
print(req.json())
# Disable and clear WAIT state generation
req = requests.request("get", url + "rawBusWaitDisable")
req = requests.request("get", url + "rawBusWaitClear")
# Take control of the bus
req = requests.request("get", url + "rawBusTake")
# Set the address bus to 1234
req = requests.request("get", url + "rawBusSetAddress/1234")
# Set the data bus to 55
req = requests.request("get", url + "rawBusSetData/55")
# Write to memory at this address
req = requests.request("get", url + "rawBusSetLine/MREQ/0")
req = requests.request("get", url + "rawBusSetLine/WR/0")
req = requests.request("get", url + "rawBusSetLine/WR/1")
req = requests.request("get", url + "rawBusSetLine/MREQ/1")
