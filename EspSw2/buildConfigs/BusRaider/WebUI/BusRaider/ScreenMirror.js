
class ScreenMirror {
    constructor() {
        this.state = {
            terminalLineData: [],
            terminalRows: 25,
            terminalCols: 80,
            termColours: []
        }

        this.scaderName = "Mirror";
        this.friendlyName = "Mirror";
        this.configObj = {};
        this.objGlobalStr = "";
    }

    postInit() {
        // Set terminal colours
        this.setAnsiColours();

        // Init screen mirror
        this.initScreenMirror();
    }

    getId() {
        return this.scaderName;
    }

    defaultConfig() {
        return {
            enable: false
        }
    }

    setAnsiColours() {
        this.state.termColours = [
            "000000", "800000", "008000", "808000", "000080", "800080", "008080", "c0c0c0",
            "808080", "ff0000", "00ff00", "ffff00", "0000ff", "ff00ff", "00ffff", "ffffff",
            "000000", "00005f", "000087", "0000af", "0000df", "0000ff", "005f00", "005f5f",
            "005f87", "005faf", "005fdf", "005fff", "008700", "00875f", "008787", "0087af",
            "0087df", "0087ff", "00af00", "00af5f", "00af87", "00afaf", "00afdf", "00afff",
            "00df00", "00df5f", "00df87", "00dfaf", "00dfdf", "00dfff", "00ff00", "00ff5f",
            "00ff87", "00ffaf", "00ffdf", "00ffff", "5f0000", "5f005f", "5f0087", "5f00af",
            "5f00df", "5f00ff", "5f5f00", "5f5f5f", "5f5f87", "5f5faf", "5f5fdf", "5f5fff",
            "5f8700", "5f875f", "5f8787", "5f87af", "5f87df", "5f87ff", "5faf00", "5faf5f",
            "5faf87", "5fafaf", "5fafdf", "5fafff", "5fdf00", "5fdf5f", "5fdf87", "5fdfaf",
            "5fdfdf", "5fdfff", "5fff00", "5fff5f", "5fff87", "5fffaf", "5fffdf", "5fffff",
            "870000", "87005f", "870087", "8700af", "8700df", "8700ff", "875f00", "875f5f",
            "875f87", "875faf", "875fdf", "875fff", "878700", "87875f", "878787", "8787af",
            "8787df", "8787ff", "87af00", "87af5f", "87af87", "87afaf", "87afdf", "87afff",
            "87df00", "87df5f", "87df87", "87dfaf", "87dfdf", "87dfff", "87ff00", "87ff5f",
            "87ff87", "87ffaf", "87ffdf", "87ffff", "af0000", "af005f", "af0087", "af00af",
            "af00df", "af00ff", "af5f00", "af5f5f", "af5f87", "af5faf", "af5fdf", "af5fff",
            "af8700", "af875f", "af8787", "af87af", "af87df", "af87ff", "afaf00", "afaf5f",
            "afaf87", "afafaf", "afafdf", "afafff", "afdf00", "afdf5f", "afdf87", "afdfaf",
            "afdfdf", "afdfff", "afff00", "afff5f", "afff87", "afffaf", "afffdf", "afffff",
            "df0000", "df005f", "df0087", "df00af", "df00df", "df00ff", "df5f00", "df5f5f",
            "df5f87", "df5faf", "df5fdf", "df5fff", "df8700", "df875f", "df8787", "df87af",
            "df87df", "df87ff", "dfaf00", "dfaf5f", "dfaf87", "dfafaf", "dfafdf", "dfafff",
            "dfdf00", "dfdf5f", "dfdf87", "dfdfaf", "dfdfdf", "dfdfff", "dfff00", "dfff5f",
            "dfff87", "dfffaf", "dfffdf", "dfffff", "ff0000", "ff005f", "ff0087", "ff00af",
            "ff00df", "ff00ff", "ff5f00", "ff5f5f", "ff5f87", "ff5faf", "ff5fdf", "ff5fff",
            "ff8700", "ff875f", "ff8787", "ff87af", "ff87df", "ff87ff", "ffaf00", "ffaf5f",
            "ffaf87", "ffafaf", "ffafdf", "ffafff", "ffdf00", "ffdf5f", "ffdf87", "ffdfaf",
            "ffdfdf", "ffdfff", "ffff00", "ffff5f", "ffff87", "ffffaf", "ffffdf", "ffffff",
            "080808", "121212", "1c1c1c", "262626", "303030", "3a3a3a", "444444", "4e4e4e",
            "585858", "606060", "666666", "767676", "808080", "8a8a8a", "949494", "9e9e9e",
            "a8a8a8", "b2b2b2", "bcbcbc", "c6c6c6", "d0d0d0", "dadada", "e4e4e4", "eeeeee"];
    }

