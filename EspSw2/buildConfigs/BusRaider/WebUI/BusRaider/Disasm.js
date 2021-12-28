const _convertBuffer = new ArrayBuffer(2)
const _ui8 = new Uint8Array(_convertBuffer)
const _i8 = new Int8Array(_convertBuffer)
const _ui16 = new Uint16Array(_convertBuffer)

const unot8 = (val) => {
    _ui8[0] = ~val
    return _ui8[0]
}

const unsigned8 = (val) => {
    _ui8[0] = val
    return _ui8[0]
}

const signed8 = (val) => {
    _i8[0] = val
    return _i8[0]
}

const unsigned16 = (val) => {
    _ui16[0] = val
    return _ui16[0]
}

const usum8 = (val1, val2) => {
    _ui8[0] = val1
    _ui8[0] += val2
    return _ui8[0]
}

const hex = (val, len) => {
    val = val.toString(16).toUpperCase()
    while (val.length < len) {
        val = '0' + val
    }
    return val
}

const hex8 = (val) => hex(val, 2)
const hex16 = (val) => hex(val, 4)

class Port {
    constructor() {
        let buf = new ArrayBuffer(65536)
        this.data = new Uint8Array(buf)
    }

    read(addr) {
        if (addr > 65535) {
            addr = addr % 65536
        }
        return this.data[addr]
    }

    write(addr, val) {
        if (addr > 65535) {
            addr = addr % 65536
        }
        this.data[addr] = val;
    }
}

class Memory {
    constructor() {
        let buf = new ArrayBuffer(65536)
        this.data = new Uint8Array(buf)
    }

    read8(addr) {
        if (addr > 65535) {
            addr = addr % 65536
        }
        return this.data[addr]
    }
    write8(addr, val) {
        if (addr > 65535) {
            addr = addr % 65536
        }
        this.data[addr] = val
    }
    setFromHexStr(addr, hexStr) {
        let i = 0;
        while (i < hexStr.length) {
            this.write8(addr++, parseInt(hexStr.substr(i, 2), 16))
            i += 2
        }
    }
}

const RegisterMap = {
    0b111: 'a',
    0b000: 'b',
    0b001: 'c',
    0b010: 'd',
    0b011: 'e',
    0b100: 'h',
    0b101: 'l'
}

const Register16Map = {
    0b00: 'bc',
    0b01: 'de',
    0b10: 'hl',
    0b11: 'sp'
}

const Register16Map2 = {
    0b00: 'bc',
    0b01: 'de',
    0b10: 'hl',
    0b11: 'af'
}

const Prefixes = {
    ix: 0xdd,
    iy: 0xfd
}

// S Z 5 H 3 PV N C

const f_c = 1
const f_n = 2
const f_pv = 4
const f_3 = 8
const f_h = 16
const f_5 = 32
const f_z = 64
const f_s = 128

const c_nz = 0b000
const c_z = 0b001
const c_nc = 0b010
const c_c = 0b011
const c_po = 0b100
const c_pe = 0b101
const c_p = 0b110
const c_m = 0b111

const hasCarry_adc = true
const hasCarry_sbc = true
const hasCarry_add = false
const hasCarry_sub = false

const isSub_adc = false
const isSub_sbc = true
const isSub_add = false
const isSub_sub = true

const isDec_inc = false
const isDec_dec = true

const ie_ei = true
const ie_di = false

const isArithmetics_a = true
const isArithmetics_l = false

const bitState_set = true
const bitState_res = false

const ArgType = {
    Byte: 1,
    Word: 2,
    Offset: 3
}

const Conditions = {
    nz: c_nz,
    z: c_z,
    nc: c_nc,
    c: c_c,
    po: c_po,
    pe: c_pe,
    p: c_p,
    m: c_m
}

// Z80 constructor

const Z80 = function (memory, io, debug) {
    if (!(this instanceof Z80)) {
        throw 'Z80 is a constructor, use "new" keyword'
    }

    if (!memory) {
        throw 'Constructor should be called with a Memory instance as the first argument'
    }

    if (!memory.read8 || typeof memory.read8 !== 'function') {
        throw 'Memory instance should have read8 method'
    }
    if (!memory.write8 || typeof memory.write8 !== 'function') {
        throw 'Memory instance should have read8 method'
    }
    this.memory = memory
    this.io = io
    this.debugMode = debug

    this.read8 = addr => {
        this.tStates += 3
        let value = memory.read8(addr)
        if (typeof value === "undefined" || value == null) {
            this.debug(`Error reading memory address 0x${hex16(addr)}, value is ${value}`)
            return value
        }
        this.debug(`Reading memory address 0x${hex16(addr)} = 0x${hex8(value)}`)
        return value
    }

    this.write8 = (addr, value) => {
        this.tStates += 3
        this.debug(`Writing to memory address 0x${hex16(addr)} = 0x${hex8(value)}`)
        memory.write8(addr, value)
    }

    this.read16 = addr => {
        return this.read8(addr) | (this.read8(addr + 1) << 8)
    }
    this.write16 = (addr, value) => {
        let high = (value & 0xff00) >> 8
        let low = value & 0xff
        this.write8(addr, low)
        this.write8(addr + 1, high)
    }

    this.ioread = addr => {
        this.tStates += 4
        return this.io.read(addr)
    }
    this.iowrite = (addr, value) => {
        this.tStates += 4
        return this.io.write(addr, value)
    }

    // Creating registers
    this.r1 = {}
    this.r2 = {}

    let bf0 = new ArrayBuffer(16)
    let bf1 = new ArrayBuffer(14)
    let bf2 = new ArrayBuffer(14)
    this.b0 = new Uint8Array(bf0)
    this.w0 = new Uint16Array(bf0)
    this.b1 = new Uint8Array(bf1)
    this.w1 = new Uint16Array(bf1)
    this.b2 = new Uint8Array(bf2)
    this.w2 = new Uint16Array(bf2)
    this._defineByteRegister('f', 0)
    this._defineByteRegister('a', 1)
    this._defineByteRegister('c', 2)
    this._defineByteRegister('b', 3)
    this._defineByteRegister('e', 4)
    this._defineByteRegister('d', 5)
    this._defineByteRegister('l', 6)
    this._defineByteRegister('h', 7)
    this._defineByteRegister('ixl', 8)
    this._defineByteRegister('ixh', 9)
    this._defineByteRegister('iyl', 10)
    this._defineByteRegister('iyh', 11)
    this._defineWordRegister('af', 0)
    this._defineWordRegister('bc', 1)
    this._defineWordRegister('de', 2)
    this._defineWordRegister('hl', 3)
    this._defineWordRegister('ix', 4)
    this._defineWordRegister('iy', 5)

    Object.defineProperty(this, 'sp', {
        get: () => {
            return this.w0[2]
        },
        set: val => {
            this.w0[2] = val
        }
    })
    // Alias
    Object.defineProperty(this.r1, 'sp', {
        get: () => {
            return this.w0[2]
        },
        set: val => {
            this.w0[2] = val
        }
    })
    Object.defineProperty(this, 'pc', {
        get: () => {
            return this.w0[0]
        },
        set: val => {
            this.w0[0] = val
        }
    })
    Object.defineProperty(this, 'r', {
        get: () => {
            return this.b0[2]
        },
        set: val => {
            this.b0[2] = val
        }
    })
    Object.defineProperty(this, 'i', {
        get: () => {
            return this.b0[3]
        },
        set: val => {
            this.b0[3] = val
        }
    })

    this.reset()
}

Z80.prototype.reset = function () {
    this.pc = 0
    this.r1.f = 0
    this.im = 0
    this.iff1 = false
    this.iff2 = false
    this.r = 0
    this.i = 0
    this.halted = false
    this.tStates = 0
    this.nmiRequested = false
    this.intRequested = false
    this.deferInt = false
    this.execIntVector = false
}

Z80.prototype._defineByteRegister = function (name, position) {
    Object.defineProperty(this.r1, name, {
        get: () => {
            return this.b1[position]
        },
        set: value => {
            this.b1[position] = value
        }
    })
    Object.defineProperty(this.r2, name, {
        get: () => {
            return this.b2[position]
        },
        set: value => {
            this.b2[position] = value
        }
    })
}

Z80.prototype._defineWordRegister = function (name, position) {
    Object.defineProperty(this.r1, name, {
        get: () => {
            return this.w1[position]
        },
        set: value => {
            this.w1[position] = value
        }
    })
    Object.defineProperty(this.r2, name, {
        get: () => {
            return this.w2[position]
        },
        set: value => {
            this.w2[position] = value
        }
    })
}

Z80.prototype.dump = function () {
    console.log('=============================')
    console.log(`PC: ${hex16(this.pc)}     tStates: ${this.tStates}`)
    console.log(`SP: ${hex16(this.sp)}`)
    console.log('')
    console.log(`AF: ${hex16(this.r1.af)}         AF': ${hex16(this.r2.af)}`)
    console.log(`BC: ${hex16(this.r1.bc)}         BC': ${hex16(this.r2.bc)}`)
    console.log(`DE: ${hex16(this.r1.de)}         DE': ${hex16(this.r2.de)}`)
    console.log(`HL: ${hex16(this.r1.hl)}         HL': ${hex16(this.r2.hl)}`)
    console.log(`IX: ${hex16(this.r1.ix)}`)
    console.log(`IY: ${hex16(this.r1.iy)}`)
    console.log('')
    console.log(`I: ${hex8(this.i)}`)
    console.log(`R: ${hex8(this.r)}`)
    console.log('')
    let flags = []
    if (this.getFlag(f_c)) {
        flags.push('C')
    }
    if (this.getFlag(f_n)) {
        flags.push('N')
    }
    if (this.getFlag(f_pv)) {
        flags.push('PV')
    }
    if (this.getFlag(f_3)) {
        flags.push('3')
    }
    if (this.getFlag(f_h)) {
        flags.push('H')
    }
    if (this.getFlag(f_5)) {
        flags.push('5')
    }
    if (this.getFlag(f_z)) {
        flags.push('Z')
    }
    if (this.getFlag(f_s)) {
        flags.push('S')
    }
    console.log(`FLAGS: ${flags.join(' ')}`)
    console.log('=============================\n')
}

// Flag operations

Z80.prototype.setFlag = function (flag) {
    this.r1.f = this.r1.f | flag
}

Z80.prototype.resFlag = function (flag) {
    this.r1.f = this.r1.f & unot8(flag)
}

Z80.prototype.valFlag = function (flag, value) {
    value ? this.setFlag(flag) : this.resFlag(flag)
}

Z80.prototype.getFlag = function (flag) {
    return (this.r1.f & flag) !== 0
}

Z80.prototype.adjustFlags = function (value) {
    this.valFlag(f_5, (value & f_5) !== 0)
    this.valFlag(f_3, (value & f_3) !== 0)
}

Z80.prototype.adjustFlagSZP = function (value) {
    this.valFlag(f_s, (value & 0x80) !== 0)
    this.valFlag(f_z, value === 0)
    this.valFlag(f_pv, ParityBit[value])
}

Z80.prototype.adjustLogicFlag = function (flag_h) {
    this.valFlag(f_s, (this.r1.a & 0x80) !== 0)
    this.valFlag(f_z, this.r1.a === 0)
    this.valFlag(f_h, flag_h)
    this.valFlag(f_n, false)
    this.valFlag(f_c, false)
    this.valFlag(f_pv, ParityBit[this.r1.a])
    this.adjustFlags(this.r1.a)
}

