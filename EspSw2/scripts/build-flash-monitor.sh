SERIALPORT=${1:-COM5}
BUILD_IDF_VERS=${2:-esp-idf}
SERIALBAUD=${3:-2000000}
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
idf.py build &&\
if [ -z "$3" ]
  then
    python.exe ./scripts/flashUsingPartitionCSV.py partitions.csv build BusRaiderESP32.bin spiffs.bin $SERIALPORT -b$SERIALBAUD
  else
    echo Sending OTA
    curl -F "file=@./build/BusRaiderESP32.bin" "http://$3/espfwupdate"
fi
if [ $? -eq "0" ] 
  then
    python.exe ./scripts/SerialMonitor.py $SERIALPORT -g
fi