    initScreenMirror() {

        // Open a web socket for screen mirroring
        this.state.screenMirrorWebSocket = new WebSocket("ws://" + location.host + "/ws", "screen");
        this.state.screenMirrorWebSocket.binaryType = 'arraybuffer';
        this.state.screenMirrorWebSocket.onopen = () => {
            console.log("Web socket open");
        }
        this.state.screenMirrorWebSocket.onclose = () => {
            console.log("Web socket closed");
        }
        this.state.screenMirrorWebSocket.onmessage = (event) => {
            console.log(`Web socket message ${event.data.byteLength}`);
            const msgData = new Uint8Array(event.data);
            console.log(this.buf2hex(msgData.slice(0, 20)));

            // Check valid message format
            if (msgData.length > 2) {
                if ((msgData[0] === 0x00) && (msgData[1] === 0x01)) {
                    console.log("Screen mirroring message");
                    this.updateMirrorFull(msgData);
                }
            }
        }
    }

    buf2hex(buffer) {
        return [...new Uint8Array(buffer)]
            .map(x => x.toString(16).padStart(2, '0'))
            .join('');
    }

    updateMirrorFull(binData) {
        console.log(`updateMirrorFull ${binData.length}`);
        let chPos = 2;
        console.log(binData[2], binData[3]);
        const mirrorWidth = (binData[chPos] << 8) + binData[chPos+1];
        const mirrorHeight = (binData[chPos+2] << 8) + binData[chPos+3];
        console.log(`Mirror size ${mirrorWidth}x${mirrorHeight}`);

        // Get terminal window
        let termText = document.getElementById("term-text");

        // Check if this is a character terminal
        const isCharTerminal = binData[chPos+4] == 0x00;
        if (isCharTerminal) {

            // Check for TRS80 font types
            const trs80Font = binData[chPos+5];
            if (trs80Font == 0x01) {
                console.log("TRS80 font");
                termText.classList.add("term-text-trs80-l1-2x3");

                // Move to the start of data
                chPos = 10;

                // Add the data
                let termTextStr = "";
                for (let i = 0; i < mirrorHeight; i++) {
                    let line = "";
                    for (let j = 0; j < mirrorWidth; j++) {
                        line += String.fromCharCode(0xe000 + binData[chPos++]);
                    }
                    termTextStr += line + "<br>";
                }
                termText.innerHTML = termTextStr;
            } else {
                termText.classList.remove("term-text-trs80-l1-2x3");
            }
        }


        // const SIZE_OF_MSG_CHAR_DATA = 6;
        // const SIZE_OF_BUF_CHAR_DATA = 4;
        // // console.log(event.data.byteLength);
        // const binData = new Uint8Array(event.data);
        // // Find C string terminator of JSON section
        // function isTerm(element, index, array) {
        //     return element === 0;
        // }
        // const strTermPos = binData.findIndex(isTerm);
        // if (strTermPos + 3 < binData.byteLength) {
        //     // Get terminal window
        //     let termText = document.getElementById("term-text");
        //     let anyChange = false;
        //     // Extract screen size
        //     let chPos = strTermPos + 1;
        //     this.state.terminalCols = binData[chPos++];
        //     this.state.terminalRows = binData[chPos++];
        //     console.log("Screen dimensions cols", this.state.terminalCols, "rows", this.state.terminalRows);
        //     if ((this.state.terminalLineData.length < this.state.terminalRows) ||
        //         (this.state.terminalLineData[0].byteLength / SIZE_OF_BUF_CHAR_DATA < this.state.terminalCols)) {
        //         // Start buffer (again) on screen dimensions init or change
        //         this.state.terminalLineData = [];
        //         termText.innerHTML = "";
        //         for (let i = 0; i < this.state.terminalRows; i++) {
        //             let newLine = new Uint8Array(this.state.terminalCols * SIZE_OF_BUF_CHAR_DATA);
        //             for (let j = 0; j < this.state.terminalCols; j += 4) {
        //                 newLine[j] = 0x20;
        //                 newLine[j + 1] = 15;
        //                 newLine[j + 2] = 0;
        //                 newLine[j + 3] = 0;
        //             }
        //             this.state.terminalLineData.push(newLine);
        //             let newPara = document.createElement("pre");
        //             termText.appendChild(newPara);
        //         }
        //         anyChange = true;
        //     }

        //     // Extract character blocks
        //     if (chPos + SIZE_OF_MSG_CHAR_DATA < binData.byteLength) {
        //         for (let i = 0; i < (binData.byteLength - strTermPos - 1) / SIZE_OF_MSG_CHAR_DATA; i++) {
        //             let col = binData[chPos];
        //             let row = binData[chPos + 1];
        //             // Update lines
        //             if ((row < this.state.terminalRows) && (col < this.state.terminalCols)) {
        //                 bufPos = col * SIZE_OF_BUF_CHAR_DATA;
        //                 chPos += 2;
        //                 for (let k = 0; k < SIZE_OF_BUF_CHAR_DATA; k++)
        //                     this.state.terminalLineData[row][bufPos++] = binData[chPos++];
        //                 anyChange = true;
        //             }
        //             else {
        //                 break;
        //             }
        //         }
        //     }

        //     // Write chars
        //     if (anyChange) {
        //         let termLines = termText.childNodes;
        //         for (let i = 0; i < Math.min(this.state.terminalRows, termLines.length); i++) {
        //             let lineStr = "";
        //             let lineData = this.state.terminalLineData[i]
        //             for (let j = 0; j < this.state.terminalCols; j++) {
        //                 const fore = lineData[j * SIZE_OF_BUF_CHAR_DATA + 1];
        //                 const back = lineData[j * SIZE_OF_BUF_CHAR_DATA + 2];
        //                 if ((fore != 15) || (back != 0))
        //                     lineStr += "<span style=\"color:#" + this.state.termColours[fore] + ";background-color:#" + this.state.termColours[back] + ";\">";
        //                 lineStr += String.fromCharCode(lineData[j * SIZE_OF_BUF_CHAR_DATA]);
        //                 if ((fore != 15) || (back != 0))
        //                     lineStr += "</span>";
        //             }
        //             termLines[i].innerHTML = lineStr;
        //         }
        //     }
        // }

        // let eventInfo = JSON.parse(event.data);
        // if (eventInfo && eventInfo.dataType === "key")
        //     this.state.term.showString(eventInfo.val);

    }

