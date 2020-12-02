zcc +z80 -startuplib --no-crt -startup=0 -zorg=0 testZ80AsmTestMem.asm -o testZ80AsmTestMem.bin -create-app
rem curl http://192.168.86.89/targetcmd/progClear
rem curl -F "file=@testZ80AsmTestMem.bin" http://192.168.86.89/upload
rem curl http://192.168.86.89/targetcmd/progWrite
rem curl http://192.168.86.89/targetcmd/resettarget