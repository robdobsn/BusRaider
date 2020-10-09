cp config/Config.mk circle
cd circle &&\
# ./makeall clean &&\
./makeall &&\
cd ../src &&\
./makeall clean &&\
./makeall &&\
# arm-none-eabi-objcopy kernel7.elf -O ihex kernel7.hex &&\
cd .. &&\
python tools/CalcCRC.py src/kernel.img
# python.exe tools/UploadToDev.py src/kernel.img --port $1 
if [ -z "$1" ]
  then
    echo "Done"
  else
    curl -F "file=@./src/kernel.img" http://$1/uploadpisw
fi
# &&\
# python.exe I2CEmulatorMonitor.py --port $1