Z80.prototype.condition = function (cond) {
    switch (cond) {
        case c_z:
            return this.getFlag(f_z)
        case c_nz:
            return !this.getFlag(f_z)
        case c_c:
            return this.getFlag(f_c)
        case c_nc:
            return !this.getFlag(f_c)
        case c_m:
            return this.getFlag(f_s)
        case c_p:
            return !this.getFlag(f_s)
        case c_pe:
            return this.getFlag(f_pv)
        case c_po:
            return !this.getFlag(f_pv)
        default:
            return true
    }
}

Z80.prototype.doPush = function (value) {
    this.sp -= 2
    this.write16(this.sp, value)
}

Z80.prototype.doPop = function (operandName) {
    let value = this.read16(this.sp)
    this.sp += 2
    return value
}

Z80.prototype.doIncDec = function (value, isDec) {
    this.debug(`doIncDec(${value}, ${isDec})`)
    if (isDec) {
        this.valFlag(f_pv, (value & 0x80) !== 0 && ((value - 1) & 0x80) === 0)
        this.debug(`f_pv set to ${this.getFlag(f_pv)}`)
        value = unsigned8(value - 1)
        this.valFlag(f_h, (value & 0x0f) === 0x0f)
        this.debug(`f_h set to ${this.getFlag(f_h)}`)
    } else {
        this.valFlag(f_pv, (value & 0x80) === 0 && ((value + 1) & 0x80) !== 0)
        this.debug(`f_pv set to ${this.getFlag(f_pv)}`)
        value = unsigned8(value + 1)
        this.valFlag(f_h, (value & 0x0f) === 0)
        this.debug(`f_h set to ${this.getFlag(f_h)}`)
    }
    this.debug(`value was set to ${value}`)
    this.valFlag(f_s, (value & 0x80) !== 0)
    this.valFlag(f_z, value === 0)
    this.valFlag(f_n, isDec)
    this.debug(`flags before adjusting: ${this.r1.f}`)
    this.adjustFlags(value)
    this.debug(`flags after adjusting: ${this.r1.f}`)
    return value
}

// R register
Z80.prototype.incr = function () {
    this.r = (this.r & 0x80) | ((this.r + 1) & 0x7f)
}

Z80.prototype.decr = function () {
    this.r = (this.r & 0x80) | ((this.r - 1) & 0x7f)
}

// Arithmetics
Z80.prototype.doArithmetics = function (value, withCarry, isSub) {
    this.debug(`doArithmetics val=${value}, carry=${withCarry}, isSub=${isSub}`)
    let res
    if (isSub) {
        this.setFlag(f_n)
        this.valFlag(f_h, (((this.r1.a & 0x0f) - (value & 0x0f)) & 0x10) !== 0)
        res = unsigned16(this.r1.a - value)
        if (withCarry && this.getFlag(f_c)) {
            res--
        }
    } else {
        this.resFlag(f_n)
        this.valFlag(f_h, (((this.r1.a & 0x0f) + (value & 0x0f)) & 0x10) !== 0)
        res = unsigned16(this.r1.a + value)
        if (withCarry && this.getFlag(f_c)) {
            res++
        }
    }
    this.valFlag(f_s, (res & 0x80) !== 0)
    this.valFlag(f_c, (res & 0x100) !== 0)
    this.valFlag(f_z, (res & 0xff) === 0)

    let minuedSign = this.r1.a & 0x80
    let subtrahendSign = value & 0x80
    let resultSign = res & 0x80
    let overflow
    if (isSub) {
        overflow = minuedSign !== subtrahendSign && resultSign !== minuedSign
    } else {
        overflow = minuedSign === subtrahendSign && resultSign !== minuedSign
    }
    this.valFlag(f_pv, overflow)
    this.adjustFlags(unsigned8(res & 0xff))
    return res & 0xff
}

Z80.prototype.doAddWord = function (a1, a2, withCarry, isSub) {
    if (withCarry && this.getFlag(f_c)) {
        a2++
    }

    let sum = a1
    if (isSub) {
        sum -= a2
        this.valFlag(f_h, (((a1 & 0x0fff) - (a2 & 0x0fff)) & 0x1000) !== 0)
    } else {
        sum += a2
        this.valFlag(f_h, (((a1 & 0x0fff) + (a2 & 0x0fff)) & 0x1000) !== 0)
    }
    this.valFlag(f_c, (sum & 0x10000) !== 0)
    if (withCarry || isSub) {
        let minuedSign = a1 & 0x8000
        let subtrahendSign = a2 & 0x8000
        let resultSign = sum & 0x8000
        let overflow
        if (isSub) {
            overflow = minuedSign != subtrahendSign && resultSign != minuedSign
        } else {
            overflow = minuedSign == subtrahendSign && resultSign != minuedSign
        }
        this.valFlag(f_pv, overflow)
        this.valFlag(f_s, resultSign !== 0)
        this.valFlag(f_z, sum === 0)
    }
    this.valFlag(f_n, isSub)
    this.adjustFlags(unsigned8(sum >> 8))
    return unsigned16(sum)
}

Z80.prototype.do_and = function (value) {
    this.r1.a &= value
    this.adjustLogicFlag(true)
}

Z80.prototype.do_or = function (value) {
    this.r1.a |= value
    this.adjustLogicFlag(false)
}

Z80.prototype.do_xor = function (value) {
    this.r1.a ^= value
    this.adjustLogicFlag(false)
}

Z80.prototype.doBit = function (b, val) {
    if ((val & (1 << b)) !== 0) {
        this.resFlag(f_z | f_pv)
    } else {
        this.setFlag(f_z | f_pv)
    }
    this.setFlag(f_h)
    this.resFlag(f_n)
    this.resFlag(f_s)
    if (b === 7 && !this.getFlag(f_z)) {
        this.setFlag(f_s)
    }
}

Z80.prototype.doBitR = function (b, val) {
    this.doBit(b, val)
    this.valFlag(f_5, (val & f_5) !== 0)
    this.valFlag(f_3, (val & f_3) !== 0)
}

Z80.prototype.doBitIndexed = function (b, addr) {
    let val = this.read8(addr)
    this.doBit(b, val)
    let high = addr >> 8
    this.valFlag(f_5, (high & f_5) !== 0)
    this.valFlag(f_3, (high & f_3) !== 0)
}

Z80.prototype.doSetRes = function (state, pos, val) {
    if (state) {
        return val | (1 << pos)
    } else {
        return val & ~(1 << pos)
    }
}

Z80.prototype.doDAA = function () {
    let factor = 0
    let carry = false

    if (this.r1.a > 0x99 || this.getFlag(f_c)) {
        factor |= 0x60
        carry = true
    }
    if ((this.r1.a & 0x0f) > 9 || this.getFlag(f_h)) {
        factor |= 0x06
    }

    let a = this.r1.a
    if (this.getFlag(f_n)) {
        this.r1.a -= factor
    } else {
        this.r1.a += factor
    }

    this.valFlag(f_h, ((a ^ this.r1.a) & 0x10) !== 0)
    this.valFlag(f_c, carry)
    this.valFlag(f_s, (this.r1.a & 0x80) !== 0)
    this.valFlag(f_z, this.r1.a === 0)
    this.valFlag(f_pv, ParityBit[this.r1.a])
    this.adjustFlags(this.r1.a)
}

Z80.prototype.doCPHL = function () {
    let val = this.read8(this.r1.hl)
    let result = this.doArithmetics(val, false, true)
    this.adjustFlags(val)
    return result
}

Z80.prototype.do_rlc = function (value, adjust) {
    this.valFlag(f_c, (value & 0x80) !== 0)
    value = unsigned8(value << 1)
    let cy = this.getFlag(f_c) ? 1 : 0
    value |= cy
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    if (adjust) {
        this.adjustFlagSZP(value)
    }
    return value
}

Z80.prototype.do_rrc = function (value, adjust) {
    this.valFlag(f_c, (value & 0x01) !== 0)
    value = unsigned8(value >> 1)
    let cy = this.getFlag(f_c) ? 0x80 : 0
    value |= cy
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    if (adjust) {
        this.adjustFlagSZP(value)
    }
    return value
}

Z80.prototype.do_rl = function (value, adjust) {
    let cy = this.getFlag(f_c) ? 1 : 0
    this.valFlag(f_c, (value & 0x80) !== 0)
    value = unsigned8(value << 1)
    value |= cy
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    if (adjust) {
        this.adjustFlagSZP(value)
    }
    return value
}

Z80.prototype.do_rr = function (value, adjust) {
    let cy = this.getFlag(f_c) ? 0x80 : 0
    this.valFlag(f_c, (value & 0x01) !== 0)
    value = unsigned8(value >> 1)
    value |= cy
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    if (adjust) {
        this.adjustFlagSZP(value)
    }
    return value
}

Z80.prototype.do_sl = function (value, isArithmetics) {
    this.valFlag(f_c, (value & 0x80) !== 0)
    value = unsigned8(value << 1)
    if (!isArithmetics) {
        value |= 1
    }
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    this.adjustFlagSZP(value)
    return value
}

Z80.prototype.do_sr = function (value, isArithmetics) {
    let bit = value & 0x80
    this.valFlag(f_c, (value & 0x01) !== 0)
    value = unsigned8(value >> 1)
    if (isArithmetics) {
        value |= bit
    }
    this.adjustFlags(value)
    this.resFlag(f_h | f_n)
    this.adjustFlagSZP(value)
    return value
}

// Creating opcode tables
Z80.prototype.opcodeTable = new Array(256)
Z80.prototype.opcodeTableCB = new Array(256)
Z80.prototype.opcodeTableED = new Array(256)
Z80.prototype.opcodeTableFD = new Array(256)
Z80.prototype.opcodeTableDD = new Array(256)
Z80.prototype.opcodeTableDDCB = new Array(256)
Z80.prototype.opcodeTableFDCB = new Array(256)
Z80.prototype.opcodeTableDDCB.opcodeOffset = 1
Z80.prototype.opcodeTableFDCB.opcodeOffset = 1

// Creating cross-table links
Z80.prototype.opcodeTable[0xcb] = { nextTable: Z80.prototype.opcodeTableCB }
Z80.prototype.opcodeTable[0xed] = { nextTable: Z80.prototype.opcodeTableED }
Z80.prototype.opcodeTable[0xdd] = { nextTable: Z80.prototype.opcodeTableDD }
Z80.prototype.opcodeTable[0xfd] = { nextTable: Z80.prototype.opcodeTableFD }
Z80.prototype.opcodeTableDD[0xcb] = { nextTable: Z80.prototype.opcodeTableDDCB }
Z80.prototype.opcodeTableFD[0xcb] = { nextTable: Z80.prototype.opcodeTableFDCB }

Z80.prototype.opcodeTableDD[0xdd] = { nextTable: Z80.prototype.opcodeTableDD }
Z80.prototype.opcodeTableDD[0xfd] = { nextTable: Z80.prototype.opcodeTableFD }
Z80.prototype.opcodeTableFD[0xdd] = { nextTable: Z80.prototype.opcodeTableDD }
Z80.prototype.opcodeTableFD[0xfd] = { nextTable: Z80.prototype.opcodeTableFD }

