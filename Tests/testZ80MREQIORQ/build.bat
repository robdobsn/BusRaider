zcc +z80 -startuplib --no-crt -startup=0 -zorg=0 main.asm -o main.bin -create-app
curl http://192.168.86.89/targetcmd/cleartarget
curl -F "file=@main.bin" http://192.168.86.89/upload
curl http://192.168.86.89/targetcmd/programtarget
curl http://192.168.86.89/targetcmd/resettarget
