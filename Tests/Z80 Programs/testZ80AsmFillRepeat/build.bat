zcc +z80 -startuplib --no-crt -startup=0 -zorg=0 testZ80AsmFillRepeat.asm -o testZ80ASMFillRepeat.bin -create-app
curl http://192.168.86.89/targetcmd/imagerClear
curl -F "file=@testZ80ASMFillRepeat.bin" http://192.168.86.89/upload
curl http://192.168.86.89/targetcmd/imagerWriteAndExec
