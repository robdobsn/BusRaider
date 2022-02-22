
class Debugger {
    constructor() {
        this.state = {
        }
        this.scaderName = "Debugger";
        this.friendlyName = "Debugger";
        this.configObj = {};
        this.objGlobalStr = "";
        this.memory = new Memory();
        this.port = new Port();
        this.z80 = new Z80(this.memory, this.port, true);
        this.breakpoints = new Breakpoints();
    }

    postInit() {

    }

    getId() {
        return this.scaderName;
    }

    defaultConfig() {
        return {
            enable: false
        }
    }

    getMsgHandlers() {
        return [];
    }

    debuggerShowClick(event) {
        event.srcElement.classList.toggle("radio-on");
        let debuggerPanel = document.getElementById("debugger-panel");
        if (event.srcElement.classList.contains("radio-on")) {
            debuggerPanel.classList.remove("panel-hidden");
            event.srcElement.innerHTML = "Hide Debugger";
        }
        else {
            debuggerPanel.classList.add("panel-hidden");
            event.srcElement.innerHTML = "Show Debugger";
        }
    }

    debuggerFlagStr(flags) {
        let flagStr = "";
        for (const [key, elem] of Object.entries(flags)) {
            flagStr += (elem !== 0) ? key : " ";
        }
        return flagStr;
    }

    enableDebugger(en) {
        const elList = [document.getElementById('debugger-disregs'),
        document.getElementById('debugger-mem1'),
        document.getElementById('debugger-mem2')];
        elList.forEach((el) => {
            if (en) {
                el.classList.add("uiDisable");
            } else {
                el.classList.remove("uiDisable");
            }
        });
    }

    hexDisplayUpdate(jsonRslt, elementId) {
        console.log(jsonRslt, elementId);
        const rslt = JSON.parse(jsonRslt);
        const memData = rslt.data;
        let addr = rslt.addr;
        const el = document.getElementById(elementId);
        let dumpHtml = "";
        for (let i = 0; i < memData.length; i += 2) {
            if (i % 32 === 0) {
                if (i !== 0)
                    dumpHtml += "\n";
                dumpHtml += intToHex(addr, 4) + " ";
                addr += 16;
            }
            dumpHtml += intToHex(memData.slice(i, i + 2), 2) + " ";
        }
        el.innerHTML = dumpHtml;
    }

    showHexDump(addr, memDumpElementId) {
        ajaxGet('/api/targetcmd/rd/' + intToHex(addr, 4) + '/100', this.hexDisplayUpdate, null, memDumpElementId);
    }

    showHexDumps() {
        let el = document.getElementById('input-debug-mem1-addr');
        let addr = parseInt(el.value, 16);
        this.showHexDump(addr, 'debugger-mem1');
        el = document.getElementById('input-debug-mem2-addr');
        addr = parseInt(el.value, 16);
        this.showHexDump(addr, 'debugger-mem2');
    }

    getRegValue(reg, regsObj) {
        switch (reg) {
            case "PC":
                return intToHex(regsObj.pc, 4);
                break;
            case "SP":
                return intToHex(regsObj.sp, 4);
                break;
            case "A":
                return intToHex(regsObj.a, 2);
                break;
            case "ADASH":
                return intToHex(regsObj.a_prime, 2);
                break;
            case "HL":
                return intToHex(regsObj.h << 8 | regsObj.l, 4);
                break;
            case "DE":
                return intToHex(regsObj.d << 8 | regsObj.e, 4);
                break;
            case "BC":
                return intToHex(regsObj.b << 8 | regsObj.c, 4);
                break;
            case "HLDASH":
                return intToHex(regsObj.h_prime << 8 | regsObj.l_prime, 4);
                break;
            case "DEDASH":
                return intToHex(regsObj.d_prime << 8 | regsObj.e_prime, 4);
                break;
            case "BCDASH":
                return intToHex(regsObj.b_prime << 8 | regsObj.c_prime, 4);
                break;
            case "IY":
                return intToHex(regsObj.iy, 4);
                break;
            case "IX":
                return intToHex(regsObj.ix, 4);
                break;
            case "I":
                return intToHex(regsObj.i, 2);
                break;
            case "R":
                return intToHex(regsObj.r, 2);
                break;
            case "IM":
                return "" + regsObj.imode;
                break;
            case "IFF":
                return "1:" + regsObj.iff1 + " 2:" + regsObj.iff2;
                break;
        }
        return "";
    }

