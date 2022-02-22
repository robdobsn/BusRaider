const USBKeys = require('./USBKeys.js');
const TRS80CmdFileFormat = require('./TRS80CmdFileFormat.js');

const TRS80_KEYBOARD_RAM_ADDR = 0x3800;
const TRS80_KEYBOARD_RAM_SIZE = 0x0100;
const TRS80_DISP_RAM_ADDR = 0x3c00;
const TRS80_DISP_RAM_SIZE = 0x400;
const TRS80_KEY_BYTES = 8;

class TRS80Machine {
    constructor(memAccess, z80Proc) {
        this.memAccess = memAccess;
        this.z80Proc = z80Proc;
        this.screenSize = [64, 16];

        // Setup memory size, etc
        this.memAccess.setup(0x10000, null, 0x10000, null);

        // Init joystick
        for (let i = 0; i < 256; i++) {
            this.memAccess.io_write((i << 8) + 0x13, 0xff);
        }
    }

    getScreenMem() {
        return this.memAccess.blockRead(0x3c00, 0x400, false);
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
        const dataStart = 16;
        const msgBuf = new Uint8Array(dataStart + screenInfo.length);
        msgBuf[0] = 0x00; // API version
        msgBuf[1] = 0x01; // Screen update
        msgBuf[2] = (screenSize[0] >> 8) & 0xff;
        msgBuf[3] = screenSize[0] & 0xff;
        msgBuf[4] = (screenSize[1] >> 8) & 0xff;
        msgBuf[5] = screenSize[1] & 0xff;
        msgBuf[6] = 0x00; // Character based screen
        msgBuf[7] = 0x01; // TRS80 font id
        msgBuf[8] = 0x00; // Buffer offset high
        msgBuf[9] = 0x00; // Buffer offet low
        msgBuf[10] = (screenInfo.length >> 8) & 0xff;
        msgBuf[11] = screenInfo.length & 0xff;
        msgBuf.set(screenInfo, dataStart);
        return msgBuf;
    }

