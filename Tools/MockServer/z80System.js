const Z80 = require('./z80');

class MemAccess {
    constructor() {
        this.addrSpace = new Uint8Array(0x10000);
        this.ioSpace = new Uint8Array(0x10000);
    }
    mem_read(addr) {
        if ((addr >= 0) && (addr < 0x10000)) {
            return this.addrSpace[addr];
        }
        return 0;
    }
    mem_write(addr, data) {
        if ((addr >= 0) && (addr < 0x10000)) {
            this.addrSpace[addr] = data;
        }
    }
    io_read(addr) {
        if ((addr >= 0) && (addr < 0x10000)) {
            return this.ioSpace[addr];
        }
        return 0;
    }
    io_write(addr, data) {
        if ((addr >= 0) && (addr < 0x10000)) {
            this.ioSpace[addr] = data;
        }
    }
    load(filename) {
        const fs = require('fs');
        const data = fs.readFileSync(filename);
        const mem = new Uint8Array(data.buffer);
        for (let i = 0; i < mem.length; i++) {
            this.mem_write(i, mem[i]);
        }
    }
};

class Z80System {
    constructor() {
        this.memAccess = new MemAccess();
        this.z80Proc = new Z80(this.memAccess);
    }

    exec(filename) {
        this.memAccess.load(filename);
        this.z80Proc.reset();
        let curState = this.z80Proc.getState();
        console.log(curState);
        this.z80Proc.run_instruction();
        curState = this.z80Proc.getState();
        console.log(curState);
    }
};

module.exports = Z80System;
