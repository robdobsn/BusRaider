# BusRaider by Rob Dobson

>[Full documentation is in the Wiki](https://github.com/robdobsn/PiBusRaider/wiki)

## Memory Mapped Graphics and Single-Step Debugging for Retro Computer

BusRaider is an add-on circuit board for the RC2014 retro computer.
It uses a Raspberry Pi Zero and an ESP32 to snoop on the bus of a 
host vintage 8 bit CPU and provide things like a memory mapped display,
WiFi connection, keyboard and single-step debugging

## Getting Started

The following resources will help get started with BusRaider:
* [Build Video](https://www.youtube.com/watch?v=00ZwtiLc9rM)
* [Documentation](https://github.com/robdobsn/PiBusRaider)

Some background information about why BusRaider exists is in the blog posts [https://robdobson.com/2018/08/trs80-galaxy-invasion-on-rc2014/](https://robdobson.com/2018/08/trs80-galaxy-invasion-on-rc2014/)
and [https://robdobson.com/2018/08/trs80-galaxy-invasion-on-rc2014/](https://robdobson.com/2018/09/raid-the-bus-and-the-speccy-lives/)

## How to run the Pi software

1. Format an SD-card with FAT32 filesystem.
2. Copy ```PiSw/bin/kernel.img``` and ```config.txt``` from the latest release to the root of the SD card.
3. Add the following files ```start.elf``` and ```bootcode.bin``` that are commonly [distributed with
the Raspberry Pi](https://github.com/raspberrypi/firmware/tree/master/boot)
4. Insert the card and reboot the Pi on the Bus Raider PCB

As soon as your BusRaider is turned on, the message "RC2014 Bus Raider V2.x.xxx" should be
displayed on the HDMI video stream along with a display area for the Retro computer.

## Documentation

For full documentation see the [Documentation Wiki](https://github.com/robdobsn/PiBusRaider/wiki)

## License

The MIT License (MIT)

Copyright (c) 2018-2019 Rob Dobson

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
