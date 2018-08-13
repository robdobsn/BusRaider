# Pi Bus Raider by Rob Dobson 2018
## Raspberry Pi Display for vintage computers

Pi Bus Raider uses a Raspberry Pi Zero based PCB to snoop on the bus of a 
host vintage 8 bit CPU and provide things like a memory mapped display and
keyboard. The PCB is designed for the RC2014.

This work is inspired by PiGFX and RC2014

More information is in the blog post https://robdobson.com/2018/08/trs80-galaxy-invasion-on-rc2014/

## How to run

1. Format an SD-card with FAT32 filesystem.
2. Copy ```PiSw/bin/kernel.img``` to the root of the SD card along with the files
   ```start.elf``` and ```bootcode.bin``` that are commonly [distributed with
the Raspberry Pi](https://github.com/raspberrypi/firmware/tree/master/boot)
3. Insert the card and reboot the Pi on the Bus Raider PCB

As soon as your raspi is turned on, the message "RC2014 Bus Raider V1.0" should be
displayed on the HDMI video stream.

## Compiling

To compile you will need to install a GNU ARM cross compiler toolchain and
ensure that  ```arm-none-eabi-gcc```, ```arm-none-eabi-as```
```arm-none-eabi-ld``` and ```arm-none-eabi-objcopy``` are in your PATH.

At this point, run:

```
$ make
```

in the PiSw directory.
 
## Development loop

To simplify development:
1. Replace the kernel.img file in the root of the Pi's SD card
with the bootloader1mbpskernel.img file in the bootloader folder (renaming
it to kernel.img). 
2. Then reboot the Pi each time you want to run a new test program and send (at 921600 baud)
the newly rebuilt (using make) pi_bus_raider.srec file in the PiSw folder using a serial
terminal software like TeraTerm. Indeed using TeraTerm you can drag and drop the .srec file
onto the terminal software and click to send
3. Press G in the terminal software and the bootloader on the Pi will run the software you just
sent down to it

## License

The MIT License (MIT)

Copyright (c) 2018 Rob Dobson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