    keyboardKey(keysDown) {

        // console.log(`updateKeyboardMemory keysDown ${JSON.stringify(keysDown)}`);

        // TRS80 keyboard is mapped as follows
        // Addr Bit 0       1       2       3       4       5       6       7
        // 3801     @       A       B       C       D       E       F       G
        // 3802     H       I       J       K       L       M       N       O
        // 3804     P       Q       R       S       T       U       V       W
        // 3808     X       Y       Z
        // 3810     0       1       2       3       4       5       6       7
        // 3820     8       9       *       +       <       =       >       ?
        // 3840     Enter   Clear   Break   Up      Down    Left    Right   Space
        // 3880     Shift   *****                   Control

        const keybdBytes = [TRS80_KEY_BYTES];
        for (let i = 0; i < TRS80_KEY_BYTES; i++)
            keybdBytes[i] = 0;

        // Go through key codes
        let suppressShift = 0;
        let suppressCtrl = 0;
        for (const [usbKeyCode, usbModifier] of Object.entries(keysDown)) {
            // Handle key
            if ((usbKeyCode >= USBKeys.KEY_A) && (usbKeyCode <= USBKeys.KEY_Z)) {
                // Handle A..Z
                const bitIdx = ((usbKeyCode - USBKeys.KEY_A) + 1) % 8;
                const keyPos = parseInt(((usbKeyCode - USBKeys.KEY_A) + 1) / 8);
                keybdBytes[keyPos] |= (1 << bitIdx);
                // console.log(`keybdBytes[${keyPos}] |= (1 << ${bitIdx}) = ${keybdBytes[keyPos]}`);
            } else if ((usbKeyCode == USBKeys.KEY_2) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) != 0)) {
                // Handle @
                keybdBytes[0] |= 1;
                suppressShift = 1;
            } else if ((usbKeyCode == USBKeys.KEY_6) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) != 0)) {
                // Handle ^
                suppressShift = 1;
            } else if ((usbKeyCode == USBKeys.KEY_7) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) != 0)) {
                // Handle &
                keybdBytes[4] |= 0x40;
            } else if ((usbKeyCode == USBKeys.KEY_8) && (usbModifier & USBKeys.KEY_MOD_LSHIFT)) {
                // Handle *
                keybdBytes[5] |= 4;
            } else if ((usbKeyCode == USBKeys.KEY_9) && (usbModifier & USBKeys.KEY_MOD_LSHIFT)) {
                // Handle (
                keybdBytes[5] |= 1;
                keybdBytes[7] |= 0x01;
            } else if ((usbKeyCode == USBKeys.KEY_0) && (usbModifier & USBKeys.KEY_MOD_LSHIFT)) {
                // Handle )
                keybdBytes[5] |= 2;
                keybdBytes[7] |= 0x01;
            } else if ((usbKeyCode >= USBKeys.KEY_1) && (usbKeyCode <= USBKeys.KEY_9)) {
                // Handle 1..9
                const bitIdx = ((usbKeyCode - USBKeys.KEY_1) + 1) % 8;
                const keyPos = parseInt(((usbKeyCode - USBKeys.KEY_1) + 1) / 8);
                keybdBytes[keyPos + 4] |= (1 << bitIdx);
            } else if (usbKeyCode == USBKeys.KEY_0) {
                // Handle 0
                keybdBytes[4] |= 1;
            } else if ((usbKeyCode == USBKeys.KEY_SEMICOLON) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) == 0)) {
                // Handle ;
                keybdBytes[5] |= 8;
                suppressShift = 1;
            } else if ((usbKeyCode == USBKeys.KEY_SEMICOLON) && (usbModifier & USBKeys.KEY_MOD_LSHIFT)) {
                // Handle :
                keybdBytes[5] |= 4;
                suppressShift = 1;
            } else if ((usbKeyCode == USBKeys.KEY_APOSTROPHE) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) == 0)) {
                // Handle '
                keybdBytes[4] |= 0x80;
                keybdBytes[7] |= 0x01;
            } else if ((usbKeyCode == USBKeys.KEY_APOSTROPHE) && (usbModifier & USBKeys.KEY_MOD_LSHIFT)) {
                // Handle "
                keybdBytes[4] |= 4;
                keybdBytes[7] |= 0x01;
            } else if (usbKeyCode == USBKeys.KEY_COMMA) {
                // Handle <
                keybdBytes[5] |= 0x10;
            } else if (usbKeyCode == USBKeys.KEY_DOT) {
                // Handle >
                keybdBytes[5] |= 0x40;
            } else if ((usbKeyCode == USBKeys.KEY_EQUAL) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) == 0)) {
                // Handle =
                keybdBytes[5] |= 0x20;
                keybdBytes[7] |= 0x01;
            } else if ((usbKeyCode == USBKeys.KEY_EQUAL) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) != 0)) {
                // Handle +
                keybdBytes[5] |= 0x8;
                keybdBytes[7] |= 0x01;
            } else if ((usbKeyCode == USBKeys.KEY_MINUS) && ((usbModifier & USBKeys.KEY_MOD_LSHIFT) == 0)) {
                // Handle -
                keybdBytes[5] |= 0x20;
                suppressShift = 1;
            } else if (usbKeyCode == USBKeys.KEY_SLASH) {
                // Handle ?
                keybdBytes[5] |= 0x80;
            } else if (usbKeyCode == USBKeys.KEY_ENTER) {
                // Handle Enter
                keybdBytes[6] |= 0x01;
            } else if (usbKeyCode == USBKeys.KEY_BACKSPACE) {
                // Treat as LEFT
                keybdBytes[6] |= 0x20;
            } else if (usbKeyCode == USBKeys.KEY_ESC) {
                // Handle Break
                keybdBytes[6] |= 0x04;
            } else if (usbKeyCode == USBKeys.KEY_UP) {
                // Handle Up
                keybdBytes[6] |= 0x08;
            } else if (usbKeyCode == USBKeys.KEY_DOWN) {
                // Handle Down
                keybdBytes[6] |= 0x10;
            } else if (usbKeyCode == USBKeys.KEY_LEFT) {
                // Handle Left
                keybdBytes[6] |= 0x20;
            } else if (usbKeyCode == USBKeys.KEY_RIGHT) {
                // Handle Right
                keybdBytes[6] |= 0x40;
            } else if (usbKeyCode == USBKeys.KEY_SPACE) {
                // Handle Space
                keybdBytes[6] |= 0x80;
            } else if (usbKeyCode == USBKeys.KEY_F1) {
                // Handle CLEAR
                keybdBytes[6] |= 0x02;
            } else if (usbKeyCode == USBKeys.KEY_LEFTSHIFT) {
                // Handle Left Shift
                keybdBytes[7] |= 0x01;
            } else if (usbKeyCode == USBKeys.KEY_RIGHTSHIFT) {
                // Handle Left Shift
                keybdBytes[7] |= 0x02;
            } else if ((usbKeyCode == USBKeys.KEY_LEFTCTRL) || (usbKeyCode == USBKeys.KEY_RIGHTCTRL)) {
                // Handle <
                keybdBytes[7] |= 0x10;
            }
        }

        // Suppress shift keys if needed
        if (suppressShift) {
            keybdBytes[7] &= 0xfc;
        }
        if (suppressCtrl) {
            keybdBytes[7] &= 0xef;
        }

        let keybdString = "";
        for (let j = 0; j < 8; j++) {
            keybdString += `0x${keybdBytes[j].toString(16)} `;
        }
        // console.log(keybdString);

        // Build RAM map
        for (let i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++) {
            // Clear initially
            let kbdVal = 0;
            // Set all locations that would be set in real TRS80 due to
            // matrix operation of keyboard on address lines
            for (let j = 0; j < TRS80_KEY_BYTES; j++) {
                if (i & (1 << j))
                    kbdVal |= keybdBytes[j];
            }
            // Store to memory
            this.memAccess.mem_write(TRS80_KEYBOARD_RAM_ADDR + i, kbdVal);
            // console.log(`Keyboard RAM[${i}] = ${kbdVal}`);
        }
    }

    loadFile(filename) {
        const fileExt = filename.substr(filename.lastIndexOf('.') + 1).toLowerCase();
        if (fileExt === 'cmd') {
            const cmdFileFormat = new TRS80CmdFileFormat();
            cmdFileFormat.process(filename, this.memAccess, this.z80Proc);
        }
    }
}

module.exports = TRS80Machine;