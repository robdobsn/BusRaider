const USBKeys = require('./USBKeys.js');
const Snapshots = require('./SpectrumSnapshots.js')

const SPECTRUM_KEYBOARD_IO_LOW_ADDR = 0xFE;
const SPECTRUM_KEYBOARD_IO_SIZE = 0x0100;
const SPECTRUM_DISP_RAM_ADDR = 0x4000;
const SPECTRUM_DISP_RAM_SIZE = 6144;
const SPECTRUM_ATTR_RAM_ADDR = 0x5800;
const SPECTRUM_ATTR_RAM_SIZE = 768;
const SPECTRUM_DISP_AND_ATTR_SIZE = SPECTRUM_DISP_RAM_SIZE + SPECTRUM_ATTR_RAM_SIZE;
const SPECTRUM_TSTATES_PER_INT = 69888;
const SYM_SHIFT = 0x80;
const CAPS_SHIFT = 0x40;


class SpectrumMachine {
    constructor(memAccess, z80Proc) {
        this.memAccess = memAccess;
        this.z80Proc = z80Proc;
        this.screenSize = [256, 192];
        this.intOnTstates = SPECTRUM_TSTATES_PER_INT;
        this.keyCodes = {};
        this.modKeyCodes = {};
        this.extKeyCodes = {};
        this.setupKeyCodes();

        // Setup memory size, etc
        this.memAccess.setup(0x10000, null, 0x10000, this.modifyIOAddr);    
    }

    getScreenMem() {
        return this.memAccess.blockRead(SPECTRUM_DISP_RAM_ADDR, SPECTRUM_DISP_AND_ATTR_SIZE, false);
    }