    termShowClick(event) {
        // console.log(event);
        event.srcElement.classList.toggle("radio-on");
        let termPanel = document.getElementById("term-panel");
        if (event.srcElement.classList.contains("radio-on")) {
            termPanel.classList.remove("panel-hidden");
            ajaxGet("/api/targetcmd/mirrorscreenon");
            document.onkeydown = "termKeyDown(event)";
            event.srcElement.innerHTML = "Hide Terminal";
            termPanel.focus();
        }
        else {
            termPanel.classList.add("panel-hidden");
            ajaxGet("/api/targetcmd/mirrorscreenoff");
            document.onkeydown = null;
            event.srcElement.innerHTML = "Show Terminal";
        }
    }
    getKeyModCodes(event) {
        let modCodes = 0;
        // if (event.ctrlKey)
        //     modCodes |= 1;
        // let jsKeyCode = event.charCode || event.keyCode;
        // if (event.shiftKey != (event.getModifierState("CapsLock") && ((jsKeyCode >= 65) && (jsKeyCode <= 90))))
        //     modCodes |= 2;
        // if (event.altKey)
        //     modCodes |= 4;
        modCodes |= (event.getModifierState("Control") ? 1 : 0);
        modCodes |= (event.getModifierState("Shift") ? 2 : 0);
        modCodes |= (event.getModifierState("Alt") ? 4 : 0);
        return modCodes;
    }
    // getUsbKeyCode(jsKeyCode) {
    //     const keyMap = {
    //         8: 
    //     let usbKeyCode = jsKeyCode;
    //     if ((jsKeyCode >= 65) && (jsKeyCode <= 90)) {
    //         usbKeyCode = jsKeyCode - 61;
    //     } else if ((jsKeyCode === 48)) {
    //         usbKeyCode = 0x27;
    //     } else if ((jsKeyCode >= 49) && (jsKeyCode <= 57)) {
    //         usbKeyCode = jsKeyCode - 49 + 0x1e;
    //     } else if ((jsKeyCode === 13)) {
    //         usbKeyCode = 0x28;
    //     } else if ((jsKeyCode === 27)) {
    //         usbKeyCode = 0x29;
    //     } else if ((jsKeyCode === 8)) {
    //         usbKeyCode = 0x2a;
    //     } else if ((jsKeyCode === 9)) {
    //         usbKeyCode = 0x2b;
    //     } else if ((jsKeyCode === 32)) {
    //         usbKeyCode = 0x2c;
    //     } else if ((jsKeyCode === 189)) {
    //         usbKeyCode = 0x2d;
    //     } else if ((jsKeyCode === 187)) {
    //         usbKeyCode = 0x2e;
    //     } else if ((jsKeyCode === 219)) {
    //         usbKeyCode = 0x2f;
    //     } else if ((jsKeyCode === 221)) {
    //         usbKeyCode = 0x30;
    //     } else if ((jsKeyCode === 220)) {
    //         usbKeyCode = 0x31;
    //     } else if ((jsKeyCode === 163)) {
    //         usbKeyCode = 0x32;
    //     } else if ((jsKeyCode === 186)) {
    //         usbKeyCode = 0x33;
    //     } else if ((jsKeyCode === 222)) {
    //         usbKeyCode = 0x34;
    //     } else if ((jsKeyCode === 192)) {
    //         usbKeyCode = 0x35;
    //     } else if ((jsKeyCode === 188)) {
    //         usbKeyCode = 0x36;
    //     } else if ((jsKeyCode === 190)) {
    //         usbKeyCode = 0x37;
    //     } else if ((jsKeyCode === 191)) {
    //         usbKeyCode = 0x38;
    //     } else if ((jsKeyCode >= 112) && (jsKeyCode <= 123)) {
    //         usbKeyCode = jsKeyCode - 112 + 0x3a;
    //     } else if ((jsKeyCode === 45)) {
    //         usbKeyCode = 0x49;
    //     } else if ((jsKeyCode === 36)) {
    //         usbKeyCode = 0x4a;
    //     } else if ((jsKeyCode === 33)) {
    //         usbKeyCode = 0x4b;
    //     } else if ((jsKeyCode === 46)) {
    //         usbKeyCode = 0x4c;
    //     } else if ((jsKeyCode === 35)) {
    //         usbKeyCode = 0x4d;
    //     } else if ((jsKeyCode === 34)) {
    //         usbKeyCode = 0x4e;
    //     } else if ((jsKeyCode === 39)) {
    //         usbKeyCode = 0x4f;
    //     } else if ((jsKeyCode === 37)) {
    //         usbKeyCode = 0x50;
    //     } else if ((jsKeyCode === 40)) {
    //         usbKeyCode = 0x51;
    //     } else if ((jsKeyCode === 38)) {
    //         usbKeyCode = 0x52;
    //     }
    //     return usbKeyCode;
    // }

