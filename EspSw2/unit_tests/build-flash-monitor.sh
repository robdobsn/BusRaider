FLASHPORT=${1:-COM5}
FLASHBAUD=${2:-2000000}
. $HOME/esp/esp-idf/export.sh
idf.py build &&\
python.exe $IDF_PATH/components/esptool_py/esptool/esptool.py -p$FLASHPORT -b$FLASHBAUD --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xe000 build/ota_data_initial.bin 0x10000 build/ric_unit_tests.bin &&\
python.exe ../SerialMonitor.py $FLASHPORT
