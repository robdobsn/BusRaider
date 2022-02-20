const Z80 = require('./Z80');
const MemAccess = require('./MemAccess');
const Z80TRS80 = require('./TRS80Machine');
const Z80Spectrum = require('./SpectrumMachine');
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
        this.tstatesSinceInt = 0;
        this.clockHz = 1000000;
        this.machines = {
            "TRS80": {
                "clockHz": 1770000,
                "create": (memAccess, z80Proc) => {
                    new Z80TRS80(memAccess, z80Proc);
                }
            },
            "ZX Spectrum": {
                "clockHz": 3500000,
                "create": (memAccess, z80Proc) => {
                    new Z80Spectrum(memAccess, z80Proc);
                }
            }
        }
    }

    setMachine(systemName) {
        if (systemName in this.machines) {
            this.machine = this.machines[systemName].create(this.memAccess, this.z80Proc);
            this.clockHz = this.machines[systemName].clockHz;
            console.log(`setMachine ${systemName} set Ok`);
            return;
        }
        console.log(`setMachine ${systemName} UNKNOWN SYSTEM NAME`);
        // if (systemName === "TRS80") {
        //     this.machine = new Z80TRS80(this.memAccess, this.z80Proc);
        // } else if (systemName === "ZX Spectrum") {
        //     this.machine = new Z80Spectrum(this.memAccess, this.z80Proc);
        // }
    }

    setClockHz(clockHz) {
        this.clockHz = clockHz;
    }

    getClockHz() {
        return this.clockHz;
    }

    loadFile(filename) {
        this.resetOnExec = true;
        const fileExt = filename.substr(filename.lastIndexOf('.') + 1).toLowerCase();
        // console.log(`load ${filename} ${fileExt}`);
        if ((fileExt === 'bin') || (fileExt === 'rom')) {
            this.memAccess.loadBinaryFile(filename);
        } else {
            if (this.machine) {
                this.machine.loadFile(filename);
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
            // this.memAccess.dumpMem(0x5b00, 0x100, false);
            // this.memAccess.dumpMem(0x7f00, 0x100, false);
        }
        // for (let i = 0; i < 10; i++) {
        //     console.log(`--------------------------------------------------------------------------------`);
        //     let curState = this.z80Proc.getState();
        //     console.log(curState);
        //     this.z80Proc.run_instruction();
        // }
        this.isRunning = true; 
        // curState = this.z80Proc.getState();
        // console.log(curState);
        // this.memAccess.dumpMem(0x3c00, 0x400, false);
        this.processorTick = setInterval(() => {
            for (let i = 0; i < this.clockHz/100000; i++) {
                if (!this.isRunning) {
                    break;
                }
                this.step();
            }
        }, 1);
    }

    step() {
        const tstates = this.z80Proc.run_instruction();
        const z80State = this.z80Proc.getState();
        if (z80State.pc in this.execbps) {
            this.isRunning = false;
        }
        this.tstatesSinceInt += tstates;
        if (this.machine.intOnTstates && (this.tstatesSinceInt >= this.machine.intOnTstates)) {
            this.z80Proc.interrupt(false, 0);
            // console.log(`interrupt at ${z80State.pc.toString(16)} ${this.tstatesSinceInt} ${tstates}`);
            this.tstatesSinceInt = 0;
        }
    }

    break() {
        this.isRunning = false;
    }

    continue() {
        this.isRunning = true;
    }

    getDump(addr, size, isIo) {
        return this.memAccess.getDump(addr, size, isIo);
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
            mem: this.memAccess.getDump(memDumpStart, this.dumpBytesTotal, false)
        };
    }

    updateMirrorScreen(websocket, screenCache) {
        // console.log(`updateMirrorScreen screenCache ${screenCache} mc ${this.machine}`);
        if (this.machine) {
            let screenInfo = this.machine.getScreenIfUpdated(screenCache);
            if (screenInfo) {
                websocket.send(this.machine.getScreenUpdateMsg(screenInfo));
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
