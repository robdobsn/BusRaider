const Z80 = require('./Z80');
const MemAccess = require('./MemAccess');
const Z80TRS80 = require('./TRS80Machine');
class Z80System {
    constructor() {
        this.memAccess = new MemAccess();
        this.z80Proc = new Z80(this.memAccess);
        this.machine = null;
        this.isRunning = false;
        this.keysDown = {};
        this.resetOnExec = true;
        this.dumpBytesBeforePC = 0x40;
        this.dumpBytesTotal = 0x100;
        this.execbps = {};
    }

    setMachine(systemName) {
        console.log(`setMachine ${systemName}`);
        if (systemName === "TRS80") {
            this.machine = new Z80TRS80(this.memAccess, this.z80Proc);
        }
    }

    load(filename) {
        this.resetOnExec = true;
        const fileExt = filename.substr(filename.lastIndexOf('.') + 1).toLowerCase();
        // console.log(`load ${filename} ${fileExt}`);
        if ((fileExt === 'bin') || (fileExt === 'rom')) {
            this.memAccess.load(filename);
        } else if (fileExt === 'cmd') {
            if (this.machine) {
                this.machine.loadCmdFile(filename);
                this.resetOnExec = false;
            }
        }
    }

    exec() {
        // Reset if required
        if (this.resetOnExec) {
            this.z80Proc.reset();
        } else {
            // console.log(`exec from ${this.z80Proc.pc.toString(16)}`);
            // this.memAccess.dumpMem(0x5b00, 0x100);
            // this.memAccess.dumpMem(0x7f00, 0x100);
        }
        for (let i = 0; i < 10; i++) {
            console.log(`--------------------------------------------------------------------------------`);
            let curState = this.z80Proc.getState();
            console.log(curState);
            this.z80Proc.run_instruction();
        }
        this.isRunning = true; 
        // curState = this.z80Proc.getState();
        // console.log(curState);
        // this.memAccess.dumpMem(0x3c00, 0x400);
        this.processorTick = setInterval(() => {
            for (let i = 0; i < 100; i++) {
                if (!this.isRunning) {
                    break;
                }
                this.step();
            }
        }, 1);
    }

    step() {
        this.z80Proc.run_instruction();
        if (this.z80Proc.getState().pc in this.execbps) {
            this.isRunning = false;
        }
    }

    break() {
        this.isRunning = false;
    }

    continue() {
        this.isRunning = true;
    }

    getDump(addr, size) {
        return this.memAccess.getDump(addr, size);
    }

    getState() {
        const regs = this.z80Proc.getState();
        let memDumpStart = regs.pc - this.dumpBytesBeforePC;
        if (memDumpStart < 0) {
            memDumpStart = 0;
        }
        return { 
            regs: regs,
            addr: memDumpStart,
            mem: this.memAccess.getDump(memDumpStart, this.dumpBytesTotal)
        };
    }

    updateMirrorScreen(websocket, screenCache) {
        // console.log(`updateMirrorScreen screenCache ${screenCache} mc ${this.machine}`);
        if (this.machine) {
            let screenInfo = this.machine.getScreenIfUpdated(screenCache);
            if (screenInfo) {
                const screenSize = this.machine.getScreenSize();
                const msgBuf = new Uint8Array(10 + screenInfo.length);
                msgBuf[0] = 0x00;
                msgBuf[1] = 0x01;
                msgBuf[2] = (screenSize[0] >> 8) & 0xff;
                msgBuf[3] = screenSize[0] & 0xff;
                msgBuf[4] = (screenSize[1] >> 8) & 0xff;
                msgBuf[5] = screenSize[1] & 0xff;
                msgBuf[6] = 0x00;
                msgBuf[7] = 0x01;
                msgBuf[8] = 0x00;
                msgBuf[9] = 0x00;
                msgBuf.set(screenInfo, 10);
                websocket.send(msgBuf);
                // console.log(`screenInfo: ${msgBuf.length}`);
            }
        }
    }

    keyboard(isdown, asciiCode, usbKeyCode, modCode) {
        // Track key down/up state
        // console.log(`keyboard ${isdown} ${asciiCode} ${usbKeyCode} ${modCode}`);
        if (isdown !== '0') {
            this.keysDown[usbKeyCode] = modCode;
            // console.log(`keyboard down ${usbKeyCode}`);
        } else {
            delete this.keysDown[usbKeyCode];
            // console.log(`keyboard up ${usbKeyCode}`);
        }

        // Send to machine
        if (this.machine) {
            this.machine.keyboardKey(this.keysDown);
        }
    }

    setExecBps(execbps) {
        this.execbps = execbps;
    }
}

module.exports = Z80System;
