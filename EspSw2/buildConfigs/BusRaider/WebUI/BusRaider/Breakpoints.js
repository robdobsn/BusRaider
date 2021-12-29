
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

    // add(breakpoint) {
    //     this.breakpoints.push(breakpoint);
    // }

    // getBreakpoints() {
    //     return this.breakpoints;
    // }

    // getBreakpoint(breakpointId) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].id == breakpointId) {
    //             return this.breakpoints[i];
    //         }
    //     }
    //     return null;
    // }

    // getBreakpointByName(breakpointName) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].name == breakpointName) {
    //             return this.breakpoints[i];
    //         }
    //     }
    //     return null;
    // }

    // getBreakpointByAddress(breakpointAddress) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].address == breakpointAddress) {
    //             return this.breakpoints[i];
    //         }
    //     }
    //     return null;
    // }

    // getBreakpointByIndex(breakpointIndex) {
    //     if (breakpointIndex < this.breakpoints.length) {
    //         return this.breakpoints[breakpointIndex];
    //     }
    //     return null;
    // }

    // getBreakpointIndex(breakpointId) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].id == breakpointId) {
    //             return i;
    //         }
    //     }
    //     return -1;
    // }

    // getBreakpointIndexByAddress(breakpointAddress) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].address == breakpointAddress) {
    //             return i;
    //         }
    //     }
    //     return -1;
    // }

    // getBreakpointIndexByName(breakpointName) {
    //     for (let i = 0; i < this.breakpoints.length; i++) {
    //         if (this.breakpoints[i].name == breakpointName) {
    //             return i;
    //         }
    //     }
    //     return -1;
    // }
}
