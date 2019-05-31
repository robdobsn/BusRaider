#!/bin/bash

# sysinfo_page - A script to update the software in the BusRaider ESP32 and Pi

if [ "$1" = "" ]; then
    echo "usage: BRUpdateESP <IPADDR>"
    echo "Please specify BusRaider IP Address"
    exit 1
fi

echo "BRUpdateESP: Uploading ESP32 firmware to BusRaider at IP Address "$1
curl -F "data=@../../EspSw/BusRaiderESP32/.pioenvs/featheresp32/firmware.bin" "http://"$1"/espFirmwareUpdate"
echo ""