Z80.prototype.disassemble = function (addr) {
    let codes = []

    if (typeof addr === 'undefined') {
        addr = this.pc
    }
    // disassemple uses memory own methods which don't affect cpu tStates
    let opCode = this.memory.read8(addr++)
    codes.push(opCode)

    let instr = this.opcodeTable[opCode]
    if (!instr) {
        return {
            dasm: 'Error disassembling ' + codes.map(c => hex8(c)).join(' '),
            nextAddr: addr
        }
    }

    while ('nextTable' in instr) {
        let nextTable = instr.nextTable
        opCode = this.memory.read8(addr++)
        codes.push(opCode)

        instr = nextTable[opCode]
        if (!instr) {
            return {
                dasm: 'Error disassembling ' + codes.map(c => hex8(c)).join(' '),
                nextAddr: addr
            }
        }
    }

    let { dasm, args } = instr
    let argNum = 0
    let arg
    args.forEach(argType => {
        switch (argType) {
            case ArgType.Byte:
                arg = this.memory.read8(addr++)
                dasm = dasm.replace(`{${argNum++}}`, `$${hex8(arg)}`)
                break
            case ArgType.Offset:
                arg = this.memory.read8(addr++)
                arg = signed8(arg)
                if (arg >= 0) {
                    arg = '+' + arg
                }
                dasm = dasm.replace(`{${argNum++}}`, `${arg}`)
                break
            case ArgType.Word:
                arg = this.memory.read8(addr) | (this.memory.read8(addr + 1) << 8)
                addr += 2
                dasm = dasm.replace(`{${argNum++}}`, `$${hex16(arg)}`)
                break
        }
    })
    return { dasm: dasm, nextAddr: addr }
}

Z80.prototype.debug = function () {
    if (this.debugMode) {
        let data = ['CPU debug:'].concat(Array.prototype.map.call(arguments, i => i))
        console.log.apply(console, data)
    }
}

Z80.prototype.execute = function () {
    let current = this.opcodeTable
    let opCode
    let offset = 0
    let operation
    let codes = []

    this.debug('execute() started, t =', this.tStates)
    while (true) {
        opCode = this.read8(this.pc + offset)
        codes.push(opCode)
        this.debug('memory read, t =', this.tStates)
        this.pc++
        this.tStates++
        this.debug('pc incremented, t =', this.tStates)
        this.incr()

        operation = current[opCode]
        if (!operation) {
            throw 'Instruction ' + codes.map(i => hex8(i)).join(' ') + ' not declared'
        }
        if (operation.funcName) {
            if (!(operation.funcName in this)) {
                throw 'Instruction ' + codes.map(i => hex8(i)).join(' ') + ' not implemented'
            }
            this.pc -= offset
            this.debug('Running ' + operation.funcName + '(), t =', this.tStates)
            this[operation.funcName].call(this)
            this.debug(operation.funcName + '() executed, t =', this.tStates)
            break
        } else {
            if (operation.nextTable) {
                current = operation.nextTable
                offset = current.opcodeOffset || 0
                if (offset > 0) {
                    this.decr()
                }
            }
        }
    }
}

// Here comes all the CPU operations available
//
// Tables are filled dynamically in class compile time
//
// Implementations are made with a simple template engine and should
// be precompiled with a compile.js script

// LD r, r'
for (let dst in RegisterMap) {
    for (let src in RegisterMap) {
        let opCode = 0b01000000 | (dst << 3) | src
        let dstRegName = RegisterMap[dst]
        let srcRegName = RegisterMap[src]
        let opFuncName = `ld_${dstRegName}_${srcRegName}`
        let disasmString = `ld ${dstRegName}, ${srcRegName}`
        Z80.prototype.opcodeTable[opCode] = {
            funcName: opFuncName,
            dasm: disasmString,
            args: []
        }
        for (let pref in Prefixes) {
            let nsrcRegName = srcRegName === 'l' || srcRegName === 'h' ? pref + srcRegName : srcRegName
            let ndstRegName = dstRegName === 'l' || dstRegName === 'h' ? pref + dstRegName : dstRegName
            opFuncName = `ld_${ndstRegName}_${nsrcRegName}`
            disasmString = `ld ${ndstRegName}, ${nsrcRegName}`
            let table = Z80.prototype.opcodeTable[Prefixes[pref]].nextTable
            table[opCode] = {
                funcName: opFuncName,
                dasm: disasmString,
                args: []
            }
        }
    }
}

// #BEGIN ld_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.r1.$1 = this.r1.$2
}

// #END

// LD r, n
for (let dst in RegisterMap) {
    let opCode = 0b00000110 | (dst << 3)
    let opFuncName = `ld_${RegisterMap[dst]}_n`
    let disasmString = `ld ${RegisterMap[dst]}, \{0\}`
    Z80.prototype[opFuncName] = null
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Byte]
    }
}
Z80.prototype.opcodeTableDD[0x26] = { funcName: 'ld_ixh_n', dasm: 'ld ixh, {0}', args: [ArgType.Byte] }
Z80.prototype.opcodeTableDD[0x2e] = { funcName: 'ld_ixl_n', dasm: 'ld ixl, {0}', args: [ArgType.Byte] }
Z80.prototype.opcodeTableFD[0x26] = { funcName: 'ld_iyh_n', dasm: 'ld iyh, {0}', args: [ArgType.Byte] }
Z80.prototype.opcodeTableFD[0x2e] = { funcName: 'ld_iyl_n', dasm: 'ld iyl, {0}', args: [ArgType.Byte] }

// #BEGIN ld_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]_n
Z80.prototype.$0 = function () {
    this.r1.$1 = this.read8(this.pc++)
}

// #END

// LD r, (HL)
for (let dst in RegisterMap) {
    let opCode = 0b01000110 | (dst << 3)
    let opFuncName = `ld_${RegisterMap[dst]}__hl_`
    let disasmString = `ld ${RegisterMap[dst]}, (hl)`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN ld_[a,b,c,d,e,h,l]__hl_
Z80.prototype.$0 = function () {
    this.r1.$1 = this.read8(this.r1.hl)
}

// #END

// LD (HL), r
for (let src in RegisterMap) {
    let opCode = 0b01110000 | src
    let opFuncName = `ld__hl__${RegisterMap[src]}`
    let disasmString = `ld (hl), ${RegisterMap[src]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN ld__hl__[a,b,c,d,e,h,l]
Z80.prototype.$0 = function () {
    this.write8(this.r1.hl, this.r1.$1)
}

// #END

// LD (HL), n
Z80.prototype.opcodeTable[0b00110110] = {
    funcName: 'ld__hl__n',
    dasm: 'ld (hl), {0}',
    args: [ArgType.Byte]
}

Z80.prototype.ld__hl__n = function () {
    this.write8(this.r1.hl, this.read8(this.pc++))
}

// LD A, (BC)
Z80.prototype.opcodeTable[0b00001010] = {
    funcName: 'ld_a__bc_',
    tStates: 7,
    dasm: 'ld a, (bc)',
    args: []
}
// LD A, (DE)
Z80.prototype.opcodeTable[0b00011010] = {
    funcName: 'ld_a__de_',
    tStates: 7,
    dasm: 'ld a, (de)',
    args: []
}

// #BEGIN ld_a__[bc,de]_
Z80.prototype.$0 = function () {
    this.r1.a = this.read8(this.r1.$1)
}

// #END

// LD (BC), A
Z80.prototype.opcodeTable[0b00000010] = {
    funcName: 'ld__bc__a',
    tStates: 7,
    dasm: 'ld (bc), a',
    args: []
}
// LD (DE), A
Z80.prototype.opcodeTable[0b00010010] = {
    funcName: 'ld__de__a',
    tStates: 7,
    dasm: 'ld (de), a',
    args: []
}

// #BEGIN ld__[bc,de]__a
Z80.prototype.$0 = function () {
    this.write8(this.r1.$1, this.r1.a)
}

// #END

// LD A, (nn)
Z80.prototype.opcodeTable[0b00111010] = {
    funcName: 'ld_a__nn_',
    dasm: 'ld a, ({0})',
    args: [ArgType.Word]
}

Z80.prototype.ld_a__nn_ = function () {
    this.r1.a = this.read8(this.read16(this.pc))
    this.pc += 2
}

// LD (nn), A
Z80.prototype.opcodeTable[0b00110010] = {
    funcName: 'ld__nn__a',
    dasm: 'ld ({0}), a',
    args: [ArgType.Word]
}

Z80.prototype.ld__nn__a = function () {
    this.write8(this.read16(this.pc), this.r1.a)
    this.pc += 2
}

// LD A, I
Z80.prototype.opcodeTableED[0b01010111] = {
    funcName: 'ld_a_i',
    tStates: 9,
    dasm: 'ld a, i',
    args: []
}
// LD A, R
Z80.prototype.opcodeTableED[0b01011111] = {
    funcName: 'ld_a_r',
    tStates: 9,
    dasm: 'ld a, r',
    args: []
}

// #BEGIN ld_a_[i,r]
Z80.prototype.$0 = function () {
    this.tStates++
    this.r1.a = this.$1
    this.adjustFlags(this.r1.a)
    this.resFlag(f_h | f_n)
    this.valFlag(f_pv, this.iff2)
    this.valFlag(f_s, (this.r1.a & 0x80) !== 0)
    this.valFlag(f_z, this.r1.a === 0)
}

// #END

// LD I, A
Z80.prototype.opcodeTableED[0b01000111] = {
    funcName: 'ld_i_a',
    tStates: 9,
    dasm: 'ld i, a',
    args: []
}
// LD R, A
Z80.prototype.opcodeTableED[0b01001111] = {
    funcName: 'ld_r_a',
    tStates: 9,
    dasm: 'ld r, a',
    args: []
}

// #BEGIN ld_[i,r]_a
Z80.prototype.$0 = function () {
    this.tStates++
    this.$1 = this.r1.a
}

// #END

// LD r, (IX/IY+d)
for (let dst in RegisterMap) {
    let opCode = 0b01000110 | (dst << 3)
    for (let iReg in Prefixes) {
        let opFuncName = `ld_${RegisterMap[dst]}__${iReg}_d_`
        let disasmString = `ld ${RegisterMap[dst]}, (${iReg}+{0})`
        Z80.prototype.opcodeTable[Prefixes[iReg]].nextTable[opCode] = {
            funcName: opFuncName,
            dasm: disasmString,
            args: [ArgType.Byte]
        }
    }
}

// #BEGIN ld_[a,b,c,d,e,h,l]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 5
    let offset = signed8(this.read8(this.pc++))
    this.r1.$1 = this.read8(this.r1.$2 + offset)
}

// #END

// LD (IX/IY+d), r
for (let src in RegisterMap) {
    let opCode = 0b01110000 | src
    for (let iReg in Prefixes) {
        let opFuncName = `ld__${iReg}_d__${RegisterMap[src]}`
        let disasmString = `ld (${iReg}+{0}), ${RegisterMap[src]}`
        Z80.prototype.opcodeTable[Prefixes[iReg]].nextTable[opCode] = {
            funcName: opFuncName,
            dasm: disasmString,
            args: [ArgType.Byte]
        }
    }
}

// #BEGIN ld__[ix,iy]_d__[a,b,c,d,e,h,l]
Z80.prototype.$0 = function () {
    this.tStates += 5
    let offset = signed8(this.read8(this.pc++))
    this.write8(this.$2 + offset, this.r1.$1)
}

// #END

// LD (IX/IY+d), n
for (let iReg in Prefixes) {
    let opFuncName = `ld__${iReg}_d__n`
    let disasmString = `ld (${iReg}+{0}), {1}`
    Z80.prototype.opcodeTable[Prefixes[iReg]].nextTable[0b00110110] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN ld__[ix,iy]_d__n
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let n = this.read8(this.pc++)
    this.write8(this.$1 + offset, n)
}

// #END

// LD dd, nn
for (let rCode in Register16Map) {
    let opCode = 0b00000001 | (rCode << 4)
    let opFuncName = `ld_${Register16Map[rCode]}_nn`
    let disasmString = `ld ${Register16Map[rCode]}, {0}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

for (let regName in Prefixes) {
    let code = Prefixes[regName]
    Z80.prototype.opcodeTable[code].nextTable[0b00100001] = {
        funcName: `ld_${regName}_nn`,
        dasm: `ld ${regName}, {0}`,
        args: [ArgType.Word]
    }
}

// #BEGIN ld_[bc,de,hl,sp,ix,iy]_nn
Z80.prototype.$0 = function () {
    this.r1.$1 = this.read16(this.pc)
    this.pc += 2
}

// #END

// LD HL, (nn)
Z80.prototype.opcodeTable[0x2a] = {
    funcName: 'ld_hl__nn_',
    dasm: `ld hl, ({0})`,
    args: [ArgType.Word]
}

// LD IX/IY, (nn)
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x2a] = {
        funcName: `ld_${pref}__nn_`,
        dasm: `ld ${pref}, ({0})`,
        args: [ArgType.Word]
    }
}

