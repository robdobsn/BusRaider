BUILDREV=${1:-BusRaider}
SERIALPORT=${2:-COM25}
BUILD_IDF_VERS=${3:-esp-idf-v4.0.1}
SERIALBAUD=${4:-2000000}
IPORHOSTNAME=${5:-}
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python ./scripts/build.py buildConfigs/$BUILDREV &&\
if [ -z "$5" ]
  then
    python.exe ./scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDREV/partitions.csv builds/$BUILDREV BusRaider.bin spiffs.bin $SERIALPORT -b$SERIALBAUD
  else
    echo Sending to RIC OTA
    curl -F "file=@./builds/$BUILDREV/BusRaider.bin" "http://$5/api/espFwUpdate"
fi
if [ $? -eq "0" ] 
  then
    python.exe scripts/SerialMonitor.py $SERIALPORT -g
fi