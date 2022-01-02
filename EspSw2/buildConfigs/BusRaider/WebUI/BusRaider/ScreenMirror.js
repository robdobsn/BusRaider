
class ScreenMirror {
    constructor() {
        this.scaderName = "Mirror";
        this.friendlyName = "Mirror";
        this.configObj = {};
        this.objGlobalStr = "";
        this.canvasImage = null;
        this.canvasImageU32 = null;
    }

    postInit() {
        // Set terminal colours
        this.setAnsiColours();

        // Init screen mirror
        this.initScreenMirror();

        // Set spectrum colours
        this.setSpectrumColours();
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
        this.termColours = [
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

    setSpectrumColours() {
        const colours = [
            "000000", "0000d8", "d80000", "d800d8", "00d800", "00d8d8", "d8d800", "d8d8d8",
            "000000", "0000ff", "ff0000", "ff00ff", "00ff00", "00ffff", "ffff00", "ffffff"
        ];
        this.spectrumColours = [];
        for (let i = 0; i < colours.length; i++) {
            this.spectrumColours[i] = parseInt(colours[i] + "ff", 16);
        }
    }

    initScreenMirror() {
    }

    webSocketMessage(msgData) {
        // Check valid message format
        if ((msgData[0] === 0x00) && (msgData[1] === 0x01)) {
            // console.log("Screen mirroring message");
            this.updateMirrorFull(msgData);
        }
    }

    buf2hex(buffer) {
        return [...new Uint8Array(buffer)]
            .map(x => x.toString(16).padStart(2, '0'))
            .join('');
    }

    updateMirrorFull(binData) {
        // Check if this is a character terminal
        if (binData[6] === 0x00) {
            this.handleCharMappedScreen(binData);
        } else if (binData[6] === 0x01) {
            this.handlePixelMappedScreen(binData);
        }
    }

    handleCharMappedScreen(binData) {
        // Get screen text element
        const screenText = document.getElementById("screen-text");

        // Check visible
        if (screenText.style.display === "none") {
            screenText.style.display = "block";
            const screenCanvas = document.getElementById("screen-canvas");
            screenCanvas.style.display = "none";
        }

        // Width and height
        const mirrorWidth = (binData[2] << 8) + binData[3];
        const mirrorHeight = (binData[4] << 8) + binData[5];

        // Check for TRS80 font types
        const fontType = binData[7];
        if (fontType == 0x01) {
            // console.log("TRS80 font");
            screenText.classList.add("screen-text-trs80-l1-2x3");

            // Move to the start of data
            let chPos = 10;

            // Add the data
            let screenTextStr = "";
            for (let i = 0; i < mirrorHeight; i++) {
                let line = "";
                for (let j = 0; j < mirrorWidth; j++) {
                    line += String.fromCharCode(0xe000 + binData[chPos++]);
                }
                screenTextStr += line + "<br>";
            }
            screenText.innerHTML = screenTextStr;
        } else {
            screenText.classList.remove("screen-text-trs80-l1-2x3");
        }
    }

    handlePixelMappedScreen(binData) {
        // Get canvas element
        const screenCanvas = document.getElementById("screen-canvas");
        const ctx = screenCanvas.getContext('2d');

        // Check visible
        if (screenCanvas.style.display === "none") {
            screenCanvas.style.display = "block";
            const screenText = document.getElementById("screen-text");
            screenText.style.display = "none";
        }

        // Width and height
        const mirrorWidth = (binData[2] << 8) + binData[3];
        const mirrorHeight = (binData[4] << 8) + binData[5];

        // Create canvas image if required
        if ((this.canvasImage === null) || 
                    (this.canvasImage.width !== mirrorWidth) || 
                    (this.canvasImage.height !== mirrorHeight) ||
                    (screenCanvas.width !== mirrorWidth) ||
                    (screenCanvas.height !== mirrorHeight)) {
            screenCanvas.width = mirrorWidth;
            screenCanvas.height = mirrorHeight;
            this.canvasImage = ctx.createImageData(mirrorWidth, mirrorHeight);
            this.canvasImageU32 = new DataView(this.canvasImage.data.buffer);
        }

        // Check for Spectrum layout
        const layoutType = binData[7];
        if (layoutType == 0x01) {
            // Move to the start of data
            let chPos = 10;

            // Draw something on the image
            const imageData = this.canvasImageU32;

            for (let y = 0; y < mirrorHeight; y++) {
                const lineStart = ((y & 0xc0) + ((y & 0x07) << 3) + ((y & 0x38) >> 3)) << 8;
                const colourStart = 10 + 0x1800 + ((lineStart >> 11) << 5);
                let foreColour = 0;
                let backColour = 0;
                let bitMask = 0x80;
                for (let x = 0; x < mirrorWidth; x++) {
                    if (bitMask === 0x80) {
                        const colourByte = binData[colourStart + (x >> 3)];
                        foreColour = this.spectrumColours[(colourByte & 0x07) + (colourByte & 0x40 ? 8 : 0)];
                        backColour = this.spectrumColours[(colourByte & 0x38) >> 3];
                    }
                    const pixelIndex = lineStart + x;
                    imageData.setUint32(pixelIndex << 2, (binData[chPos] & bitMask) ? foreColour : backColour, false);
                    if (bitMask === 0x01) {
                        chPos++;
                        bitMask = 0x80;
                    } else {
                        bitMask >>= 1;
                    }
                }
            }
            ctx.putImageData(this.canvasImage, 0, 0);
        }
    }

    termShow(show) {
        // Show panel
        const screenPanel = document.getElementById("screen-panel");
        if (show) {
            screenPanel.classList.remove("panel-hidden");
            ajaxGet("/api/targetcmd/mirrorscreenon");
            // document.onkeydown = "termKeyDown(event)";
            screenPanel.focus();
        }
        else {
            screenPanel.classList.add("panel-hidden");
            ajaxGet("/api/targetcmd/mirrorscreenoff");
            // document.onkeydown = null;
        }

        // Update button
        const termButton = document.getElementById("btn-term-show");
        if (termButton) {
            if (show) {
                termButton.innerHTML = "Hide Screen";
                termButton.classList.add("radio-on");
            } else {
                termButton.innerHTML = "Show Screen";
                termButton.classList.remove("radio-on");
            }
        }
    }

    termShowClick(event) {
        // console.log(event);
        this.termShow(!event.srcElement.classList.contains("radio-on"));
    }

    getKeyModCodes(event) {
        let modCodes = 0;
        modCodes |= (event.getModifierState("Control") ? 1 : 0);
        modCodes |= (event.getModifierState("Shift") ? 2 : 0);
        modCodes |= (event.getModifierState("Alt") ? 4 : 0);
        return modCodes;
    }

    termKeyboardEvent(event, isdown) {
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
    }

    termKeyDown(event) {
        this.termKeyboardEvent(event, 1);
    }

    termKeyUp(event) {
        this.termKeyboardEvent(event, 0);
    }
    updateMainDiv(docElem, urlParams) {
        docElem.innerHTML =
            `
                <div id="screen-panel" tabindex="110" class="layout-region panel-hidden">
                    <div id="screen-sub-panel" class="uiPanelSub">
                        <div id="screen-text" class="screen-text" tabindex="0"></div>
                        <canvas id="screen-canvas" class="screen-canvas" tabindex="1" style="display:none;"></canvas>
                    </div>
                </div>
            `;
        document.getElementById('screen-text').addEventListener('keydown', (event) => {
            this.termKeyDown(event)
        }, true);
        document.getElementById('screen-text').addEventListener('keyup', (event) => {
            this.termKeyUp(event);
        }, true);
        document.getElementById('screen-canvas').addEventListener('keydown', (event) => {
            this.termKeyDown(event)
        }, true);
        document.getElementById('screen-canvas').addEventListener('keyup', (event) => {
            this.termKeyUp(event);
        }, true);
        if (urlParams.get("screen") === '1') {
            this.termShow(true);
            const windowHeading = document.getElementById('heading-area');
            windowHeading.innerHTML = "<h1>BusRaider Screen</h1>";
        }
    }
}