// LD dd, (nn)
for (let rCode in Register16Map) {
    let opCode = 0b01001011 | (rCode << 4)
    let opFuncName = `ld_${Register16Map[rCode]}__nn_`
    let disasmString = `ld ${Register16Map[rCode]}, ({0})`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN ld_[ix,iy,bc,de,hl,sp]__nn_
Z80.prototype.$0 = function () {
    this.r1.$1 = this.read16(this.read16(this.pc))
    this.pc += 2
}

// #END

// LD (nn), HL
Z80.prototype.opcodeTable[0x22] = {
    funcName: 'ld__nn__hl',
    dasm: `ld ({0}), hl`,
    args: [ArgType.Word]
}

// LD (nn), IX/IY
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x22] = {
        funcName: `ld__nn__${pref}`,
        dasm: `ld ({0}), ${pref}`,
        args: [ArgType.Word]
    }
}

// LD (nn), dd
for (let rCode in Register16Map) {
    let opCode = 0b01000011 | (rCode << 4)
    let opFuncName = `ld__nn__${Register16Map[rCode]}`
    let disasmString = `ld ({0}), ${Register16Map[rCode]}`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN ld__nn__[ix,iy,bc,de,hl,sp]
Z80.prototype.$0 = function () {
    let addr = this.read16(this.pc)
    this.pc += 2
    this.write16(addr, this.r1.$1)
}

// #END

// LD SP, HL/IX/IY
Z80.prototype.opcodeTable[0xf9] = {
    funcName: 'ld_sp_hl',
    dasm: 'ld sp, hl',
    args: []
}
Z80.prototype.ld_sp_hl = function () {
    this.tStates += 2
    this.sp = this.r1.hl
}

Z80.prototype.opcodeTableDD[0xf9] = {
    funcName: 'ld_sp_ix',
    dasm: 'ld sp, ix',
    args: []
}
Z80.prototype.ld_sp_ix = function () {
    this.tStates += 2
    this.sp = this.r1.ix
}

Z80.prototype.opcodeTableFD[0xf9] = {
    funcName: 'ld_sp_iy',
    dasm: 'ld sp, iy',
    args: []
}
Z80.prototype.ld_sp_iy = function () {
    this.tStates += 2
    this.sp = this.r1.iy
}

// PUSH qq
for (let rCode in Register16Map2) {
    let opCode = 0b11000101 | (rCode << 4)
    let opFuncName = `push_${Register16Map2[rCode]}`
    let disasmString = `push ${Register16Map2[rCode]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

for (let pref in Prefixes) {
    let opFuncName = `push_${pref}`
    let disasmString = `push ${pref}`
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xe5] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN push_[af,bc,de,hl,ix,iy]
Z80.prototype.$0 = function () {
    this.tStates++
    this.doPush(this.r1.$1)
}

// #END

// POP qq
for (let rCode in Register16Map2) {
    let opCode = 0b11000001 | (rCode << 4)
    let opFuncName = `pop_${Register16Map2[rCode]}`
    let disasmString = `pop ${Register16Map2[rCode]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

for (let pref in Prefixes) {
    let opFuncName = `pop_${pref}`
    let disasmString = `pop ${pref}`
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xe1] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN pop_[af,bc,de,hl,ix,iy]
Z80.prototype.$0 = function () {
    this.r1.$1 = this.doPop()
}

// #END

// NOP
Z80.prototype.opcodeTable[0x00] = {
    funcName: 'nop',
    dasm: 'nop',
    args: []
}

Z80.prototype.nop = function () {
    // nop
}

// EX DE, HL
Z80.prototype.opcodeTable[0xeb] = {
    funcName: 'ex_de_hl',
    dasm: 'ex de, hl',
    args: [],
    tStates: 4
}
Z80.prototype.ex_de_hl = function () {
    let _t = this.r1.de
    this.r1.de = this.r1.hl
    this.r1.hl = _t
}

// EX AF, AF'
Z80.prototype.opcodeTable[0x08] = {
    funcName: 'ex_af_af_',
    dasm: "ex af, af'",
    args: [],
    tStates: 4
}
Z80.prototype.ex_af_af_ = function () {
    let _t = this.r1.af
    this.r1.af = this.r2.af
    this.r2.af = _t
}

// EXX
Z80.prototype.opcodeTable[0xd9] = {
    funcName: 'exx',
    dasm: 'exx',
    args: [],
    tStates: 4
}
Z80.prototype.exx = function () {
    let _t = this.r1.bc
    this.r1.bc = this.r2.bc
    this.r2.bc = _t
    _t = this.r1.de
    this.r1.de = this.r2.de
    this.r2.de = _t
    _t = this.r1.hl
    this.r1.hl = this.r2.hl
    this.r2.hl = _t
}

// EX (SP), HL/IX/IY
Z80.prototype.opcodeTable[0xe3] = {
    funcName: 'ex__sp__hl',
    dasm: 'ex (sp), hl',
    args: [],
    tStates: 19
}
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xe3] = {
        funcName: `ex__sp__${pref}`,
        dasm: `ex (sp), ${pref}`,
        args: [],
        tStates: 19
    }
}

// #BEGIN ex__sp__[hl,ix,iy]
Z80.prototype.$0 = function () {
    this.tStates += 3
    let _t = this.r1.$1
    this.r1.$1 = this.read16(this.sp)
    this.write16(this.sp, _t)
}

// #END

// LDI
Z80.prototype.opcodeTableED[0xa0] = {
    funcName: 'ldi',
    dasm: 'ldi',
    args: [],
    tStates: 16
}
Z80.prototype.ldi = function () {
    this.tStates += 2
    let val = this.read8(this.r1.hl++)
    this.write8(this.r1.de++, val)
    this.r1.bc--
    let sum = usum8(this.r1.a, val)
    this.valFlag(f_5, (sum & 0x02) !== 0)
    this.valFlag(f_3, (sum & f_3) !== 0)
    this.resFlag(f_h | f_n)
    this.valFlag(f_pv, this.r1.bc !== 0)
}

// LDIR
Z80.prototype.opcodeTableED[0xb0] = {
    funcName: 'ldir',
    dasm: 'ldir',
    args: []
}
Z80.prototype.ldir = function () {
    this.ldi()
    if (this.r1.bc !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// LDD
Z80.prototype.opcodeTableED[0xa8] = { funcName: 'ldd', dasm: 'ldd', args: [] }
Z80.prototype.ldd = function () {
    this.tStates += 2
    let val = this.read8(this.r1.hl--)
    this.write8(this.r1.de--, val)
    this.r1.bc--
    let sum = usum8(this.r1.a, val)
    this.valFlag(f_5, (sum & 0x02) !== 0)
    this.valFlag(f_3, (sum & f_3) !== 0)
    this.resFlag(f_h | f_n)
    this.valFlag(f_pv, this.r1.bc !== 0)
}

// LDDR
Z80.prototype.opcodeTableED[0xb8] = {
    funcName: 'lddr',
    dasm: 'lddr',
    args: []
}
Z80.prototype.lddr = function () {
    this.ldd()
    if (this.r1.bc !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// CPI
Z80.prototype.opcodeTableED[0xa1] = { funcName: 'cpi', dasm: 'cpi', args: [] }
Z80.prototype.cpi = function () {
    this.tStates += 5
    let carry = this.getFlag(f_c)
    let value = this.doCPHL()
    if (this.getFlag(f_h)) {
        value--
    }
    this.r1.hl++
    this.r1.bc--
    this.valFlag(f_pv, this.r1.bc !== 0)
    this.valFlag(f_c, carry)
    this.valFlag(f_5, (value & (1 << 2)) !== 0)
    this.valFlag(f_3, (value & (1 << 3)) !== 0)
}

// CPIR
Z80.prototype.opcodeTableED[0xb1] = {
    funcName: 'cpir',
    dasm: 'cpir',
    args: []
}
Z80.prototype.cpir = function () {
    this.cpi()
    if (this.r1.bc !== 0 && !this.getFlag(f_z)) {
        this.tStates += 5
        this.pc -= 2
    }
}

// CPD
Z80.prototype.opcodeTableED[0xa9] = { funcName: 'cpd', dasm: 'cpd', args: [] }
Z80.prototype.cpd = function () {
    this.tStates += 5
    let carry = this.getFlag(f_c)
    let value = this.doCPHL()
    if (this.getFlag(f_h)) {
        value--
    }
    this.r1.hl--
    this.r1.bc--
    this.valFlag(f_pv, this.r1.bc !== 0)
    this.valFlag(f_c, carry)
    this.valFlag(f_5, (value & (1 << 1)) !== 0)
    this.valFlag(f_3, (value & (1 << 3)) !== 0)
}

// CPDR
Z80.prototype.opcodeTableED[0xb9] = {
    funcName: 'cpdr',
    dasm: 'cpdr',
    args: []
}
Z80.prototype.cpdr = function () {
    this.cpd()
    if (this.r1.bc !== 0 && !this.getFlag(f_z)) {
        this.tStates += 5
        this.pc -= 2
    }
}

// ADD A, r
for (let rCode in RegisterMap) {
    let opCode = 0b10000000 | rCode
    let rName = RegisterMap[rCode]
    let opFuncName = `add_a_${rName}`
    let disasmString = `add a, ${rName}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableDD[0x84] = { funcName: 'add_a_ixh', dasm: 'add a, ixh', args: [] }
Z80.prototype.opcodeTableDD[0x85] = { funcName: 'add_a_ixl', dasm: 'add a, ixl', args: [] }
Z80.prototype.opcodeTableFD[0x84] = { funcName: 'add_a_iyh', dasm: 'add a, iyh', args: [] }
Z80.prototype.opcodeTableFD[0x85] = { funcName: 'add_a_iyl', dasm: 'add a, iyl', args: [] }

// ADC A, r
for (let rCode in RegisterMap) {
    let opCode = 0b10001000 | rCode
    let rName = RegisterMap[rCode]
    let opFuncName = `adc_a_${rName}`
    let disasmString = `adc a, ${rName}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableDD[0x8c] = { funcName: 'adc_a_ixh', dasm: 'adc a, ixh', args: [] }
Z80.prototype.opcodeTableDD[0x8d] = { funcName: 'adc_a_ixl', dasm: 'adc a, ixl', args: [] }
Z80.prototype.opcodeTableFD[0x8c] = { funcName: 'adc_a_iyh', dasm: 'adc a, iyh', args: [] }
Z80.prototype.opcodeTableFD[0x8d] = { funcName: 'adc_a_iyl', dasm: 'adc a, iyl', args: [] }

// SUB A, r
for (let rCode in RegisterMap) {
    let opCode = 0b10010000 | rCode
    let rName = RegisterMap[rCode]
    let opFuncName = `sub_a_${rName}`
    let disasmString = `sub ${rName}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableDD[0x94] = { funcName: 'sub_a_ixh', dasm: 'sub ixh', args: [] }
Z80.prototype.opcodeTableDD[0x95] = { funcName: 'sub_a_ixl', dasm: 'sub ixl', args: [] }
Z80.prototype.opcodeTableFD[0x94] = { funcName: 'sub_a_iyh', dasm: 'sub iyh', args: [] }
Z80.prototype.opcodeTableFD[0x95] = { funcName: 'sub_a_iyl', dasm: 'sub iyl', args: [] }

// SBC A, r
for (let rCode in RegisterMap) {
    let opCode = 0b10011000 | rCode
    let rName = RegisterMap[rCode]
    let opFuncName = `sbc_a_${rName}`
    let disasmString = `sbc a, ${rName}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableDD[0x9c] = { funcName: 'sbc_a_ixh', dasm: 'sbc a, ixh', args: [] }
Z80.prototype.opcodeTableDD[0x9d] = { funcName: 'sbc_a_ixl', dasm: 'sbc a, ixl', args: [] }
Z80.prototype.opcodeTableFD[0x9c] = { funcName: 'sbc_a_iyh', dasm: 'sbc a, iyh', args: [] }
Z80.prototype.opcodeTableFD[0x9d] = { funcName: 'sbc_a_iyl', dasm: 'sbc a, iyl', args: [] }

// #BEGIN [adc,sbc,add,sub]_a_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.r1.a = this.doArithmetics(this.r1.$2, hasCarry_$1, isSub_$1)
}

// #END

// ADD/ADC/SUB/SBC a, n
Z80.prototype.opcodeTable[0xc6] = {
    funcName: 'add_a_n',
    dasm: 'add a, {0}',
    args: [ArgType.Byte]
}
Z80.prototype.opcodeTable[0xce] = {
    funcName: 'adc_a_n',
    dasm: 'adc a, {0}',
    args: [ArgType.Byte]
}
Z80.prototype.opcodeTable[0xde] = {
    funcName: 'sbc_a_n',
    dasm: 'sbc a, {0}',
    args: [ArgType.Byte]
}
Z80.prototype.opcodeTable[0xd6] = {
    funcName: 'sub_a_n',
    dasm: 'sub {0}',
    args: [ArgType.Byte]
}

// #BEGIN [adc,add,sbc,sub]_a_n
Z80.prototype.$0 = function () {
    this.r1.a = this.doArithmetics(this.read8(this.pc++), hasCarry_$1, isSub_$1)
}

// #END

// ADD a, (hl/ix+d/iy+d)
Z80.prototype.opcodeTable[0x86] = {
    funcName: 'add_a__hl_',
    dasm: 'add a, (hl)',
    args: []
}
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x86] = {
        funcName: `add_a__${pref}_d_`,
        dasm: `add a, (${pref}{0})`,
        args: [ArgType.Offset]
    }
}

// ADC a, (hl/ix+d/iy+d)
Z80.prototype.opcodeTable[0x8e] = {
    funcName: 'adc_a__hl_',
    dasm: 'adc a, (hl)',
    args: []
}
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x8e] = {
        funcName: `adc_a__${pref}_d_`,
        dasm: `adc a, (${pref}{0})`,
        args: [ArgType.Offset]
    }
}

// SUB a, (hl/ix+d/iy+d)
Z80.prototype.opcodeTable[0x96] = {
    funcName: 'sub_a__hl_',
    dasm: 'sub (hl)',
    args: []
}
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x96] = {
        funcName: `sub_a__${pref}_d_`,
        dasm: `sub (${pref}{0})`,
        args: [ArgType.Offset]
    }
}

