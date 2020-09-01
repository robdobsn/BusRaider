cp config/Config.mk circle
cd circle &&\
# ./makeall clean &&\
./makeall &&\
cd ../src &&\
./makeall clean &&\
./makeall &&\
# arm-none-eabi-objcopy kernel7.elf -O ihex kernel7.hex &&\
cd .. &&\
python.exe tools/UploadToDev.py src/kernel.img --port $1 
# &&\
# python.exe I2CEmulatorMonitor.py --port $1