    termKeyboardEvent(event, isdown) {
        // console.log(event);
        // if ((event.keyCode === 16) || (event.keyCode === 17) || (event.keyCode === 18) || (event.keyCode === 20)) {
        //     // Ctrl, shift or Alt
        //     return;
        // }
        let jsKeyCode = event.charCode || event.keyCode;  // Get the Unicode value
        let usbKeyCode = -1;
        if (event.code in jsToHidKeyMap) {
            usbKeyCode = jsToHidKeyMap[event.code];
        }
        let modCodes = this.getKeyModCodes(event);
        
        const cmdStr = "/api/keyboard/" + isdown.toString() + "/" + jsKeyCode.toString() + "/" + usbKeyCode.toString() + "/" + modCodes.toString();
        console.log(event.code + " --- " + cmdStr);
        if (usbKeyCode == -1) {
            return;
        }
        ajaxGet(cmdStr);
        event.preventDefault();
        //     if (event.ctrlKey) {
        //         event.preventDefault();
        //         jsKeyCode -= 64;
        //         let ch = String.fromCharCode(jsKeyCode);
        //         console.log(ch, "stt", jsKeyCode, event.ctrlKey, event.shiftKey);
        //         callAjax("/api/sendkey/" + jsKeyCode.toString());
        //         return;
        //     }
        // } else if (jsKeyCode == 38) {
        //     callAjax("/api/sendkey/");
        // if (((jsKeyCode >= 64) && (jsKeyCode <= 90)) && (event.ctrlKey))
        //     jsKeyCode -= 64;
        // else if (((jsKeyCode >= 65) && (jsKeyCode <= 90)) && (event.shiftKey == event.getModifierState("CapsLock")))
        //     jsKeyCode += 32;
    }

