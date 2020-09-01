cd circle &&\
# ./makeall clean &&\
./makeall &&\
cd ../src &&\
./makeall clean &&\
./makeall &&\
# arm-none-eabi-objcopy kernel7.elf -O ihex kernel7.hex &&\
cd .. 
# &&\
# python.exe UploadToI2CEmu.py I2CEmulator/kernel8-32.img --port $1 &&\
# python.exe I2CEmulatorMonitor.py --port $1