// SBC a, (hl/ix+d/iy+d)
Z80.prototype.opcodeTable[0x9e] = {
    funcName: 'sbc_a__hl_',
    dasm: 'sbc a, (hl)',
    args: []
}
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x9e] = {
        funcName: `sbc_a__${pref}_d_`,
        dasm: `sbc a, (${pref}{0})`,
        args: [ArgType.Offset]
    }
}

// #BEGIN [adc,sbc,add,sub]_a__hl_
Z80.prototype.$0 = function () {
    this.r1.a = this.doArithmetics(this.read8(this.r1.hl), hasCarry_$1, isSub_$1)
}

// #END

// #BEGIN [adc,sbc,add,sub]_a__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 5
    let offset = signed8(this.read8(this.pc++))
    this.r1.a = this.doArithmetics(this.read8(this.r1.$2 + offset), hasCarry_$1, isSub_$1)
}

// #END

// ADD HL, BC/DE/HL/SP
for (let rCode in Register16Map) {
    let opCode = 0b00001001 | (rCode << 4)
    let opFuncName = `add_hl_${Register16Map[rCode]}`
    let disasmString = `add hl, ${Register16Map[rCode]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
    for (let pref in Prefixes) {
        let sReg = Register16Map[rCode] === 'hl' ? pref : Register16Map[rCode]
        opFuncName = `add_${pref}_${sReg}`
        disasmString = `add ${pref}, ${sReg}`
        Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[opCode] = {
            funcName: opFuncName,
            dasm: disasmString,
            args: []
        }
    }
}

// ADC ---//----
for (let rCode in Register16Map) {
    let opCode = 0b01001010 | (rCode << 4)
    let opFuncName = `adc_hl_${Register16Map[rCode]}`
    let disasmString = `adc hl, ${Register16Map[rCode]}`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// SBC ---//----
for (let rCode in Register16Map) {
    let opCode = 0b01000010 | (rCode << 4)
    let opFuncName = `sbc_hl_${Register16Map[rCode]}`
    let disasmString = `sbc hl, ${Register16Map[rCode]}`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN [add,adc,sbc]_hl_[hl,bc,de,sp]
Z80.prototype.$0 = function () {
    this.tStates += 7
    this.r1.hl = this.doAddWord(this.r1.hl, this.r1.$2, hasCarry_$1, isSub_$1)
}

// #END

// #BEGIN add_[ix,iy]_[ix,iy,bc,de,sp]
Z80.prototype.$0 = function () {
    this.tStates += 7
    this.r1.$1 = this.doAddWord(this.r1.$1, this.r1.$2, false, false)
}

// #END

// AND r
for (let rCode in RegisterMap) {
    let opCode = 0b10100000 | rCode
    let opFuncName = `and_${RegisterMap[rCode]}`
    let disasmString = `and ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0xa4] = { funcName: 'and_ixh', dasm: 'and ixh', args: [] }
Z80.prototype.opcodeTableDD[0xa5] = { funcName: 'and_ixl', dasm: 'and ixl', args: [] }
Z80.prototype.opcodeTableFD[0xa4] = { funcName: 'and_iyh', dasm: 'and iyh', args: [] }
Z80.prototype.opcodeTableFD[0xa5] = { funcName: 'and_iyl', dasm: 'and iyl', args: [] }

// AND n
Z80.prototype.opcodeTable[0xe6] = { funcName: 'and_n', dasm: 'and {0}', args: [ArgType.Byte] }
// AND (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0xa6] = { funcName: 'and__hl_', dasm: 'and (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xa6] = {
        funcName: `and__${pref}_d_`,
        dasm: `and ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// OR r
for (let rCode in RegisterMap) {
    let opCode = 0b10110000 | rCode
    let opFuncName = `or_${RegisterMap[rCode]}`
    let disasmString = `or ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0xb4] = { funcName: 'or_ixh', dasm: 'or ixh', args: [] }
Z80.prototype.opcodeTableDD[0xb5] = { funcName: 'or_ixl', dasm: 'or ixl', args: [] }
Z80.prototype.opcodeTableFD[0xb4] = { funcName: 'or_iyh', dasm: 'or iyh', args: [] }
Z80.prototype.opcodeTableFD[0xb5] = { funcName: 'or_iyl', dasm: 'or iyl', args: [] }

// OR n
Z80.prototype.opcodeTable[0xf6] = { funcName: 'or_n', dasm: 'or {0}', args: [ArgType.Byte] }
// OR (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0xb6] = { funcName: 'or__hl_', dasm: 'or (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xb6] = {
        funcName: `or__${pref}_d_`,
        dasm: `or ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// XOR r
for (let rCode in RegisterMap) {
    let opCode = 0b10101000 | rCode
    let opFuncName = `xor_${RegisterMap[rCode]}`
    let disasmString = `xor ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0xac] = { funcName: 'xor_ixh', dasm: 'xor ixh', args: [] }
Z80.prototype.opcodeTableDD[0xad] = { funcName: 'xor_ixl', dasm: 'xor ixl', args: [] }
Z80.prototype.opcodeTableFD[0xac] = { funcName: 'xor_iyh', dasm: 'xor iyh', args: [] }
Z80.prototype.opcodeTableFD[0xad] = { funcName: 'xor_iyl', dasm: 'xor iyl', args: [] }

// XOR n
Z80.prototype.opcodeTable[0xee] = { funcName: 'xor_n', dasm: 'xor {0}', args: [ArgType.Byte] }
// XOR (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0xae] = { funcName: 'xor__hl_', dasm: 'xor (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xae] = {
        funcName: `xor__${pref}_d_`,
        dasm: `xor ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// #BEGIN [and,or,xor]_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.do_$1(this.r1.$2)
}

// #END

// #BEGIN [and,or,xor]_n
Z80.prototype.$0 = function () {
    this.do_$1(this.read8(this.pc++))
}

// #END

// #BEGIN [and,or,xor]__hl_
Z80.prototype.$0 = function () {
    this.do_$1(this.read8(this.r1.hl))
}

// #END

// #BEGIN [and,or,xor]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 5
    let offset = signed8(this.read8(this.pc++))
    this.do_$1(this.read8(this.r1.$2 + offset))
}

// #END

// CP r
for (let rCode in RegisterMap) {
    let opCode = 0b10111000 | rCode
    let opFuncName = `cp_${RegisterMap[rCode]}`
    let disasmString = `cp ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0xbc] = { funcName: 'cp_ixh', dasm: 'cp ixh', args: [] }
Z80.prototype.opcodeTableDD[0xbd] = { funcName: 'cp_ixl', dasm: 'cp ixl', args: [] }
Z80.prototype.opcodeTableFD[0xbc] = { funcName: 'cp_iyh', dasm: 'cp iyh', args: [] }
Z80.prototype.opcodeTableFD[0xbd] = { funcName: 'cp_iyl', dasm: 'cp iyl', args: [] }

// CP n
Z80.prototype.opcodeTable[0xfe] = { funcName: 'cp_n', dasm: 'cp {0}', args: [ArgType.Byte] }
// CP (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0xbe] = { funcName: 'cp__hl_', dasm: 'cp (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xbe] = {
        funcName: `cp__${pref}_d_`,
        dasm: `cp ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// #BEGIN cp_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.doArithmetics(this.r1.$1, false, true)
    this.adjustFlags(this.r1.$1)
}

// #END

Z80.prototype.cp__hl_ = function () {
    this.doCPHL()
}

// #BEGIN cp__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 5
    let offset = signed8(this.read8(this.pc++))
    let value = this.read8(this.r1.$1 + offset)
    this.doArithmetics(value, false, true)
    this.adjustFlags(value)
}

// #END

Z80.prototype.cp_n = function () {
    let value = this.read8(this.pc++)
    this.doArithmetics(value, false, true)
    this.adjustFlags(value)
}

// INC r
for (let rCode in RegisterMap) {
    let opCode = 0b00000100 | (rCode << 3)
    let opFuncName = `inc_${RegisterMap[rCode]}`
    let disasmString = `inc ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0x24] = { funcName: 'inc_ixh', dasm: 'inc ixh', args: [] }
