
class Breakpoints {
    constructor() {
        this.breakpoints = {}
    }

    bpAt(addr) {
        return this.breakpoints[addr] !== undefined;
    }

    toggle(addr) {
        if (this.bpAt(addr)) {
            delete this.breakpoints[addr];
        } else {
            this.breakpoints[addr] = {
                addr: addr,
            };
        }
    }

    getJson() {
        return { execbps: this.breakpoints };
    }
}
