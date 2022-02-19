BUILDREV=${1:-BusRaider}
SERIALPORT=${2:-COM25}
BUILD_IDF_VERS=${3:-esp-idf-v4.0.1}
FSIMAGE=${4:-}
IPORHOSTNAME=${5:-}
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
python ./scripts/build.py buildConfigs/$BUILDREV &&\
if [ -z "$5" ]
  then
    if [ -z "$4" ]
    then
      python.exe scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDREV/partitions.csv builds/$BUILDREV $BUILDREV.bin $SERIALPORT -b2000000
    else
      python.exe scripts/flashUsingPartitionCSV.py buildConfigs/$BUILDREV/partitions.csv builds/$BUILDREV $BUILDREV.bin $SERIALPORT -b2000000 -f $4
    fi
  else
    echo Sending to RIC OTA
    curl -F "file=@./builds/$BUILDREV/$BUILDREV.bin" "http://$5/api/espFwUpdate"
fi
if [ $? -eq "0" ]
  then
    python.exe scripts/SerialMonitor.py $SERIALPORT -g
fi