Z80.prototype.opcodeTableDD[0x2c] = { funcName: 'inc_ixl', dasm: 'inc ixl', args: [] }
Z80.prototype.opcodeTableFD[0x24] = { funcName: 'inc_iyh', dasm: 'inc iyh', args: [] }
Z80.prototype.opcodeTableFD[0x2c] = { funcName: 'inc_iyl', dasm: 'inc iyl', args: [] }

// INC (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0x34] = { funcName: 'inc__hl_', dasm: 'inc (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x34] = {
        funcName: `inc__${pref}_d_`,
        dasm: `inc ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// DEC r
for (let rCode in RegisterMap) {
    let opCode = 0b00000101 | (rCode << 3)
    let opFuncName = `dec_${RegisterMap[rCode]}`
    let disasmString = `dec ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTable[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableDD[0x25] = { funcName: 'dec_ixh', dasm: 'dec ixh', args: [] }
Z80.prototype.opcodeTableDD[0x2d] = { funcName: 'dec_ixl', dasm: 'dec ixl', args: [] }
Z80.prototype.opcodeTableFD[0x25] = { funcName: 'dec_iyh', dasm: 'dec iyh', args: [] }
Z80.prototype.opcodeTableFD[0x2d] = { funcName: 'dec_iyl', dasm: 'dec iyl', args: [] }

// DEC (HL/IX+d/IY+d)
Z80.prototype.opcodeTable[0x35] = { funcName: 'dec__hl_', dasm: 'dec (hl)', args: [] }
for (let pref in Prefixes) {
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x35] = {
        funcName: `dec__${pref}_d_`,
        dasm: `dec ${pref}{0}`,
        args: [ArgType.Offset]
    }
}

// #BEGIN [inc,dec]_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.r1.$2 = this.doIncDec(this.r1.$2, isDec_$1)
}

// #END

// #BEGIN [inc,dec]__hl_
Z80.prototype.$0 = function () {
    this.tStates++
    let value = this.read8(this.r1.hl)
    this.write8(this.r1.hl, this.doIncDec(value, isDec_$1))
}

// #END

// #BEGIN [inc,dec]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 6
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$2 + offset
    let value = this.read8(addr)
    this.write8(addr, this.doIncDec(value, isDec_$1))
}

// #END

// INC ss
for (let rCode in Register16Map) {
    let opCode = 0b00000011 | (rCode << 4)
    let opFuncName = `inc_${Register16Map[rCode]}`
    let disasmString = `inc ${Register16Map[rCode]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
for (let pref in Prefixes) {
    let opFuncName = `inc_${pref}`
    let disasmString = `inc ${pref}`
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x23] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// DEC ss
for (let rCode in Register16Map) {
    let opCode = 0b00001011 | (rCode << 4)
    let opFuncName = `dec_${Register16Map[rCode]}`
    let disasmString = `dec ${Register16Map[rCode]}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
for (let pref in Prefixes) {
    let opFuncName = `dec_${pref}`
    let disasmString = `dec ${pref}`
    Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0x2b] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN inc_[hl,sp,de,bc,ix,iy]
Z80.prototype.$0 = function () {
    this.tStates += 2
    this.r1.$1++
}
// #END
// #BEGIN dec_[hl,sp,de,bc,ix,iy]
Z80.prototype.$0 = function () {
    this.tStates += 2
    this.r1.$1--
}
// #END

// DAA
Z80.prototype.opcodeTable[0x27] = { funcName: 'daa', dasm: 'daa', args: [] }
Z80.prototype.daa = function () {
    this.doDAA()
}

// CPL
Z80.prototype.opcodeTable[0x2f] = { funcName: 'cpl', dasm: 'cpl', args: [] }
Z80.prototype.cpl = function () {
    this.r1.a = ~this.r1.a
    this.setFlag(f_h | f_n)
    this.adjustFlags(this.r1.a)
}

// NEG
Z80.prototype.opcodeTableED[0x44] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x54] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x64] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x74] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x4c] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x5c] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x6c] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.opcodeTableED[0x7c] = { funcName: 'neg', dasm: 'neg', args: [] }
Z80.prototype.neg = function () {
    let _t = this.r1.a
    this.r1.a = 0
    this.r1.a = this.doArithmetics(_t, false, true)
    this.setFlag(f_n)
}

// CCF
Z80.prototype.opcodeTable[0x3f] = { funcName: 'ccf', dasm: 'ccf', args: [] }
Z80.prototype.ccf = function () {
    this.valFlag(f_c, !this.getFlag(f_c))
    this.resFlag(f_n)
    this.adjustFlags(this.r1.a)
}
// SCF
Z80.prototype.opcodeTable[0x37] = { funcName: 'scf', dasm: 'scf', args: [] }
Z80.prototype.scf = function () {
    this.setFlag(f_c)
    this.resFlag(f_n | f_h)
    this.adjustFlags(this.r1.a)
}
// HALT
Z80.prototype.opcodeTable[0x76] = { funcName: 'halt', dasm: 'halt', args: [] }
Z80.prototype.halt = function () {
    this.halted = true
    this.pc--
}
// DI
Z80.prototype.opcodeTable[0xf3] = { funcName: 'di', dasm: 'di', args: [] }
Z80.prototype.opcodeTable[0xfb] = { funcName: 'ei', dasm: 'ei', args: [] }

// #BEGIN [di,ei]
Z80.prototype.$0 = function () {
    this.iff1 = ie_$1
    this.iff2 = ie_$1
    this.deferInt = true
}

// #END

// IM
Z80.prototype.opcodeTableED[0x46] = { funcName: 'im_0', dasm: 'im 0', args: [] }
Z80.prototype.opcodeTableED[0x4e] = { funcName: 'im_0', dasm: 'im 0', args: [] }
Z80.prototype.opcodeTableED[0x66] = { funcName: 'im_0', dasm: 'im 0', args: [] }
Z80.prototype.opcodeTableED[0x6e] = { funcName: 'im_0', dasm: 'im 0', args: [] }
Z80.prototype.opcodeTableED[0x56] = { funcName: 'im_1', dasm: 'im 1', args: [] }
Z80.prototype.opcodeTableED[0x76] = { funcName: 'im_1', dasm: 'im 1', args: [] }
Z80.prototype.opcodeTableED[0x5e] = { funcName: 'im_2', dasm: 'im 2', args: [] }
Z80.prototype.opcodeTableED[0x7e] = { funcName: 'im_2', dasm: 'im 2', args: [] }

// #BEGIN im_[0,1,2]
Z80.prototype.$0 = function () {
    this.im = $1
}

// #END

//
// ROTATE INSTRUCTIONS
//
Z80.prototype.opcodeTable[0x07] = { funcName: 'rlca', dasm: 'rlca', args: [] }
Z80.prototype.opcodeTable[0x17] = { funcName: 'rla', dasm: 'rla', args: [] }
Z80.prototype.opcodeTable[0x0f] = { funcName: 'rrca', dasm: 'rrca', args: [] }
Z80.prototype.opcodeTable[0x1f] = { funcName: 'rra', dasm: 'rra', args: [] }

// #BEGIN [rl,rr,rlc,rrc]a
Z80.prototype.$0 = function () {
    this.r1.a = this.do_$1(this.r1.a, false)
}

// #END