    debuggerUpdate(rslt) {
        this.enableDebugger(true)
        console.log(rslt);
        // Regs
        let curPCAddr = 0;
        const mcRsltObj = JSON.parse(rslt);
        if (mcRsltObj.regs !== undefined) {
            const regsObj = mcRsltObj.regs
            curPCAddr = regsObj.pc;
            const regNames = ["PC", "SP", "A", "HL", "DE", "BC", "IX", "IY", "ADASH", "HLDASH", "DEDASH", "BCDASH", "I", "R", "IM", "IFF"];
            regNames.forEach((reg) => {
                const el = document.getElementById("regs-" + reg);
                const regStr = this.getRegValue(reg, regsObj)
                el.innerHTML = typeof regStr === 'string' ? regStr.replace("0x", "").toUpperCase() : regStr;
            });
            const f1 = document.getElementById("regs-FLAGS");
            f1.innerHTML = this.debuggerFlagStr(regsObj.flags);
            const f1Dash = document.getElementById("regs-FLAGSDASH");
            f1Dash.innerHTML = this.debuggerFlagStr(regsObj.flags_prime);
        }
        // Disassembly
        if (mcRsltObj.disasm !== undefined) {
            const el = document.getElementById("debugger-disasm");
            const disasmLines = mcRsltObj.disasm.split('%');
            let disasmText = "";
            for (let i = 0; i < disasmLines.length; i++) {
                lin = disasmLines[i];
                lin = lin.replace(/\~/g, '\t')
                lin = lin.replace(/\^/g, '\t')
                lin = lin.toUpperCase()
                disasmText += lin + '<BR>';
            }
            el.innerHTML = disasmText;
        } else if ((mcRsltObj.mem !== undefined) && (mcRsltObj.addr !== undefined)) {
            const el = document.getElementById("debugger-disasm");
            this.memory.setFromHexStr(mcRsltObj.addr, mcRsltObj.mem);
            let disasmLines = []
            let disasmBPs = []
            let curAddr = mcRsltObj.addr;
            const memLen = mcRsltObj.mem.length;
            let curPCLineIdx = 0;
            while (true) {
                const disRslt = this.z80.disassemble(curAddr);
                if (disRslt.nextAddr >= mcRsltObj.addr + memLen) {
                    break;
                }
                let lineStr = intToHex(curAddr, 4, true) + " ";
                for (let i = 0; i < 4; i++) {
                    if (i < disRslt.nextAddr-curAddr) {
                        lineStr += intToHex(this.memory.read8(curAddr + i), 2, true);
                    } else {
                        lineStr += "  ";
                    }
                }
                disasmLines.push(lineStr + " " + disRslt.dasm);
                disasmBPs.push({addr: curAddr, bp: this.breakpoints.bpAt(curAddr)});
                curAddr = disRslt.nextAddr;
                if (curAddr <= curPCAddr) {
                    curPCLineIdx = disasmLines.length;
                }
            }
            let disasmText = `<pre class="disasm-code">`;
            for (let i = 0; i < disasmLines.length; i++) {
                disasmText += `<div class="disasm-row"><span class="disasm-col-bp" data-addr="${disasmBPs[i].addr}">`;
                disasmText += `<svg class="icon-bp ${disasmBPs[i].bp?'icon-bp-on':''}" data-addr="${disasmBPs[i].addr}"><use data-addr="${disasmBPs[i].addr}" xlink:href="#icon-red-dot"></use></svg>`;
                disasmText += `</span><span class="disasm-col" `;
                disasmText += (curPCLineIdx == i ? `id="disasm-cur-pc">` : `>`) + disasmLines[i] + '</span>';
                disasmText += "</div>";
            }
            disasmText += "</pre>";
            el.innerHTML = disasmText;
            const lineHeight = document.getElementById("disasm-cur-pc").parentElement.offsetHeight;
            let scrollToLineEl = document.getElementById("disasm-cur-pc");
            el.scrollTop = scrollToLineEl.offsetTop - el.offsetTop - lineHeight;
            // make breakpoint icons clickable
            const bpEls = document.getElementsByClassName("disasm-col-bp");
            for (let i = 0; i < bpEls.length; i++) {
                bpEls[i].addEventListener("click", (e) => {
                    console.log(`bp click ${e.target.dataset.addr}`);
                    this.breakpoints.toggle(e.target.dataset.addr);
                    let targetEl = e.target;
                    if (e.target.classList.contains("disasm-col-bp")) {
                        targetEl = e.target.firstElementChild;
                    } else if (!e.target.classList.contains("icon-bp")) {
                        targetEl = e.target.parentElement;
                    }
                    targetEl.classList.toggle("icon-bp-on");
                    this.sendBreakpoints();
                });
            }
        }
        this.showHexDumps();
    }

    sendBreakpoints() {
        const bpJson = this.breakpoints.getJson();
        ajaxPost('/api/targetcmd/setbps', JSON.stringify(bpJson));
    }

    validateHexInput(event, memDumpElementId) {
        console.log(event);
        if (event.code === "Backspace" || event.code === "Delete" || event.code === "Enter")
            return true
        const okKeys = "01234567890ABCDEFabcdef"
        if (okKeys.includes(event.key))
            return true;
        return false
    }
    onChangeHexInput(event, memDumpElementId) {
        console.log(event);
        const val = event.target.value
        // const rslt = /^([0-9A-Fa-f]{1,4})$/.test(val)
        let addr = parseInt(event.target.value, 16);
        this.showHexDump(isNaN(addr) ? 0 : addr, memDumpElementId)
    }

