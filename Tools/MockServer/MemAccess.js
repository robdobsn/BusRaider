
class MemAccess {
    constructor() {
        this.addrSpace = Buffer.alloc(0x10000);
        this.ioSpace = Buffer.alloc(0x10000);
    }
    mem_read(addr) {
        if (addr > 65535) {
            addr = addr % 0x10000;
        }
        return this.addrSpace[addr];
    }
    mem_write(addr, data) {
        if (addr > 65535) {
            addr = addr % 0x10000;
        }
        this.addrSpace[addr] = data;
    }
    io_read(addr) {
        if (addr > 65535) {
            addr = addr % 0x10000;
        }
        return this.ioSpace[addr];
    }
    io_write(addr, data) {
        if (addr > 65535) {
            addr = addr % 0x10000;
        }
        this.ioSpace[addr] = data;
    }
    blockRead(start, len, isIo) {
        if (isIo) {
            return this.ioSpace.slice(start, start + len);
        } else {
            return this.addrSpace.slice(start, start + len);
        }
    }
    blockWrite(start, data, isIo) {
        if (isIo) {
            this.ioSpace.set(data, start);
        } else {
            this.addrSpace.set(data, start);
        }
    }

    load(filename) {
        const fs = require('fs');
        const data = fs.readFileSync(filename);
        const mem = new Uint8Array(data.buffer);
        for (let i = 0; i < mem.length; i++) {
            this.mem_write(i, mem[i]);
        }
        console.log(`Loaded ${mem.length} bytes from ${filename}`);
    }
    memToHex(start, len, sep) {
        let hexStr = "";
        for (let i = start; i < start+len; i++) {
            hexStr += this.addrSpace[i % 0x10000].toString(16).padStart(2, '0') + sep;
        }
        return hexStr;
    }
    dumpMem(start, len) {
        // const hexStr = this.addrSpace.slice(start, start + len)
        //     .map(x => x.toString(16).padStart(2, '0'))
        //     .join('');
        // console.log(hexStr);
        console.log(this.memToHex(start, len, " "));
    }
    getDump(addr, len) {
        return this.memToHex(addr, len, "");
    }
};

module.exports = MemAccess;
