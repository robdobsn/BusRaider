const Z80 = require('./Z80');
const MemAccess = require('./MemAccess');
const Z80TRS80 = require('./TRS80Machine');
class Z80System {
    constructor() {
        this.memAccess = new MemAccess();
        this.z80Proc = new Z80(this.memAccess);
        this.machine = null;
        this.isRunning = false;
    }

    setMachine(systemName) {
        console.log(`setMachine ${systemName}`);
        if (systemName === "TRS80") {
            this.machine = new Z80TRS80(this.memAccess);
        }
    }

    exec(filename) {
        this.memAccess.load(filename);
        this.z80Proc.reset();
        let curState = this.z80Proc.getState();
        console.log(curState);
        this.isRunning = true; 
        // curState = this.z80Proc.getState();
        // console.log(curState);
        // this.memAccess.dumpMem(0x3c00, 0x400);
        this.processorTick = setInterval(() => {
            for (let i = 0; i < 100; i++) {
                this.step();
            }
        }, 1);
    }

    step() {
        if (this.isRunning) {
            this.z80Proc.run_instruction();
        }
    }

    updateMirrorScreen(websocket, screenCache) {
        // console.log(`updateMirrorScreen screenCache ${screenCache} mc ${this.machine}`);
        if (this.machine) {
            screenCache = this.machine.getScreenIfUpdated(screenCache);
            // console.log(`updateMirrorScreen screenCache ${screenCache}`);
            if (screenCache) {
                const screenSize = this.machine.getScreenSize();
                const msgBuf = new Uint8Array(10 + screenCache.length);
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
                msgBuf.set(screenCache, 10);
                websocket.send(msgBuf);
                console.log(`screenCache: ${msgBuf.length}`);
                return screenCache;
            }
        }
        return null;
    }
};

module.exports = Z80System;