    updateMainDiv(docElem, urlParams) {
        if (urlParams.get("debugger") === '0') {
            return;
        }
        docElem.innerHTML =
            `
            <div id="debugger-panel" class="layout-region panel-hidden">
                <div class="uiPanelSub">
                    <span id="btn-debug-break" class="button">Break</span>
                    <span id="btn-debug-continue" class="button">Continue</span>
                    <span id="btn-debug-step-into" class="button">Step In</span>
                </div>
                <div id="debugger-disregs" class="uiPanelSub uiPanel2Col uiPanelBordered uiPanelGroup uiDisable uiSmallerText">
                    <code id="debugger-disasm" class="uiPanelColumn">
                    </code>
                    <div id="debugger-regs" class="uiPanelRegsColumn">
                        <table class="uiTable uiTableBordered uiTableFiles uiTableStripe">
                            <tbody>
                            <colgroup>
                                <col style="width: fit-content;">
                                <col style="width: fit-content;">
                                <col style="width: fit-content;">
                                <col style="width: fit-content;">
                            </colgroup>
                            <tr>
                                <td class="regs-label">PC</td>
                                <td id="regs-PC" class="regs-val"></td>
                                <td class="regs-label">SP</td>
                                <td id="regs-SP" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">A</td>
                                <td id="regs-A" class="regs-val"></td>
                                <td class="regs-label">A'</td>
                                <td id="regs-ADASH" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">F</td>
                                <td id="regs-FLAGS" class="regs-val"></td>
                                <td class="regs-label">F'</td>
                                <td id="regs-FLAGSDASH" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">HL</td>
                                <td id="regs-HL" class="regs-val"></td>
                                <td class="regs-label">HL'</td>
                                <td id="regs-HLDASH" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">DE</td>
                                <td id="regs-DE" class="regs-val"></td>
                                <td class="regs-label">DE'</td>
                                <td id="regs-DEDASH" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">BC</td>
                                <td id="regs-BC" class="regs-val"></td>
                                <td class="regs-label">BC'</td>
                                <td id="regs-BCDASH" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">IX</td>
                                <td id="regs-IX" class="regs-val"></td>
                                <td class="regs-label">IY</td>
                                <td id="regs-IY" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">I</td>
                                <td id="regs-I" class="regs-val"></td>
                                <td class="regs-label">R</td>
                                <td id="regs-R" class="regs-val"></td>
                            </tr>
                            <tr>
                                <td class="regs-label">IM</td>
                                <td id="regs-IM" class="regs-val"></td>
                                <td class="regs-label">IFF</td>
                                <td id="regs-IFF" class="regs-val"></td>
                            </tr>
                            </tbody>
                        </table>
                    </div>
                </div>
                <div id="debugger-memdump" class="uiPanelSub uiDisable uiSmallerText">
                    <div class="uiPanelSub uiPanel2Col">
                        <div class="uiPanelBordered uiPanelGroup uiPanelColumn">
                            <div class="uiPanelSubButtons">
                                <p class="uiInput-label">Memory Address</p>
                                <input id="input-debug-mem1-addr" class="uiInput-hex" value="0000"></input>
                            </div>
                            <code id="debugger-mem1" class="debugger-memory">
                            </code>
                        </div>
                        <div class="uiPanelBordered uiPanelGroup uiPanelColumn uiPanelCol2HideWidth">
                            <div class="uiPanelSubButtons">
                                <p class="uiInput-label">Memory Address</p>
                                <input id="input-debug-mem2-addr" class="uiInput-hex" value="0000"></input>
                            </div>
                            <code id="debugger-mem2" class="debugger-memory">
                            </code>
                        </div>
                    </div>
                </div>
                <div class="uiPanelSub">
                    <span id="btn-target-reset" class="button">Reset Z80</span>
                    <span id="btn-imager-clear" class="button">Clear target buffer</span>
                    <span id="btn-imager-write" class="button">Program from buffer</span>
                    <span id="btn-imager-write-and-exec" class="button">Program and execute</span>
                </div>
            </div>
            `;
        document.getElementById("btn-debug-break").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/debugbreak', (resp) => {
                this.debuggerUpdate(resp);
            });
        });
        document.getElementById("btn-debug-continue").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/debugcontinue');
            this.enableDebugger(false);
        });
        document.getElementById("btn-debug-step-into").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/debugstepin', (resp) => {
                this.debuggerUpdate(resp);
            });
        });
        document.getElementById("input-debug-mem1-addr").addEventListener('keydown', (event) => {
            this.validateHexInput(event, 'debugger-mem1');
        });
        document.getElementById("input-debug-mem1-addr").addEventListener('change', (event) => {
            this.onChangeHexInput(event, 'debugger-mem1');
        });
        document.getElementById("input-debug-mem2-addr").addEventListener('keydown', (event) => {
            this.validateHexInput(event, 'debugger-mem2');
        });
        document.getElementById("input-debug-mem2-addr").addEventListener('change', (event) => {
            this.onChangeHexInput(event, 'debugger-mem2');
        });
        document.getElementById("btn-target-reset").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/targetreset');
        });
        document.getElementById("btn-imager-clear").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/imagerclear');
        });
        document.getElementById("btn-imager-write").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/imagerwrite');
        });
        document.getElementById("btn-imager-write-and-exec").addEventListener('click', (event) => {
            ajaxGet('/api/targetcmd/imagerwriteandexec');
        });
    }
}
