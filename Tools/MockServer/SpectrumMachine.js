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

    keyboardKey(keysDown) {
    }

    loadFile(filename) {
    }
}

module.exports = SpectrumMachine;