# Import the requests library
import requests

url = "http://192.168.86.7/targetcmd/"
# Disable BusRaider's managed control of the bus
req = requests.request("get", url + "rawBusControlOn")
if req.apparent_encoding == "ascii":
    print(req.json()) 
else:
    print(f"Not working apparent_encoding {req.apparent_encoding}")
    if req.apparent_encoding == "ascii":
        print(req.content)
    exit()
# Disable and clear WAIT state generation
req = requests.request("get", url + "rawBusWaitDisable")
print(req.json()) 
req = requests.request("get", url + "rawBusWaitClear")
print(req.json()) 
# Take control of the bus
req = requests.request("get", url + "rawBusTake")
print(req.json()) 
# Set the address bus to 1234
req = requests.request("get", url + "rawBusSetAddress/1234")
print(req.json()) 
# Set the data bus to 55
req = requests.request("get", url + "rawBusSetData/55")
print(req.json()) 
# Write to memory at this address
req = requests.request("get", url + "rawBusSetLine/MREQ/0")
print(req.json()) 
req = requests.request("get", url + "rawBusSetLine/WR/0")
print(req.json()) 
req = requests.request("get", url + "rawBusSetLine/WR/1")
print(req.json()) 
req = requests.request("get", url + "rawBusSetLine/MREQ/1")
print(req.json()) 
req = requests.request("get", url + "rawBusRelease")
print(req.json()) 
req = requests.request("get", url + "rawBusControlOff")
print(req.json()) 
req = requests.request("get", url + "targetReset")
print(req.json()) 
