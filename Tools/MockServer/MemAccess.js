
class MemAccess {
    constructor() {
        this.memSpace = Buffer.alloc(0x10000);
        this.ioSpace = Buffer.alloc(0x10000);
        this.ioAddrModifierCB = null;
        this.memAddrModifierCB = null;
    }
    setup(memSize, memAddrModifierCB, ioSize, ioAddrModifierCB) {
        this.memSpace = Buffer.alloc(memSize);
        this.memAddrModifierCB = memAddrModifierCB;
        this.ioSpace = Buffer.alloc(ioSize);
        this.ioAddrModifierCB = ioAddrModifierCB;
    }
    mem_read(addr, noModifier) {
        if (!noModifier && this.memAddrModifierCB) {
            addr = this.memAddrModifierCB(addr);
        }
        if (addr > this.memSpace.length) {
            addr = addr % this.memSpace.length;
        }
        return this.memSpace[addr];
    }
    mem_write(addr, data, noModifier) {
        if (!noModifier && this.memAddrModifierCB) {
            addr = this.memAddrModifierCB(addr);
        }
        if (addr > this.memSpace.length) {
            addr = addr % this.memSpace.length;
        }
        this.memSpace[addr] = data;
    }
    io_read(addr, noModifier) {
        if (!noModifier && this.ioAddrModifierCB) {
            addr = this.ioAddrModifierCB(addr);
        }
        if (addr > this.ioSpace.length) {
            addr = addr % this.ioSpace.length;
        }
        return this.ioSpace[addr];
    }
    io_write(addr, data, noModifier) {
        if (!noModifier && this.ioAddrModifierCB) {
            addr = this.ioAddrModifierCB(addr);
        }
        if (addr > this.ioSpace.length) {
            addr = addr % this.ioSpace.length;
        }
        this.ioSpace[addr] = data;
    }
    blockRead(start, len, isIo) {
        if (isIo) {
            return this.ioSpace.slice(start, start + len);
        } else {
            return this.memSpace.slice(start, start + len);
        }
    }
    blockWrite(start, data, isIo) {
        if (isIo) {
            this.ioSpace.set(data, start);
        } else {
            this.memSpace.set(data, start);
        }
    }
    blockFill(start, len, val, isIO) {
        if (isIO) {
            this.ioSpace.fill(val, start, start+len);
        } else {
            this.memSpace.fill(val, start, start+len);
        }
    }

    loadBinaryFile(filename) {
        const fs = require('fs');
        const data = fs.readFileSync(filename);
        const mem = new Uint8Array(data.buffer);
        for (let i = 0; i < mem.length; i++) {
            this.mem_write(i, mem[i]);
        }
        console.log(`Loaded ${mem.length} bytes from ${filename}`);
    }
    memToHex(start, len, sep, isIo) {
        if (isIo) {
            return this.bufToHex(this.ioSpace.slice(start, start + len), sep);
        } else {
            return this.bufToHex(this.memSpace.slice(start, start + len), sep);
        }
    }
    bufToHex(buf, sep) {
        let hexStr = "";
        for (let i = 0; i < buf.length; i++) {
            hexStr += buf[i].toString(16).padStart(2, '0') + sep;
        }
        return hexStr;
    }
    dumpMem(start, len, isIo) {
        // const hexStr = this.memSpace.slice(start, start + len)
        //     .map(x => x.toString(16).padStart(2, '0'))
        //     .join('');
        // console.log(hexStr);
        console.log(this.memToHex(start, len, " ", isIo));
    }
    getDump(addr, len, isIo) {
        return this.memToHex(addr, len, "", isIo);
    }
};

module.exports = MemAccess;