    termKeyDown(event) {
        this.termKeyboardEvent(event, 1);
    }

    termKeyUp(event) {
        this.termKeyboardEvent(event, 0);
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

    updateMainDiv(docElem) {
        docElem.innerHTML =
            `
                <div id="term-panel" tabindex="110" class="layout-region">
                    <div id="term-text-sub-panel" class="uiPanelSub">
                        <div id="term-text" class="term-text" tabindex="0"></div>
                    </div>
                </div>
            `;
        document.getElementById('term-text').addEventListener('keydown', (event) => {
            this.termKeyDown(event)
        }, true);
        document.getElementById('term-text').addEventListener('keyup', (event) => { 
            this.termKeyUp(event);
        }, true);
        // document.getElementById('term-panel').addEventListener('onkeydown', (event) => {
        //     this.termKeyDown(event)
        // });
        // document.getElementById('term-text-sub-panel').addEventListener('onkeydown', (event) => {
        //     this.termKeyDown(event) 
        // });
    }

    // // Create terminal emulator
    // this.state.term = new TerminalEmulator("term", true, true, 80, 25, false, 25);
    // this.state.term.setInputCallback(terminalEmulatorCharCallback);
    // let termArea = document.getElementById("terminal-area");
    // termArea.appendChild(this.state.term.html);
    // function terminalEmulatorCharCallback(ch)
    // {
    //     if (this.state.terminalKeysSocket)
    //     {
    //         let msg = { dataType: "key", val: ch };
    //         let jsonRaw = JSON.stringify(msg);
    //         this.state.terminalKeysSocket.send(jsonRaw);
    //         this.state.terminalKeysSocket.send("{\"dataType\":\"key\",\"val\":\"" + ch + "\"}");
    //     }
    //     // alert(ch + "==>" + ch.charCodeAt(0));
    // }
}