zcc +z80 -startuplib --no-crt -startup=0 -zorg=0 testZ80AsmFill.asm -o testZ80ASMFill.bin -create-app
curl http://192.168.86.89/targetcmd/cleartarget
curl -F "file=@testZ80ASMFill.bin" http://192.168.86.89/upload
curl http://192.168.86.89/targetcmd/programtarget
curl http://192.168.86.89/targetcmd/resettarget