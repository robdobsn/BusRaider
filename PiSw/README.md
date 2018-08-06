# Pi Bus Raider 
## Raspberry Pi Display for vintage computers

Pi Bus Raider uses a Raspberry Pi Zero based PCB to snoop on the bus of a 
host vintage 8 bit CPU and provide things like a memory mapped display and
keyboard.

This work is inspired by PiGFX and RC2014

## How to run

1. Format an SD-card with FAT32 filesystem.
2. Copy ```bin/kernel.img``` in the root of the SD card along with the files
   ```start.elf``` and ```bootcode.bin``` that are commonly [distributed with
the Raspberry Pi](https://github.com/raspberrypi/firmware/tree/master/boot)
3. Add a new text file with a single line called config.txt containing:
```
init_uart_clock=3000000
```
4. Insert the card and reboot the Pi.

As soon as your raspi is turned on, the message "Pi Bus Raider" should be
displayed on the HDMI video stream.

## Compiling

To compile you will need to install a GNU ARM cross compiler toolchain and
ensure that  ```arm-none-eabi-gcc```, ```arm-none-eabi-as```
```arm-none-eabi-ld``` and ```arm-none-eabi-objcopy``` are in your PATH.

At this point, run:

```
$ make
```

in the root directory.
 

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
