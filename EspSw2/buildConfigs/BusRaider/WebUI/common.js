////////////////////////////////////////////////////////////////////////////
// Common (common.js)
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// bodyIsLoaded
////////////////////////////////////////////////////////////////////////////

// Function called when body is loaded
function bodyIsLoaded() {
    // Setup common state
    window.appState = {
        configObj: {},
        spinner: new Spinner(),
        uiInSetupMode: false,
    };
    window.appState.elems = {};
    window.appState.msgHandlers = {};

    // Add the Screen Mirror
    const screenMirror = new ScreenMirror();
    window.appState.elems[screenMirror.getId()] = screenMirror;
    
    // Add the Debugger
    const z80Debugger = new Debugger();
    window.appState.elems[z80Debugger.getId()] = z80Debugger;
    
    // Add the Machines
    const machines = new Machines();
    window.appState.elems[machines.getId()] = machines;

    // Add the File System
    const fileSystem = new FileSystem();
    window.appState.elems[fileSystem.getId()] = fileSystem;

    // Wire up modules
    machines.setModules(screenMirror, z80Debugger);

    // Record module message handlers
    for (const [key, elem] of Object.entries(window.appState.elems)) {
        const msgHandlers = elem.getMsgHandlers();
        for (const msgHandler of msgHandlers) {
            if (msgHandler.msgName in this.appState.msgHandlers) {
                this.appState.msgHandlers[msgHandler.msgName].push(msgHandler.handler);
            } else {
                this.appState.msgHandlers[msgHandler.msgName] = [ msgHandler.handler ];
            }
        }
    }

    // Get settings and update UI
    getAppSettingsAndUpdateUI();

    // Open websocket
    webSocketOpen();
}

////////////////////////////////////////////////////////////////////////////
// Get and post the app settings from the server
////////////////////////////////////////////////////////////////////////////

// Get application settings and update UI
async function getAppSettingsAndUpdateUI() {
    const getSettingsResponse = await fetch("/api/getsettings/nv");
    if (getSettingsResponse.ok) {
        const settings = await getSettingsResponse.json();
        if ("nv" in settings) {
            // Extract non-volatile settings
            window.appState.configObj = settings.nv;

            // Common setup defaults
            commonSetupDefaults();

            // Update UI
            commonUIUpdate();

            // Post init
            for (const [key, elem] of Object.entries(window.appState.elems)) {
                elem.postInit();
            }

        } else {
            alert("getAppSettings settings missing nv section");
        }
    } else {
        alert("getAppSettings failed");
    }
}

// Post applicaton settings
function postAppSettings(okCallback, failCallback) {
    let jsonStr = JSON.stringify(window.appState.configObj);
    jsonStr = jsonStr.replace("\n", "\\n")
    console.log("postAppSettings " + jsonStr);
    ajaxPost("/api/postsettings", jsonStr, okCallback, failCallback);
}

////////////////////////////////////////////////////////////////////////////
// UI Update
////////////////////////////////////////////////////////////////////////////

function commonUIUpdate() {

    // Clear the UI
    document.getElementById("elements-area").innerHTML = "";

    // Get URL params
    const urlParams = new URLSearchParams(window.location.search);

    // Init element UI
    for (const [key, elem] of Object.entries(window.appState.elems)) {

        // Get or create the documentElement
        const mainDocElem = commonGetOrCreateDocElem(elem.getId());

        // Generate the div for the main UI
        const elemDiv = document.createElement("div");
        elemDiv.id = elem.getId() + "-main";
        mainDocElem.appendChild(elemDiv);

        // Populate the main div
        elem.updateMainDiv(elemDiv, urlParams);
    }
}

////////////////////////////////////////////////////////////////////////////
// Setup default values
////////////////////////////////////////////////////////////////////////////

function commonSetupDefaults() {
    // Initialize the elements
    for (const [key, elem] of Object.entries(window.appState.elems)) {
        // Config
        if (!(elem.scaderName in window.appState.configObj)) {
            window.appState.configObj[elem.scaderName] = elem.defaultConfig();
        }
        elem.configObj = window.appState.configObj[elem.scaderName];
        elem.objGlobalStr = `window.appState.elems['${elem.scaderName}']`;
    }

    // Ensure there is a title for the main screen
    if (!("ScaderCommon" in window.appState.configObj)) {
        window.appState.configObj.ScaderCommon = {
            name: "Scader"
        }
    }
}

////////////////////////////////////////////////////////////////////////////
// Save and restart
////////////////////////////////////////////////////////////////////////////

// Reset functions
function commonSaveAndRestart() {
    // Save settings
    postAppSettings(
        () => {
            console.log("Post settings ok");
            commonRestartOnly();
        },
        () => {
            console.log("Post settings failed");
            alert("Save settings failed")
        },
    );
}

function commonRestartOnly() {
    // Reset
    ajaxGet("/api/reset");
    window.appState.isUIInSetupMode = false;

    // Update UI after a time delay
    const controlButtons = document.getElementById("save-load-settings-buttons");
    window.appState.spinner.spin(controlButtons);
    setTimeout(() => {
        window.appState.uiInSetupMode = false;
        commonUIUpdate();
        window.appState.spinner.stop();
    }, 10000);
}