    getScreenSize() {
        return this.screenSize;
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

    modifyIOAddr(addr) {
        // Check for keyboard decoding
        if ((addr & 0x01) === 0) {
            addr = (addr & 0xff00) + 0xfe;
        }
        return addr;
    }

    keyboardKey(keysDown) {
        // Clear keyboard io space
        for (let i = 0; i < SPECTRUM_KEYBOARD_IO_SIZE; i++) {
            this.memAccess.io_write((i << 8) + SPECTRUM_KEYBOARD_IO_LOW_ADDR, 0xff);
        }

        // Go through non-spectrum keys (like ' and " on regular keyboard)
        let addrVal = 0;
        let bitPos = 0;
        let suppressProcessing = false;
        for (const [usbKeyCode, usbModifier] of Object.entries(keysDown)) {
            if (usbKeyCode in this.extKeyCodes) {
                suppressProcessing = true;
                const ioVals = this.extKeyCodes[usbKeyCode];
                if (usbModifier & (USBKeys.KEY_MOD_LSHIFT | USBKeys.KEY_MOD_RSHIFT)) {
                    this.setIOForKey(ioVals[1]);
                } else {
                    this.setIOForKey(ioVals[0]);
                }
            }
        }

        // Handle ZX Spectrum keys
        if (!suppressProcessing) {
            // Handle key
            for (const [usbKeyCode, usbModifier] of Object.entries(keysDown)) {
                if (usbKeyCode in this.keyCodes) {
                    const ioVals = this.keyCodes[usbKeyCode];
                    suppressProcessing = this.setIOForKey(ioVals);
                }
            }
        }

        // Handle modifier keys
        if (!suppressProcessing) {
            // Handle key
            for (const [usbKeyCode, usbModifier] of Object.entries(keysDown)) {
                if (usbKeyCode in this.modKeyCodes) {
                    const ioVals = this.modKeyCodes[usbKeyCode];
                    this.setIOForKey(ioVals);
                }
            }
        }
        
        // Display keyboard memory
        const kbdMem = new Uint8Array(SPECTRUM_KEYBOARD_IO_SIZE);
        for (let i = 0; i < SPECTRUM_KEYBOARD_IO_SIZE; i++) {
            const val = this.memAccess.io_read((i << 8) + SPECTRUM_KEYBOARD_IO_LOW_ADDR);
            kbdMem[i] = val;
        }
        // console.log(`keyboard mem ${this.memAccess.bufToHex(kbdMem, "")}`);
    }

    setIOForKey(ioVals) {
        // Main key
        let modifierSet = false;
        const ioAddr = ioVals[0];
        const ioBitPos = ioVals[1];
        this.setIOBits(ioAddr, ioBitPos);

        // Modifier keys
        if (ioVals.length > 2) {
            for (let i = 2; i < ioVals.length; i++) {
                const modKey = ioVals[i];
                if (modKey & SYM_SHIFT) {
                    this.setIOBits(15, 1);
                    modifierSet = true;
                } else if (modKey & CAPS_SHIFT) {
                    this.setIOBits(8, 0);
                    modifierSet = true;
                }
            }
        }
        return modifierSet;
    }

    setIOBits(ioAddr, ioBitPos) {
        const ioAddrMask = (1 << (ioAddr-8));
        const ioBitPosMask = ~(1 << ioBitPos);
        // console.log(`setIOBits ${ioAddr} ${ioBitPos} ${ioAddrMask} ${ioBitPosMask}`)
        for (let i = 0; i < SPECTRUM_KEYBOARD_IO_SIZE; i++) {
            // console.log(`${i & ioAddrMask}`)
            if ((i & ioAddrMask) === 0) {
                const ioAd = (i << 8) + SPECTRUM_KEYBOARD_IO_LOW_ADDR;
                const curVal = this.memAccess.io_read(ioAd, true);
                this.memAccess.io_write(ioAd, curVal & ioBitPosMask, true);
                // console.log(`setIOBits ${ioAd} ${ioBitPosMask} ${curVal} ${this.memAccess.io_read(ioAd, true)}`)
            }
        }
    }

    setupKeyCodes() {
        this.keyCodes[USBKeys.KEY_A] = [9, 0];
        this.keyCodes[USBKeys.KEY_B] = [15,4];
        this.keyCodes[USBKeys.KEY_C] = [8, 3];
        this.keyCodes[USBKeys.KEY_D] = [9, 2];
        this.keyCodes[USBKeys.KEY_E] = [10,2];
        this.keyCodes[USBKeys.KEY_F] = [9 ,3];
        this.keyCodes[USBKeys.KEY_G] = [9, 4];
        this.keyCodes[USBKeys.KEY_H] = [14,4];
        this.keyCodes[USBKeys.KEY_I] = [13,2];
        this.keyCodes[USBKeys.KEY_J] = [14,3];
        this.keyCodes[USBKeys.KEY_K] = [14,2];
        this.keyCodes[USBKeys.KEY_L] = [14,1];
        this.keyCodes[USBKeys.KEY_M] = [15,2];
        this.keyCodes[USBKeys.KEY_N] = [15,3];
        this.keyCodes[USBKeys.KEY_O] = [13,1];
        this.keyCodes[USBKeys.KEY_P] = [13,0];
        this.keyCodes[USBKeys.KEY_Q] = [10,0];
        this.keyCodes[USBKeys.KEY_R] = [10,3];
        this.keyCodes[USBKeys.KEY_S] = [9, 1];
        this.keyCodes[USBKeys.KEY_T] = [10,4];
        this.keyCodes[USBKeys.KEY_U] = [13,3];
        this.keyCodes[USBKeys.KEY_V] = [8, 4];
        this.keyCodes[USBKeys.KEY_W] = [10,1];
        this.keyCodes[USBKeys.KEY_X] = [8, 2];
        this.keyCodes[USBKeys.KEY_Y] = [13,4];
        this.keyCodes[USBKeys.KEY_Z] = [8, 1];
        this.keyCodes[USBKeys.KEY_0] = [12,0];
        this.keyCodes[USBKeys.KEY_1] = [11,0];
        this.keyCodes[USBKeys.KEY_2] = [11,1];
        this.keyCodes[USBKeys.KEY_3] = [11,2];
        this.keyCodes[USBKeys.KEY_4] = [11,3];
        this.keyCodes[USBKeys.KEY_5] = [11,4];
        this.keyCodes[USBKeys.KEY_6] = [12,4];
        this.keyCodes[USBKeys.KEY_7] = [12,3];
        this.keyCodes[USBKeys.KEY_8] = [12,2];
        this.keyCodes[USBKeys.KEY_9] = [12,1];

        this.keyCodes[USBKeys.KEY_ENTER] = [14, 0];
        this.keyCodes[USBKeys.KEY_SPACE] = [15, 0];
        this.keyCodes[USBKeys.KEY_BACKSPACE] = [12, 0, CAPS_SHIFT];
        
        this.modKeyCodes[USBKeys.KEY_LEFTCTRL] = [15, 1];
        this.modKeyCodes[USBKeys.KEY_RIGHTCTRL] = [15, 1];
        this.modKeyCodes[USBKeys.KEY_LEFTALT] = [15, 1];
        this.modKeyCodes[USBKeys.KEY_RIGHTALT] = [15, 1];
        this.modKeyCodes[USBKeys.KEY_LEFTSHIFT] = [8, 0];
        this.modKeyCodes[USBKeys.KEY_RIGHTSHIFT] = [8, 0]

        this.extKeyCodes[USBKeys.KEY_MINUS] = [[14,3,SYM_SHIFT],[12,0,SYM_SHIFT]];
        this.extKeyCodes[USBKeys.KEY_APOSTROPHE] = [[12,3,SYM_SHIFT],[13,0,SYM_SHIFT]];

    }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Keyboard handling
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// uint32_t McZXSpectrum::getKeyBitmap(const int* keyCodes, int keyCodesLen, 
//     const uint8_t* currentKeyPresses)
// {
// uint32_t retVal = 0xff;
// for (int i = 0; i < MAX_KEYS; i++)
// {
// int bitMask = 0x01;
// for (int j = 0; j < keyCodesLen; j++)
// {
//     if (currentKeyPresses[i] == keyCodes[j])
//         retVal &= ~bitMask;
//     bitMask = bitMask << 1;
// }
// }
// return retVal;
// }

// // Handle a key press
// void McZXSpectrum::keyHandler( unsigned char ucModifiers, 
//          const unsigned char* rawKeys)
// {
// // Note that in the following I've used the KEY_HANJA as a placeholder
// // as I think it is a key that won't normally occur

// // Clear key buf scratchpad 
// _keyBuf.clearScratch();

// // Check shift & ctrl
// bool isShift = ((ucModifiers & KEY_MOD_LSHIFT) != 0) || ((ucModifiers & KEY_MOD_RSHIFT) != 0);
// bool isCtrl = ((ucModifiers & KEY_MOD_LCTRL) != 0) || ((ucModifiers & KEY_MOD_RCTRL) != 0);

// // Regular keys mapped to Spectrum keys
// struct KeyCodeDef {
// uint8_t regularKeyCode;
// bool isShifted;
// uint8_t matrixIdx;
// uint8_t matrixCode;
// uint8_t shiftCode;
// };
// static const KeyCodeDef regularKeyMapping[] = {
// {KEY_EQUAL, false, 6, 0xfd, 0xfd},
// {KEY_EQUAL, true, 6, 0xfb, 0xfd},
// };

// // Check for regular key codes in the key buffer
// bool specialKeyBackspace = false;
// for (int i = 0; i < MAX_KEYS; i++)
// {
// if (rawKeys[i] == KEY_BACKSPACE)
//     specialKeyBackspace = true;

// // Check regular keys
// for (unsigned j = 0; j < sizeof(regularKeyMapping)/sizeof(KeyCodeDef); j++)
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "check regular mapping %d isShift %d code %02x",
//     //                     j, isShift, regularKeyMapping[j].regularKeyCode);
//     if ((rawKeys[i] == regularKeyMapping[j].regularKeyCode) && (regularKeyMapping[j].isShifted == isShift))
//     {
//         _keyBuf.andScratch(regularKeyMapping[j].matrixIdx, regularKeyMapping[j].matrixCode);
//         _keyBuf.andScratch(ZXSPECTRUM_SHIFT_KEYS_ROW_IDX, regularKeyMapping[j].shiftCode);
//     }
// }
// }

// // Key table
// static const int keyTable[ZXSPECTRUM_KEYBOARD_NUM_ROWS][ZXSPECTRUM_KEYS_IN_ROW] = {
//     {KEY_HANJA, KEY_Z, KEY_X, KEY_C, KEY_V},
//     {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G},
//     {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T},
//     {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5},
//     {KEY_0, KEY_9, KEY_8, KEY_7, KEY_6},
//     {KEY_P, KEY_O, KEY_I, KEY_U, KEY_Y},
//     {KEY_ENTER, KEY_L, KEY_K, KEY_J, KEY_H},
//     {KEY_SPACE, KEY_HANJA, KEY_M, KEY_N, KEY_B}
// };

// // Handle encoding of keys
// for (int keyRow = 0; keyRow < ZXSPECTRUM_KEYBOARD_NUM_ROWS; keyRow++)
// {
// uint32_t keyBits = getKeyBitmap(keyTable[keyRow], ZXSPECTRUM_KEYS_IN_ROW, rawKeys);
// _keyBuf.andScratch(keyRow, keyBits);
// }

// // Handle CAPS-SHIFT (shift on std keyboard) key modifier (inject one for backspace)
// if (specialKeyBackspace || isShift)
// _keyBuf.andScratch(ZXSPECTRUM_BS_KEY_ROW_IDX, 0xfe);

// // Handle modifier for delete (on the zero key)
// if (specialKeyBackspace)
// _keyBuf.andScratch(ZXSPECTRUM_DEL_KEY_ROW_IDX, 0xfe);

// // Handle Sym key (ctrl)
// if (isCtrl)
// _keyBuf.andScratch(ZXSPECTRUM_SHIFT_KEYS_ROW_IDX, 0xfd);

// // LogWrite(MODULE_PREFIX, LOG_DEBUG, "keyHandler mod %02x Key[0] %02x Key[1] %02x KeyBits %02x %02x %02x %02x %02x %02x %02x %02x", 
// //                 ucModifiers, rawKeys[0], rawKeys[1],
// //                 _keyBuf.getScratchBitMap(0),
// //                 _keyBuf.getScratchBitMap(1),
// //                 _keyBuf.getScratchBitMap(2),
// //                 _keyBuf.getScratchBitMap(3),
// //                 _keyBuf.getScratchBitMap(4),
// //                 _keyBuf.getScratchBitMap(5),
// //                 _keyBuf.getScratchBitMap(6),
// //                 _keyBuf.getScratchBitMap(7)
// //                 );

// // Add to keyboard queue
// _keyBuf.putFromScratch();
// }

    loadFile(filename) {
        const fileExt = filename.substr(filename.lastIndexOf('.') + 1).toLowerCase();
        if (fileExt === 'z80') {
            const fs = require('fs');
            const pData = fs.readFileSync(filename);
            const dataBytes = new Uint8Array(pData);
            console.log(`${dataBytes.length} ${typeof dataBytes}`);
            const snapshot = Snapshots.parseZ80File(dataBytes.buffer);

            for (const prop in snapshot.memoryPages) {
                const page = snapshot.memoryPages[prop];
                console.log(`${prop} ${page.length}`);
                const memAddrs = {5: 0x4000, 1: 0x8000, 2: 0xc000};
                this.memAccess.blockWrite(memAddrs[prop], page, false);
            }
            console.log(`${JSON.stringify(snapshot.registers)}`);
            this.setZ80Registers(snapshot.registers);

            // Set registers etc
            // this.memAccess.blockWrite()
            // cmdFileFormat.process(filename, this.memAccess, this.z80Proc);
        }
    }

    setZ80Registers(registers) {
        const flags = registers.AF & 0xff;
        const flags_ = registers.AF_ & 0xff;
        const z80State = {
            flags: {
                S: (flags & 0x80) !== 0,
                Z: (flags & 0x40) !== 0,
                Y: (flags & 0x20) !== 0,
                H: (flags & 0x10) !== 0,
                X: (flags & 0x08) !== 0,
                P: (flags & 0x04) !== 0,
                N: (flags & 0x02) !== 0,
                C: (flags & 0x01) !== 0
            },
            flags_prime: {
                S: (flags_ & 0x80) !== 0,
                Z: (flags_ & 0x40) !== 0,
                Y: (flags_ & 0x20) !== 0,
                H: (flags_ & 0x10) !== 0,
                X: (flags_ & 0x08) !== 0,
                P: (flags_ & 0x04) !== 0,
                N: (flags_ & 0x02) !== 0,
                C: (flags_ & 0x01) !== 0
            },
            a: registers.AF >> 8,
            b: registers.BC >> 8,
            c: registers.BC & 0xff,
            d: registers.DE >> 8,
            e: registers.DE & 0xff,
            h: registers.HL >> 8,
            l: registers.HL & 0xff,
            a_prime: registers.AF_ >> 8,
            b_prime: registers.BC_ >> 8,
            c_prime: registers.BC_ & 0xff,
            d_prime: registers.DE_ >> 8,
            e_prime: registers.DE_ & 0xff,
            h_prime: registers.HL_ >> 8,
            l_prime: registers.HL_ & 0xff,
            sp: registers.SP,
            pc: registers.PC,
            i: registers.IR >> 8,
            r: registers.IR & 0xff,
            imode: registers.im,
            iff1: registers.iff1,
            iff2: registers.iff2,
            ix: registers.IX,
            iy: registers.IY,
            halted: false,
            do_delayed_di: false,
            do_delayed_ei: false,
            cycle_counter: 0,
        }
        this.z80Proc.setState(z80State);
    }

}

module.exports = SpectrumMachine;