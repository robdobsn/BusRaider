
    class Debugger {
        constructor() {
            this.state = {
            }
            this.scaderName = "Debugger";
            this.friendlyName = "Debugger";
            this.configObj = {};                
        }

        init() {
            // Config
            if (!(this.scaderName in window.appState.configObj)) {
                window.appState.configObj[this.scaderName] = this.defaultConfig();
            }
            this.configObj = window.appState.configObj[this.scaderName]
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

        debuggerShowClick(event) {
            event.srcElement.classList.toggle("radio-on");
            let debuggerPanel = document.getElementById("debugger-panel");
            if (event.srcElement.classList.contains("radio-on"))
            {
                debuggerPanel.classList.remove("panel-hidden");
                event.srcElement.innerHTML = "Hide Debugger";
            }
            else
            {
                debuggerPanel.classList.add("panel-hidden");
                event.srcElement.innerHTML = "Show Debugger";
            }
        }

        debuggerFlagStr(flags) {
            const flagStr = "SZ H PNC"
            let fMask = 0x80;
            let rsltStr = ""
            for (let i = 0; i < 8; i++) {
                rsltStr += ((fMask & flags) != 0) ? flagStr[i] : " ";
                fMask = fMask >> 1;
            }
            return rsltStr;
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
        
        intToHex(val, digits, uc=true) {
            let st = "0000" + val.toString(16);
            if (uc)
                st = st.toUpperCase();
            return st.slice(st.length-digits);
        }
        
        hexDisplayUpdate(jsonRslt, elementId) {
            console.log(jsonRslt, elementId);
            const rslt = JSON.parse(jsonRslt);
            const memData = rslt.data;
            const memAddr = rslt.addr;
            const el = document.getElementById(elementId);
            let dumpHtml = "";
            let addr = parseInt(memAddr, 16);
            for (let i = 0; i < memData.length; i+=2) {
                if (i % 32 === 0) {
                    if (i !== 0)
                        dumpHtml += "\n";
                    dumpHtml += intToHex(addr, 4) + " ";                    
                    addr += 16;
                }
                dumpHtml += intToHex(memData.slice(i,i+2), 2) + " ";
            }
            el.innerHTML = dumpHtml;
        }
        
        showHexDump(addr, memDumpElementId) {
            callAjaxWithParam('/api/targetcmd/Rd/' + intToHex(addr, 4) + '/100', hexDisplayUpdate, memDumpElementId);
        }
        
        debuggerUpdate(rslt) {
            enableDebugger(true)
            console.log(rslt);
            // Regs
            const regsObj = JSON.parse(rslt);
            const regNames = ["PC","SP","BC","AF","HL","DE","IX","IY","AFDASH","BCDASH","HLDASH","DEDASH","I","R","IM","IFF"];
            regNames.forEach((reg) => {
                const el = document.getElementById("regs-" + reg);
                const regStr = regsObj[reg]
                el.innerHTML = typeof regStr === 'string' ? regStr.replace("0x","").toUpperCase() : regStr;
            });
            const f1 = document.getElementById("regs-FLAGS");
            f1.innerHTML = debuggerFlagStr(parseInt(regsObj["AF"],16) & 0xf0);
            const f1Dash = document.getElementById("regs-FLAGSDASH");
            f1Dash.innerHTML = debuggerFlagStr(parseInt(regsObj["AFDASH"],16) & 0xf0);
            // Disassembly
            const el = document.getElementById("debugger-disasm");
            const disasmLines = regsObj.disasm.split('%');
            let disasmText = "";
            for (let i = 0; i < disasmLines.length; i++) {
                lin = disasmLines[i];
                lin = lin.replace(/\~/g, '\t')
                lin = lin.replace(/\^/g, '\t')
                lin = lin.toUpperCase()
                disasmText += lin + '<BR>';
            }
            el.innerHTML = disasmText;
            showHexDumps();
        }

        updateMainDiv(docElem) {
            docElem.innerHTML = 
            `
            <div id="debugger-panel" class="layout-region panel-hidden">
                <div class="uiPanelSub">
                    <span class="button" onclick="callAjax('/api/targetcmd/debugBreak', debuggerUpdate);">Break</span>
                    <span class="button" onclick="sendCmdToTarget('debugContinue'); enableDebugger(false);">Continue</span>
                    <span class="button" onclick="callAjax('/api/targetcmd/debugStepIn', debuggerUpdate);">Step In</span>
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
                                <td class="regs-label">AF</td>
                                <td id="regs-AF" class="regs-val"></td>
                                <td class="regs-label">AF'</td>
                                <td id="regs-AFDASH" class="regs-val"></td>
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
                                <input id="debugger-mem1-addr" class="uiInput-hex" value="0000" onkeydown="return validateHexInput(event, 'debugger-mem1');" 
                                        oninput="onChangeHexInput(event, 'debugger-mem1');">
                                    </input>
                            </div>
                            <code id="debugger-mem1" class="debugger-memory">
                            </code>
                        </div>
                        <div class="uiPanelBordered uiPanelGroup uiPanelColumn uiPanelCol2HideWidth">
                            <div class="uiPanelSubButtons">
                                <p class="uiInput-label">Memory Address</p>
                                <input id="debugger-mem2-addr" class="uiInput-hex" value="0000" onkeydown="return validateHexInput(event, 'debugger-mem2');" 
                                        oninput="onChangeHexInput(event, 'debugger-mem2');"></input>
                            </div>
                            <code id="debugger-mem2" class="debugger-memory">
                            </code>
                        </div>
                    </div>
                </div>
                <div class="uiPanelSub">
                    <span class="button" onclick="sendCmdToTarget('targetReset')">Reset Z80</span>
                    <span class="button" onclick="sendCmdToTarget('imagerClear')">Clear target buffer</span>
                    <span class="button" onclick="sendCmdToTarget('imagerWrite')">Program from buffer</span>
                    <span class="button" onclick="sendCmdToTarget('imagerWriteAndExec')">Program and execute</span>
                </div>
            </div>
            `;
        }

    }