function commonRestoreDefaults() {
    // Restore defaults
    window.appState.configObj = {};
    commonSetupDefaults();
    commonSaveAndRestart();
}

////////////////////////////////////////////////////////////////////////////
// UI Generation
////////////////////////////////////////////////////////////////////////////

function commonGetOrCreateDocElem(elemName) {
    // Get existing
    let elemDocElem = document.getElementById(elemName);
    if (elemDocElem)
        return elemDocElem;
    const elementsArea = document.getElementById("elements-area");
    if (!elementsArea)
        return null;
    elemDocElem = document.createElement("div");
    elemDocElem.id = elemName;
    elementsArea.appendChild(elemDocElem);
    return elemDocElem;
}

function commonGenCheckbox(parentEl, elName, cbName, label, checked) {
    let elStr = `<div class='common-config'>`;
    elStr += `<input type="checkbox" ${checked ? "checked" : ""} class="common-checkbox" ` +
        `id="${elName}-${cbName}-checkbox" name="${elName}-${cbName}-checkbox" ` +
        `onclick="window.appState.elems['${elName}'].${cbName}ChangeCB(this);"'>`;
    elStr += `<label for="${elName}-enable-checkbox">${label}</label>`;
    elStr += `</div>`;
    parentEl.innerHTML += elStr;
}

////////////////////////////////////////////////////////////////////////////
// AJAX
////////////////////////////////////////////////////////////////////////////

// Basic AJAX GET
function ajaxGet(url, okCallback, failCallback, okParam) {
    let xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4) {
            if (xmlhttp.status == 200) {
                if ((okCallback !== undefined) && (typeof okCallback !== 'undefined'))
                    okCallback(xmlhttp.responseText, okParam);
            } else {
                if ((failCallback !== undefined) && (typeof failCallback !== 'undefined'))
                    failCallback(xmlhttp);
            }
        }
    }
    xmlhttp.open("GET", url, true);
    xmlhttp.send();
}

// Basic AJAX POST
function ajaxPost(url, jsonStrToPos, okCallback, failCallback, okParam) {
    const xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState === 4) {
            if (xmlhttp.status === 200) {
                if ((okCallback !== undefined) && (typeof okCallback !== 'undefined'))
                    okCallback(xmlhttp.responseText, okParam);
            } else {
                if ((failCallback !== undefined) && (typeof failCallback !== 'undefined'))
                    failCallback(xmlhttp);
            }
        }
    };
    xmlhttp.open("POST", url);
    xmlhttp.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
    xmlhttp.send(jsonStrToPos);
}

////////////////////////////////////////////////////////////////////////////
// WebSocket
////////////////////////////////////////////////////////////////////////////

function webSocketOpen() {
    // Open a web socket for screen mirroring
    window.appState.websocket = new WebSocket("ws://" + location.host + "/ws");
    window.appState.websocket.binaryType = 'arraybuffer';
    window.appState.websocket.onopen = () => {
        console.log("Web socket open");
    }
    window.appState.websocket.onclose = () => {
        console.log("Web socket closed");
    }
    window.appState.websocket.onmessage = (event) => {
        // Interpret websocket message
        const msgData = new Uint8Array(event.data);

        // Find end of JSON section
        const jsonStartPos = 2;
        const stringTerm = msgData.indexOf(0, jsonStartPos);
        if (stringTerm <= 0)
            return;

        // Extract JSON
        const jsonBinary = msgData.subarray(jsonStartPos, stringTerm);
        const jsonStr = new TextDecoder("ascii").decode(jsonBinary);
        let jsonData = null;
        try {
            jsonData = JSON.parse(jsonStr);
        } catch (error) {
            console.log(`ws onmessage invalid json ${jsonStr}`);
            return;
        }
        if (!jsonData)
            return;

        // Extract binary section if any
        const binaryData = msgData.subarray(stringTerm+1, stringTerm+1+jsonData.dataLen);

        // Send to message handler(s)
        if (jsonData.cmdName in this.appState.msgHandlers) {
            for (const msgHandler of this.appState.msgHandlers[jsonData.cmdName]) {
                msgHandler(jsonData, binaryData);
            }
        }


        // // console.log(`Web socket message ${event.data.byteLength}`);
        // // console.log(this.buf2hex(msgData.slice(0, 20)));
        // if (msgData.length < 2) {
        //     console.log("Web socket message too short");
        //     return;
        // }
        // // Offer to elems
        // for (const [key, elem] of Object.entries(window.appState.elems)) {
        //     if (elem.webSocketMessage(msgData))
        //         return;
        // }
    }
}

////////////////////////////////////////////////////////////////////////////
// Utils
////////////////////////////////////////////////////////////////////////////

function preventDefaults(e) {
    e.preventDefault();
    e.stopPropagation();
}

function highlight(e) {
    let regionId = e.currentTarget.id;
    let dropArea = document.getElementById(regionId);
    dropArea.classList.add('highlight');
}

function unhighlight(e) {
    let regionId = e.currentTarget.id;
    let dropArea = document.getElementById(regionId);
    dropArea.classList.remove('highlight');
}

function intToHex(val, digits, uc=true) {
    let st = "0000" + val.toString(16);
    if (uc)
        st = st.toUpperCase();
    return st.slice(st.length-digits);
}