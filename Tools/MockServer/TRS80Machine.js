

class TRS80Machine {
    constructor(memAccess) {
        this.memAccess = memAccess;
    }

    getScreenMem() {
        return this.memAccess.blockRead(0x3c00, 0x400, false);
    }

    getScreenSize() {
        return [64, 16];
    }

    getScreenIfUpdated(screenCache) {
        let isChanged = false;
        const screenMem = this.getScreenMem();
        if (screenCache === null) {
            isChanged = true;
            console.log(`getScreenIfUpdated screenCache null`);
        } else if (screenCache.length !== screenMem.length) {
            isChanged = true;
        } else {
            for (let i = 0; i < screenCache.length; i++) {
                if (screenCache[i] !== screenMem[i]) {
                    isChanged = true;
                    break;
                }
            }
        }
        if (isChanged) {
            console.log(`screen changed len ${screenMem.length}`);
            screenCache = screenMem;
            return screenCache;
        }
        return null;
    }
};

module.exports = TRS80Machine;