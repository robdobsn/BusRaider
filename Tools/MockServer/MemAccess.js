
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
    blockRead(start, len, isIo) {
        if (isIo) {
            return this.ioSpace.slice(start, start + len);
        } else {
            return this.addrSpace.slice(start, start + len);
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
    dumpMem(start, len) {
        const hexStr = this.addrSpace.slice(start, start + len)
            .map(x => x.toString(16).padStart(2, '0'))
            .join('');
        console.log(hexStr);
    }
};

module.exports = MemAccess;
