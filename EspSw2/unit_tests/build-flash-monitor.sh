SERIALPORT=${1:-COM5}
BUILD_IDF_VERS=${2:-esp-idf}
SERIALBAUD=${3:-2000000}
. $HOME/esp/${BUILD_IDF_VERS}/export.sh
idf.py build &&\
python.exe ../scripts/flashUsingPartitionCSV.py partitions.csv build ric_unit_tests.bin ../spiffs.bin $SERIALPORT -b$SERIALBAUD
python.exe ../scripts/SerialMonitor.py $SERIALPORT -g
