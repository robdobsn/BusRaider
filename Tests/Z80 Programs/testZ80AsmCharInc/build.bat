zcc +z88 -startuplib --no-crt -startup=0 -zorg=0 testZ80ASMCharInc.asm -o testZ80ASMCharInc.bin -create-app
rem curl http://192.168.86.89/targetcmd/cleartarget
rem curl -F "file=@testZ80ASMCharInc.bin" http://192.168.86.89/upload
rem curl http://192.168.86.89/targetcmd/ProgramAndReset
