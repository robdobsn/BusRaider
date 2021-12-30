const USBKeys = require('./USBKeys.js');

const SPECTRUM_KEYBOARD_RAM_ADDR = 0x3800;
const SPECTRUM_KEYBOARD_RAM_SIZE = 0x0100;
const SPECTRUM_DISP_RAM_ADDR = 0x4000;
const SPECTRUM_DISP_RAM_SIZE = 6144;
const SPECTRUM_ATTR_RAM_ADDR = 0x5800;
const SPECTRUM_ATTR_RAM_SIZE = 768;
const SPECTRUM_DISP_AND_ATTR_SIZE = SPECTRUM_DISP_RAM_SIZE + SPECTRUM_ATTR_RAM_SIZE;


class SpectrumMachine {
    constructor(memAccess, z80Proc) {
        this.memAccess = memAccess;
        this.z80Proc = z80Proc;

    }

    getScreenMem() {
        return this.memAccess.blockRead(SPECTRUM_DISP_RAM_ADDR, SPECTRUM_DISP_AND_ATTR_SIZE, false);
    }

    getScreenSize() {
        return [256, 192];
    }

    getScreenIfUpdated(screenCache) {
        const screenMem = this.getScreenMem();
        const isChanged = !("cache" in screenCache) || !screenCache.cache.equals(screenMem);
        if (isChanged) {
            screenCache.cache = Buffer.from(screenMem);
            // console.log(`screen changed len ${screenCache.cache.length}`);
            return screenMem
        }
        return null;
    }

    getScreenUpdateMsg(screenInfo) {
        const screenSize = this.screenSize;
        const msgBuf = new Uint8Array(10 + screenInfo.length);
        msgBuf[0] = 0x00; // API version
        msgBuf[1] = 0x01; // Screen update
        msgBuf[2] = (screenSize[0] >> 8) & 0xff;
        msgBuf[3] = screenSize[0] & 0xff;
        msgBuf[4] = (screenSize[1] >> 8) & 0xff;
        msgBuf[5] = screenSize[1] & 0xff;
        msgBuf[6] = 0x01; // Pixel based screen
        msgBuf[7] = 0x01; // Spectrum layout
        msgBuf[8] = 0x00; // Reserved
        msgBuf[9] = 0x00; // Reserved
        msgBuf.set(screenInfo, 10);
        return msgBuf;
    }

    keyboardKey(keysDown) {
    }

    loadFile(filename) {
    }
}

module.exports = SpectrumMachine;