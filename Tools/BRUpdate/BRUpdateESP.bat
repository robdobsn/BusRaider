@echo off

if [%1]==[] goto usage

echo "BRUpdateESP: Uploading ESP32 firmware to BusRaider at IP Address "%1
curl -F "data=@../../EspSw/BusRaiderESP32/.pioenvs/featheresp32/firmware.bin" "http://"%1"/espFirmwareUpdate"
echo ""

goto eof

:usage
    echo "usage: BRUpdateESP <IPADDR>"
    echo "Please specify BusRaider IP Address"

:eof