// RLC s
for (let rCode in RegisterMap) {
    let opCode = rCode
    let opFuncName = `rlc_${RegisterMap[rCode]}`
    let disasmString = `rlc ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableCB[0x06] = { funcName: 'rlc__hl_', dasm: 'rlc (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x06] = { funcName: 'rlc__ix_d_', dasm: 'rlc (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x06] = { funcName: 'rlc__iy_d_', dasm: 'rlc (iy{0})', args: [ArgType.Offset] }

// RL s
for (let rCode in RegisterMap) {
    let opCode = 0b00010000 | rCode
    let opFuncName = `rl_${RegisterMap[rCode]}`
    let disasmString = `rl ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableCB[0x16] = { funcName: 'rl__hl_', dasm: 'rl (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x16] = { funcName: 'rl__ix_d_', dasm: 'rl (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x16] = { funcName: 'rl__iy_d_', dasm: 'rl (iy{0})', args: [ArgType.Offset] }

// RRC s
for (let rCode in RegisterMap) {
    let opCode = 0b00001000 | rCode
    let opFuncName = `rrc_${RegisterMap[rCode]}`
    let disasmString = `rrc ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableCB[0x0e] = { funcName: 'rrc__hl_', dasm: 'rrc (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x0e] = { funcName: 'rrc__ix_d_', dasm: 'rrc (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x0e] = { funcName: 'rrc__iy_d_', dasm: 'rrc (iy{0})', args: [ArgType.Offset] }

// RR s
for (let rCode in RegisterMap) {
    let opCode = 0b00011000 | rCode
    let opFuncName = `rr_${RegisterMap[rCode]}`
    let disasmString = `rr ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableCB[0x1e] = { funcName: 'rr__hl_', dasm: 'rr (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x1e] = { funcName: 'rr__ix_d_', dasm: 'rr (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x1e] = { funcName: 'rr__iy_d_', dasm: 'rr (iy{0})', args: [ArgType.Offset] }

// #BEGIN [rlc,rrc,rl,rr]_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.r1.$2 = this.do_$1(this.r1.$2, true)
}

// #END

// #BEGIN [rlc,rrc,rl,rr]__hl_
Z80.prototype.$0 = function () {
    this.tStates++
    this.write8(this.r1.hl, this.do_$1(this.read8(this.r1.hl), true))
}

// #END

// #BEGIN [rlc,rrc,rl,rr]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$2 + offset
    this.write8(addr, this.do_$1(this.read8(addr), true))
}

// #END

// SLA m
for (let rCode in RegisterMap) {
    let opCode = 0b00100000 | rCode
    let opFuncName = `sla_${RegisterMap[rCode]}`
    let disasmString = `sla ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableCB[0x26] = { funcName: 'sla__hl_', dasm: 'sla (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x26] = { funcName: 'sla__ix_d_', dasm: 'sla (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x26] = { funcName: 'sla__iy_d_', dasm: 'sla (iy{0})', args: [ArgType.Offset] }
// SRA m
for (let rCode in RegisterMap) {
    let opCode = 0b00101000 | rCode
    let opFuncName = `sra_${RegisterMap[rCode]}`
    let disasmString = `sra ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableCB[0x2e] = { funcName: 'sra__hl_', dasm: 'sra (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x2e] = { funcName: 'sra__ix_d_', dasm: 'sra (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x2e] = { funcName: 'sra__iy_d_', dasm: 'sra (iy{0})', args: [ArgType.Offset] }
// SLL m
for (let rCode in RegisterMap) {
    let opCode = 0b00110000 | rCode
    let opFuncName = `sll_${RegisterMap[rCode]}`
    let disasmString = `sll ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableCB[0x36] = { funcName: 'sll__hl_', dasm: 'sll (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x36] = { funcName: 'sll__ix_d_', dasm: 'sll (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x36] = { funcName: 'sll__iy_d_', dasm: 'sll (iy{0})', args: [ArgType.Offset] }
// SRL m
for (let rCode in RegisterMap) {
    let opCode = 0b00111000 | rCode
    let opFuncName = `srl_${RegisterMap[rCode]}`
    let disasmString = `srl ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
}
Z80.prototype.opcodeTableCB[0x3e] = { funcName: 'srl__hl_', dasm: 'srl (hl)', args: [] }
Z80.prototype.opcodeTableDDCB[0x3e] = { funcName: 'srl__ix_d_', dasm: 'srl (ix{0})', args: [ArgType.Offset] }
Z80.prototype.opcodeTableFDCB[0x3e] = { funcName: 'srl__iy_d_', dasm: 'srl (iy{0})', args: [ArgType.Offset] }

// #BEGIN [sl,sr][l,a]_[a,b,c,d,e,h,l,ixh,ixl,iyh,iyl]
Z80.prototype.$0 = function () {
    this.r1.$3 = this.do_$1(this.r1.$3, isArithmetics_$2)
}

// #END

// #BEGIN [sl,sr][l,a]__hl_
Z80.prototype.$0 = function () {
    this.tStates++
    this.write8(this.r1.hl, this.do_$1(this.read8(this.r1.hl), isArithmetics_$2))
}

// #END

// #BEGIN [sl,sr][l,a]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$3 + offset
    this.write8(addr, this.do_$1(this.read8(addr), isArithmetics_$2))
}

// #END

// RLD
Z80.prototype.opcodeTableED[0x6f] = { funcName: 'rld', dasm: 'rld', args: [] }
Z80.prototype.rld = function () {
    this.tStates += 4
    let ah = this.r1.a & 0x0f
    let hl = this.read8(this.r1.hl)
    this.r1.a = (this.r1.a & 0xf0) | ((hl & 0xf0) >> 4)
    hl = (hl << 4) | ah
    this.write8(this.r1.hl, hl)
    this.resFlag(f_h | f_n)
    this.adjustFlagSZP(this.r1.a)
    this.adjustFlags(this.r1.a)
}

// RRD
Z80.prototype.opcodeTableED[0x67] = { funcName: 'rrd', dasm: 'rrd', args: [] }
Z80.prototype.rrd = function () {
    this.tStates += 4
    let ah = this.r1.a & 0x0f
    let hl = this.read8(this.r1.hl)
    this.r1.a = (this.r1.a & 0xf0) | (hl & 0x0f)
    hl = (hl >> 4) | (ah << 4)
    this.write8(this.r1.hl, hl)
    this.resFlag(f_h | f_n)
    this.adjustFlagSZP(this.r1.a)
}

// BIT
for (let b = 0; b < 8; b++) {
    for (let rCode in RegisterMap) {
        let opCode = 0b01000000 | (b << 3) | rCode
        let opFuncName = `bit_${b}_${RegisterMap[rCode]}`
        let disasmString = `bit ${b}, ${RegisterMap[rCode]}`
        Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
    }
    let opCode = 0b01000110 | (b << 3)
    Z80.prototype.opcodeTableCB[opCode] = { funcName: `bit_${b}__hl_`, dasm: `bit ${b}, (hl)`, args: [] }
    for (let s = 0; s < 8; s++) {
        let opCode = 0b01000000 | (b << 3) | s
        Z80.prototype.opcodeTableDDCB[opCode] = { funcName: `bit_${b}__ix_d_`, dasm: `bit ${b}, (ix{0})`, args: [] }
        Z80.prototype.opcodeTableFDCB[opCode] = { funcName: `bit_${b}__iy_d_`, dasm: `bit ${b}, (iy{0})`, args: [] }
    }
}

// #BEGIN bit_[0,1,2,3,4,5,6,7]_[a,b,c,d,e,h,l]
Z80.prototype.$0 = function () {
    this.doBitR($1, this.r1.$2)
}

// #END

// #BEGIN bit_[0,1,2,3,4,5,6,7]__hl_
Z80.prototype.$0 = function () {
    this.tStates++
    this.doBitR($1, this.read8(this.r1.hl))
}

// #END

// #BEGIN bit_[0,1,2,3,4,5,6,7]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$2 + offset
    this.doBitIndexed($1, addr)
}

// #END

// SET/RES
for (let b = 0; b < 8; b++) {
    for (let rCode in RegisterMap) {
        let opCode = 0b11000000 | (b << 3) | rCode
        let opFuncName = `set_${b}_${RegisterMap[rCode]}`
        let disasmString = `set ${b}, ${RegisterMap[rCode]}`
        Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
    }
    let opCode = 0b11000110 | (b << 3)
    Z80.prototype.opcodeTableCB[opCode] = { funcName: `set_${b}__hl_`, dasm: `set ${b}, (hl)`, args: [] }
    Z80.prototype.opcodeTableDDCB[opCode] = { funcName: `set_${b}__ix_d_`, dasm: `set ${b}, (ix{0})`, args: [] }
    Z80.prototype.opcodeTableFDCB[opCode] = { funcName: `set_${b}__iy_d_`, dasm: `set ${b}, (iy{0})`, args: [] }
}

for (let b = 0; b < 8; b++) {
    for (let rCode in RegisterMap) {
        let opCode = 0b10000000 | (b << 3) | rCode
        let opFuncName = `res_${b}_${RegisterMap[rCode]}`
        let disasmString = `res ${b}, ${RegisterMap[rCode]}`
        Z80.prototype.opcodeTableCB[opCode] = { funcName: opFuncName, dasm: disasmString, args: [] }
    }
    let opCode = 0b10000110 | (b << 3)
    Z80.prototype.opcodeTableCB[opCode] = { funcName: `res_${b}__hl_`, dasm: `res ${b}, (hl)`, args: [] }
    Z80.prototype.opcodeTableDDCB[opCode] = { funcName: `res_${b}__ix_d_`, dasm: `res ${b}, (ix{0})`, args: [] }
    Z80.prototype.opcodeTableFDCB[opCode] = { funcName: `res_${b}__iy_d_`, dasm: `res ${b}, (iy{0})`, args: [] }
}

// #BEGIN [set,res]_[0,1,2,3,4,5,6,7]_[a,b,c,d,e,h,l]
Z80.prototype.$0 = function () {
    this.r1.$3 = this.doSetRes(bitState_$1, $2, this.r1.$3)
}

// #END

// #BEGIN [set,res]_[0,1,2,3,4,5,6,7]__hl_
Z80.prototype.$0 = function () {
    this.tStates++
    this.write8(this.r1.hl, this.doSetRes(bitState_$1, $2, this.read8(this.r1.hl)))
}

// #END

// #BEGIN [set,res]_[0,1,2,3,4,5,6,7]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$3 + offset
    this.write8(addr, this.doSetRes(bitState_$1, $2, this.read8(addr)))
}

// #END

// IN A, (n)
Z80.prototype.opcodeTable[0xdb] = { funcName: 'in_a__n_', dasm: 'in a, (0x{0})', args: [ArgType.Byte] }
Z80.prototype.in_a__n_ = function () {
    let port = this.read8(this.pc++)
    port = (this.r1.a << 8) | port
    this.r1.a = this.ioread(port)
}

// OUT (n), A
Z80.prototype.opcodeTable[0xd3] = { funcName: 'out__n__a', dasm: 'out (0x{0}), a', args: [ArgType.Byte] }
Z80.prototype.out__n__a = function () {
    let port = this.read8(this.pc++)
    port = (this.r1.a << 8) | port
    this.iowrite(port, this.r1.a)
}

// IN r, (C)
for (let rCode in RegisterMap) {
    let opCode = 0b01000000 | (rCode << 3)
    let opFuncName = `in_${RegisterMap[rCode]}__c_`
    let disasmString = `in ${RegisterMap[rCode]}, (c)`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}
Z80.prototype.opcodeTableED[0x70] = {
    funcName: 'in_f__c_',
    dasm: 'in f, (c)',
    args: []
}

// #BEGIN in_[a,b,c,d,e,f,h,l]__c_
Z80.prototype.$0 = function () {
    this.r1.$1 = this.ioread(this.r1.bc)
    this.resFlag(f_h | f_n)
    this.adjustFlagSZP(this.r1.$1)
    this.adjustFlags(this.r1.$1)
}

// #END

// INI
Z80.prototype.opcodeTableED[0xa2] = { funcName: 'ini', dasm: 'ini', args: [] }
Z80.prototype.ini = function () {
    this.tStates++
    let value = this.ioread(this.r1.bc)
    this.write8(this.r1.hl++, value)
    this.r1.b = this.doIncDec(this.r1.b, isDec_dec)
    this.valFlag(f_n, (value & 0x80) !== 0)
    let fv = value + ((this.r1.c + 1) & 0xff)
    this.valFlag(f_h, fv > 0xff)
    this.valFlag(f_c, fv > 0xff)
    this.valFlag(f_pv, ParityBit[(fv & 7) ^ this.r1.b])
}
// INIR
Z80.prototype.opcodeTableED[0xb2] = { funcName: 'inir', dasm: 'inir', args: [] }
Z80.prototype.inir = function () {
    this.ini()
    if (this.r1.b !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// IND
Z80.prototype.opcodeTableED[0xaa] = { funcName: 'ind', dasm: 'ind', args: [] }
Z80.prototype.ind = function () {
    this.tStates++
    let value = this.ioread(this.r1.bc)
    this.write8(this.r1.hl--, value)
    this.r1.b = this.doIncDec(this.r1.b, isDec_dec)
    this.valFlag(f_n, (value & 0x80) !== 0)
    let fv = value + ((this.r1.c - 1) & 0xff)
    this.valFlag(f_h, fv > 0xff)
    this.valFlag(f_c, fv > 0xff)
    this.valFlag(f_pv, ParityBit[(fv & 7) ^ this.r1.b])
}
// INDR
Z80.prototype.opcodeTableED[0xba] = { funcName: 'indr', dasm: 'indr', args: [] }
Z80.prototype.indr = function () {
    this.ind()
    if (this.r1.b !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// OUT (C), r
for (let rCode in RegisterMap) {
    let opCode = 0b01000001 | (rCode << 3)
    let opFuncName = `out__c__${RegisterMap[rCode]}`
    let disasmString = `out (c), ${RegisterMap[rCode]}`
    Z80.prototype.opcodeTableED[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

// #BEGIN out__c__[a,b,c,d,e,h,l]
Z80.prototype.$0 = function () {
    this.iowrite(this.r1.bc, this.r1.$1)
}

Z80.prototype.opcodeTableED[0x71] = { funcName: 'out__c__0', dasm: 'out (c), 0', args: [] }
Z80.prototype.out__c__0 = function () {
    this.iowrite(this.r1.bc, 0)
}

// #END

// OUTI
Z80.prototype.opcodeTableED[0xa3] = { funcName: 'outi', dasm: 'outi', args: [] }
Z80.prototype.outi = function () {
    this.tStates++
    let value = this.read8(this.r1.hl++)
    this.r1.b = this.doIncDec(this.r1.b, isDec_dec)

    this.iowrite(this.r1.bc, value)
    let fv = value + this.r1.l
    this.valFlag(f_n, (value & 0x80) !== 0)
    this.valFlag(f_h, fv > 0xff)
    this.valFlag(f_c, fv > 0xff)
    this.valFlag(f_pv, ParityBit[(fv & 7) ^ this.r1.b])
    this.adjustFlags(this.r1.b)
}
// OTIR
Z80.prototype.opcodeTableED[0xb3] = { funcName: 'otir', dasm: 'otir', args: [] }
Z80.prototype.otir = function () {
    this.outi()
    if (this.r1.b !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// OUTD
Z80.prototype.opcodeTableED[0xab] = { funcName: 'outd', dasm: 'outd', args: [] }
Z80.prototype.outd = function () {
    this.tStates++
    let value = this.read8(this.r1.hl--)
    this.r1.b = this.doIncDec(this.r1.b, isDec_dec)

    this.iowrite(this.r1.bc, value)
    let fv = value + this.r1.l
    this.valFlag(f_n, (value & 0x80) !== 0)
    this.valFlag(f_h, fv > 0xff)
    this.valFlag(f_c, fv > 0xff)
    this.valFlag(f_pv, ParityBit[(fv & 7) ^ this.r1.b])
    this.adjustFlags(this.r1.b)
}
// OTDR
Z80.prototype.opcodeTableED[0xbb] = { funcName: 'otdr', dasm: 'otdr', args: [] }
Z80.prototype.otdr = function () {
    this.outd()
    if (this.r1.b !== 0) {
        this.tStates += 5
        this.pc -= 2
    }
}

// JP nn
Z80.prototype.opcodeTable[0xc3] = {
    funcName: 'jp_nn',
    dasm: 'jp {0}',
    args: [ArgType.Word]
}

Z80.prototype.jp_nn = function () {
    this.pc = this.read16(this.pc)
}

// JP cc, nn
for (let cond in Conditions) {
    let opCode = 0b11000010 | (Conditions[cond] << 3)
    let opFuncName = `jp_${cond}_nn`
    let disasmString = `jp_${cond}_{0}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN jp_[c,nc,z,nz,pe,po,p,m]_nn
Z80.prototype.$0 = function () {
    // no matter if the condition is true we spend 6 tstates to read the addr
    let addr = this.read16(this.pc)
    if (this.condition(c_$1)) {
        this.pc = addr
    } else {
        this.pc += 2
    }
}

// #END

Z80.prototype.opcodeTable[0x18] = { funcName: 'jr_pc_e', dasm: 'jr pc{0}', args: [ArgType.Offset] }
Z80.prototype.jr_pc_e = function () {
    let offset = signed8(this.read8(this.pc++))
    this.tStates += 5
    this.pc = this.pc + offset
}

Z80.prototype.opcodeTable[0x38] = { funcName: 'jr_c_pc_e', dasm: 'jr c, pc{0}', args: [ArgType.Offset] }
Z80.prototype.opcodeTable[0x30] = { funcName: 'jr_nc_pc_e', dasm: 'jr nc, pc{0}', args: [ArgType.Offset] }
Z80.prototype.opcodeTable[0x28] = { funcName: 'jr_z_pc_e', dasm: 'jr z, pc{0}', args: [ArgType.Offset] }
Z80.prototype.opcodeTable[0x20] = { funcName: 'jr_nz_pc_e', dasm: 'jr nz, pc{0}', args: [ArgType.Offset] }

// #BEGIN jr_[c,nc,z,nz]_pc_e
Z80.prototype.$0 = function () {
    let offset = signed8(this.read8(this.pc++))
    if (this.condition(c_$1)) {
        this.tStates += 5
        this.pc = this.pc + offset
    }
}

// #END

// JP (HL/IX/IY)
Z80.prototype.opcodeTable[0xe9] = { funcName: 'jp__hl_', dasm: 'jp (hl)', args: [] }
Z80.prototype.opcodeTableDD[0xe9] = { funcName: 'jp__ix_', dasm: 'jp (ix)', args: [] }
Z80.prototype.opcodeTableFD[0xe9] = { funcName: 'jp__iy_', dasm: 'jp (iy)', args: [] }

// #BEGIN jp__[hl,ix,iy]_
Z80.prototype.$0 = function () {
    this.pc = this.r1.$1
}
// #END

// DJNZ
Z80.prototype.opcodeTable[0x10] = { funcName: 'djnz_pc_e', dasm: 'djnz pc{0}', args: [ArgType.Offset] }
Z80.prototype.djnz_pc_e = function () {
    this.tStates++
    let offset = signed8(this.read8(this.pc++))
    this.r1.b--
    if (this.r1.b !== 0) {
        this.tStates += 5
        this.pc = this.pc + offset
    }
}

// CALL nn
Z80.prototype.opcodeTable[0xcd] = { funcName: 'call_nn', dasm: 'call {0}', args: [ArgType.Word] }
Z80.prototype.call_nn = function () {
    let addr = this.read16(this.pc)
    this.pc += 2
    this.tStates++
    this.doPush(this.pc)
    this.pc = addr
}

// CALL cc, nn
for (let cond in Conditions) {
    let opCode = 0b11000100 | (Conditions[cond] << 3)
    let opFuncName = `call_${cond}_nn`
    let disasmString = `call ${cond}, {0}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN call_[c,m,nc,nz,p,pe,po,z]_nn
Z80.prototype.$0 = function () {
    let addr = this.read16(this.pc)
    this.pc += 2
    if (this.condition(c_$1)) {
        this.tStates++
        this.doPush(this.pc)
        this.pc = addr
    }
}

// #END

// RET
Z80.prototype.opcodeTable[0xc9] = { funcName: 'ret', dasm: 'ret', args: [] }
Z80.prototype.ret = function () {
    this.pc = this.doPop()
}

// RET cc
for (let cond in Conditions) {
    let opCode = 0b11000000 | (Conditions[cond] << 3)
    let opFuncName = `ret_${cond}`
    let disasmString = `ret ${cond}`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: [ArgType.Word]
    }
}

// #BEGIN ret_[c,m,nc,nz,p,pe,po,z]
Z80.prototype.$0 = function () {
    this.tStates++
    if (this.condition(c_$1)) {
        this.ret()
    }
}

// #END

let retnDeclaration = { funcName: 'retn', dasm: 'retn', args: [] }
Z80.prototype.opcodeTableED[0x45] = retnDeclaration
Z80.prototype.opcodeTableED[0x55] = retnDeclaration
Z80.prototype.opcodeTableED[0x65] = retnDeclaration
Z80.prototype.opcodeTableED[0x75] = retnDeclaration
Z80.prototype.opcodeTableED[0x5d] = retnDeclaration
Z80.prototype.opcodeTableED[0x6d] = retnDeclaration
Z80.prototype.opcodeTableED[0x7d] = retnDeclaration

Z80.prototype.retn = function () {
    this.iff1 = this.iff2
    this.ret()
}

Z80.prototype.opcodeTableED[0x4d] = { funcName: 'reti', dasm: 'reti', args: [] }
Z80.prototype.reti = function () {
    this.iff1 = this.iff2
    this.ret()
}

// RST p
for (let p = 0; p < 8; p++) {
    let vector = p << 3
    let opCode = 0b11000111 | vector
    let opFuncName = `rst_${hex8(vector)}h`
    let disasmString = `rst ${hex8(vector)}h`
    Z80.prototype.opcodeTable[opCode] = {
        funcName: opFuncName,
        dasm: disasmString,
        args: []
    }
}

/* #BEGIN rst_[00,08,10,18,20,28,30,38]h
Z80.prototype.$0 = function() {
  this.tStates += 1
  this.doPush(this.pc)
  this.pc = 0x$1
}

  #END */

// Sophisticated indexed acc loads
for (let b = 0; b < 8; b++) {
    let opCode
    for (let rCode in RegisterMap) {
        opCode = 0b10000000 | rCode | (b << 3)
        Z80.prototype.opcodeTableDDCB[opCode] = {
            funcName: `ld_${RegisterMap[rCode]}_res_${b}__ix_d_`,
            dasm: `ld ${RegisterMap[rCode]}, res ${b}, (ix{0})`,
            args: [ArgType.Offset]
        }
        Z80.prototype.opcodeTableFDCB[opCode] = {
            funcName: `ld_${RegisterMap[rCode]}_res_${b}__iy_d_`,
            dasm: `ld ${RegisterMap[rCode]}, res ${b}, (iy{0})`,
            args: [ArgType.Offset]
        }
        opCode = 0b11000000 | rCode | (b << 3)
        Z80.prototype.opcodeTableDDCB[opCode] = {
            funcName: `ld_${RegisterMap[rCode]}_set_${b}__ix_d_`,
            dasm: `ld ${RegisterMap[rCode]}, set ${b}, (ix{0})`,
            args: [ArgType.Offset]
        }
        Z80.prototype.opcodeTableFDCB[opCode] = {
            funcName: `ld_${RegisterMap[rCode]}_set_${b}__iy_d_`,
            dasm: `ld ${RegisterMap[rCode]}, set ${b}, (iy{0})`,
            args: [ArgType.Offset]
        }
    }
}

for (let rCode in RegisterMap) {
    let iMap = {
        0b00: 'sla',
        0b01: 'sra',
        0b10: 'sll',
        0b11: 'srl'
    }
    for (let op in iMap) {
        for (let pref in Prefixes) {
            let opCode = 0b00100000 | (op << 3) | rCode
            let opFuncName = `ld_${RegisterMap[rCode]}_${iMap[op]}__${pref}_d_`
            let disasmString = `ld ${RegisterMap[rCode]}, ${iMap[op]} (${pref}{0})`
            Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xcb].nextTable[opCode] = {
                funcName: opFuncName,
                dasm: disasmString,
                args: [ArgType.Offset]
            }
        }
    }
}

// #BEGIN ld_[a,b,c,d,e,h,l]_[set,res]_[0,1,2,3,4,5,6,7]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$4 + offset
    this.r1.$1 = this.doSetRes(bitState_$2, $3, this.read8(addr))
    this.write8(addr, this.r1.$1)
}

// #END

// #BEGIN ld_[a,b,c,d,e,h,l]_[sl,sr][a,l]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$4 + offset
    this.r1.$1 = this.do_$2(this.read8(addr), isArithmetics_$3)
    this.write8(addr, this.r1.$1)
}

// #END

for (let rCode in RegisterMap) {
    let iMap = {
        0b00: 'rlc',
        0b01: 'rrc',
        0b10: 'rl',
        0b11: 'rr'
    }
    for (let op in iMap) {
        for (let pref in Prefixes) {
            let opCode = rCode | (op << 3)
            let opFuncName = `ld_${RegisterMap[rCode]}_${iMap[op]}__${pref}_d_`
            let disasmString = `ld ${RegisterMap[rCode]}, ${iMap[op]} (${pref}{0})`
            Z80.prototype.opcodeTable[Prefixes[pref]].nextTable[0xcb].nextTable[opCode] = {
                funcName: opFuncName,
                dasm: disasmString,
                args: [ArgType.Offset]
            }
        }
    }
}

// #BEGIN ld_[a,b,c,d,e,h,l]_[rl,rlc,rr,rrc]__[ix,iy]_d_
Z80.prototype.$0 = function () {
    this.tStates += 2
    let offset = signed8(this.read8(this.pc++))
    let addr = this.r1.$3 + offset
    this.r1.$1 = this.do_$2(this.read8(addr), true)
    this.write8(addr, this.r1.$1)
}

// #END

for (let code = 0; code < 256; code++) {
    if (!Z80.prototype.opcodeTableDD[code]) {
        Z80.prototype.opcodeTableDD[code] = Z80.prototype.opcodeTable[code]
    }
    if (!Z80.prototype.opcodeTableFD[code]) {
        Z80.prototype.opcodeTableFD[code] = Z80.prototype.opcodeTable[code]
    }
}

const ParityBit = [
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true,
    false,
    true,
    true,
    false,
    false,
    true,
    true,
    false,
    true,
    false,
    false,
    true
]
