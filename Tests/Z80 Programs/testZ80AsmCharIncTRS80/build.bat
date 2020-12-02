zcc +z88 -startuplib --no-crt -startup=0 -zorg=0 testZ80ASMCharIncTRS80.asm -o testZ80ASMCharIncTRS80.bin -create-app
rem curl http://192.168.86.89/targetcmd/progClear
rem curl -F "file=@testZ80ASMCharInc.bin" http://192.168.86.89/upload
rem curl http://192.168.86.89/targetcmd/progWriteAndExec
