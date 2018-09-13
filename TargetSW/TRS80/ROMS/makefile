
all: level1.srec level2.srec model3.srec

%.srec : %.rom
	arm-none-eabi-objcopy $< -O srec $@
	
