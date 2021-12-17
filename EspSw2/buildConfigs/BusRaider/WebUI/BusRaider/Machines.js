
    class Machines {
        constructor() {
            this.state = {
                machineConfigs: [],
                machineConfigCurIdx: 0,
                machineList: [],
                machinePopupIdx: 0,
                fsDefault: "local"
            }
            this.scaderName = "Machines";
            this.friendlyName = "Machines";
            this.configObj = {};
            this.objGlobalStr = "";
            this.showTerminalCB = null;
            this.showDebuggerCB = null;
        }

        setButtons(showTerminalCB, showDebuggerCB) {
            this.showTerminalCB = showTerminalCB;
            this.showDebuggerCB = showDebuggerCB;
        }

        postInit() {
            // Populate machine list
            ajaxGet("/api/querystatus", (resp) => {
                this.populateMcList(resp);
                // Select
                this.setupMachineDropDownList();
                // Insert tabs when tab pressed in machine popup
                this.enableTabInsertion('popup-machine-text');
            });
        }

        getId() {
            return this.scaderName;
        }

        defaultConfig() {
            return {
                enable: false
            }
        }

        updateUI() {
            // Get or create the documentElement
            const mainDocElem = commonGetOrCreateDocElem(this.scaderName);

            // Generate the div for the main UI
            const mainDiv = document.createElement("div");
            mainDiv.id = this.scaderName + "-main";
            mainDocElem.appendChild(mainDiv);

            // Populate the main div
            this.updateMainDiv(mainDocElem);
        }

        setupMachineDropDownList() {
            const selEl = document.getElementById("machine-select");
            if (selEl.selectedIndex < 0)
                selEl.selectedIndex = 0;
            // Remove any existing selected item div
            const selSelectedDivs = selEl.parentElement.getElementsByClassName("select-selected");
            if (selSelectedDivs.length > 0) {
                selSelectedDivs[0].remove();
            }
            // Remove any existing select-items
            const selItemsDivs = selEl.parentElement.getElementsByClassName("select-items");
            if (selItemsDivs.length > 0) {
                selItemsDivs[0].remove();
            }
            // Selected item div
            let selItemDiv = document.createElement("div");
            selItemDiv.setAttribute("class", "select-selected");
            selItemDiv.innerHTML = selEl.options[selEl.selectedIndex].innerHTML;
            selEl.parentNode.appendChild(selItemDiv);
            // Options div
            let optionsDiv = document.createElement("div");
            optionsDiv.setAttribute("class", "select-items select-hide");
            for (let j = 0; j < selEl.length; j++) {
                // Option div (for each option)
                let optionDiv = document.createElement("div");
                optionDiv.innerHTML = selEl.options[j].innerHTML;
                optionDiv.addEventListener("click", (event) => {
                    // Handle item click = select item
                    const target = event.target;
                    let selection = target.parentNode.parentNode.getElementsByTagName("select")[0];
                    let prevSibling = target.parentNode.previousSibling;
                    for (let k = 0; k < selection.length; k++) {
                        if (selection.options[k].innerHTML === target.innerHTML) {
                            selection.selectedIndex = k;
                            prevSibling.innerHTML = target.innerHTML;
                            let sameAsSel = target.parentNode.getElementsByClassName("same-as-selected");
                            for (let n = 0; n < sameAsSel.length; n++) {
                                sameAsSel[n].removeAttribute("class");
                            }
                            target.setAttribute("class", "same-as-selected");
                            break;
                        }
                    }
                    prevSibling.click();

                });
                optionsDiv.appendChild(optionDiv);
                // Last item only
                if (j === selEl.length - 1)
                    optionDiv.setAttribute("class", "select-last-option");
            }
            selEl.parentNode.appendChild(optionsDiv);
            selItemDiv.addEventListener("click", (event) => {
                // Select box clicked = close other select boxes & open/close current
                event.stopPropagation();
                const target = event.target;
                closeAllSelect(target);
                target.nextSibling.classList.toggle("select-hide");
                target.classList.toggle("select-arrow-active");
                if (target.nextSibling.classList.contains("select-hide")) {
                    if (!this.machinePopupClose(true)) {
                        const mcSel = document.getElementById("machine-select");
                        this.sendSelectMachine(mcSel.selectedIndex, mcSel.value);
                    }
                } else {
                    this.machinePopupClose(true);
                }
            });

            function closeAllSelect(elmnt) {
                // Close all selects
                let selArray = [];
                let selectedItems = document.getElementsByClassName("select-items");
                let selectsSelected = document.getElementsByClassName("select-selected");
                for (let i = 0; i < selectsSelected.length; i++) {
                    if (elmnt === selectsSelected[i]) {
                        selArray.push(i)
                    }
                    else {
                        selectsSelected[i].classList.remove("select-arrow-active");
                    }
                }
                for (let i = 0; i < selectedItems.length; i++) {
                    if (selArray.indexOf(i)) {
                        selectedItems[i].classList.add("select-hide");
                    }
                }
            }

            // Click outside the document - close all selects
            document.addEventListener("click", closeAllSelect);
        }

        sendSelectMachine(mcIdx, mcName) {
            let mcConfig = this.state.machineConfigs[mcIdx];
            mcConfig.name = mcName;
            let mcJson = JSON.stringify(mcConfig);
            let xhr = new XMLHttpRequest();
            xhr.open('POST', "/api/setmcjson", true);
            xhr.setRequestHeader("Content-Type", "application/json");
            xhr.send(mcJson);
            // Update details
            setTimeout(() => this.queryStatusUpdate(), 500);
        }

        enableTabInsertion(elementId) {
            let el = document.getElementById(elementId);
            el.onkeydown = (e) => {
                let handled = false;
                if (e.code === 9) {
                    let val = this.value;
                    let selStart = this.selectionStart;
                    let selEnd = this.selectionEnd;
                    this.value = val.substring(0, selStart) + '\t' + val.substring(selEnd);
                    this.selectionStart = this.selectionEnd = selStart + 1;
                    handled = true;
                }
                return !handled;
            }
            el.onkeyup = (e) => {
                // Validate json
                this.machineConfigValidateForm();
            }
        }    
        
        populateMcList(jsonResp) {
            let queryResult = JSON.parse(jsonResp);
            this.state.machineList = queryResult.status.machineList;
            if (this.state.machineList === undefined)
                this.state.machineList = [];
            // Clear list and fill
            let mcListSelect = document.getElementById("machine-select");
            if (mcListSelect && this.state.machineList) {
                for (let i = 0; i < mcListSelect.options.length; i++)
                    mcListSelect.remove(i);
                for (let i = 0; i < this.state.machineList.length; i++) {
                    let option = document.createElement('option');
                    option.text = this.state.machineList[i];
                    option.value = this.state.machineList[i];
                    mcListSelect.add(option);
                }
                // Set current machine
                mcListSelect.value = queryResult.status.machineCur;
            }
            // Re-setup select
            this.setupMachineDropDownList();
            // Setup machine config files for each machine
            this.state.machineConfigs = [];
            for (let i = 0; i < this.state.machineList.length; i++) {
                // setup config file
                let configContent = { name: this.state.machineList[i] }; 
                this.state.machineConfigs.push(configContent);
            }
            // Show frequency
            this.showFrequency(queryResult.status);
            // Start getting first machine's config
            this.machineConfigReq(true);
        }        

        queryStatusUpdate()
        {
            ajaxGet("/api/querystatus", (resp) => this.updateMachineInfo(resp));
        }

        updateMachineInfo(jsonResp)
        {
            let status = JSON.parse(jsonResp);
            // Show frequency
            this.showFrequency(status.status);
        }

        showFrequency(status)
        {
            try
            {
                let clockHz = status.clockHz;
                if ((clockHz > 0) && (clockHz < 1000000000))
                {
                    let clockDisp = document.getElementById('clock-freq-hz');
                    clockDisp.value = clockHz / 1000000;
                }
            }
            catch(e)
            {
            }
        }

        machineConfigReq(first)
        {
            let fileIdx = 0
            if (!first)
                fileIdx = this.state.machineConfigCurIdx + 1;
            if (fileIdx >= this.state.machineList.length)
                return;
            let mcName = this.state.machineList[fileIdx]
            this.state.machineConfigCurIdx = fileIdx;
            ajaxGet("/api/files/" + this.state.fsDefault + "/" + mcName + ".json", 
                    (resp) => {
                        this.machineConfigGot(resp, mcName);
                    }, 
                    () => {
                        this.machineConfigGetFailed();
                    });
        }

        machineConfigGot(jsonResp, mcName) {
            console.log(jsonResp);
            let fileIdx = this.state.machineConfigCurIdx;
            let fileName = this.state.machineList[fileIdx]
            try
            {
                let configContent = JSON.parse(jsonResp);
                configContent.name = mcName;
                this.state.machineConfigs[fileIdx] = configContent;
            }
            catch (e)
            {
                console.log("Failed to parse received JSON", jsonResp, e);
            }
            // Get next config if there is one
            this.machineConfigReq(false);
        }

        machineConfigGetFailed(jsonResp) {
            console.log("File not found");
            let fileIdx = this.state.machineConfigCurIdx;
            let fileName = this.state.machineList[fileIdx]
            let fileContent = { name: fileName, hw: [] };
            this.state.machineConfigs[fileIdx] = fileContent;
            // Get next config if there is one
            this.machineConfigReq(false);
        }

        machineConfigCancel(event) {
            event.preventDefault();
            this.machinePopupClose(false);
        }

        machineConfigOk() {
            let mcName = this.state.machineList[this.state.machinePopupIdx];
            // Get Json from form
            let formJson = document.getElementById('popup-machine-text').value;
            // Check for no changes
            if (formJson == JSON.stringify(this.state.machineConfigs[this.state.machinePopupIdx], null, 2))
            {
                this.machinePopupClose(false);
                return false;
            }
            if (!this.machineConfigValidateForm())
                return false;
            // Format Json and ensure machine name is right
            let unJson = JSON.parse(formJson);
            unJson["name"] = mcName;
            this.state.machineConfigBeingWritten = unJson;
            formJson = JSON.stringify(unJson, null, 2);
            // Upload file
            let formData = new FormData();
            let contentBlob = new Blob([formJson], {type: 'plain/text'});
            formData.append("file", contentBlob, mcName + ".json");
            let xmlhttp = new XMLHttpRequest();
            xmlhttp.onreadystatechange = () =>
            {
                if (xmlhttp.readyState === 4) {
                    if (xmlhttp.status === 200) {
                        console.log("Save response " + xmlhttp.responseText);
                        this.machinePopupClose(false);
                        // Update 
                        this.state.machineConfigs[this.state.machinePopupIdx] = this.state.machineConfigBeingWritten;
                        // Set machine
                        this.sendSelectMachine(this.state.machinePopupIdx, mcName);
                    }
                    else {
                        console.log("Save failed " + xmlhttp.responseText);
                        alert("Save config failed");
                    }
                }
            };
            xmlhttp.open('POST', "/api/fileupload");
            xmlhttp.send(formData);
            return true;
        }

        machinePopupToggle(event) {
            event.preventDefault();
            let pop = document.getElementById('popup-machine');
            if (pop.classList.contains('popup-hidden'))
                this.machinePopupShow();
            else
                this.machinePopupClose(true);
        }

        machinePopupShow() {
            let pop = document.getElementById('popup-machine');
            if (!pop.classList.contains('popup-hidden'))
                return;
            pop.classList.remove('popup-hidden');
            let selIdx = document.getElementById("machine-select").selectedIndex;
            this.state.machinePopupIdx = selIdx;
            let popText = document.getElementById('popup-machine-text');
            popText.value = JSON.stringify(this.state.machineConfigs[selIdx], null, 2)
            this.machineConfigValidateForm();
        }

        machinePopupClose(doSave) {
            let pop = document.getElementById('popup-machine');
            if (pop.classList.contains('popup-hidden'))
                return false;
            
            pop.classList.add('popup-hidden');
            if (doSave)
            {
                return this.machineConfigOk();
            }
            return false;
        }

        machineConfigValidateForm()
        {
            let valid = false;
            let errorText = ""
            let formJson = document.getElementById('popup-machine-text').value;
            try
            {
                let unJson = JSON.parse(formJson);
                valid = true;
            }
            catch (e)
            {
                errorText = e.text;
            }
            let confStatus = document.getElementById('popup-machine-status');
            if (valid)
            {
                confStatus.innerHTML = "Valid";
                confStatus.classList.add('data-valid')
                confStatus.classList.remove('data-invalid')
            }
            else
            {
                confStatus.innerHTML = "Invalid JSON " + errorText;
                confStatus.classList.add('data-invalid')
                confStatus.classList.remove('data-valid')
            }
            return valid;
        }

        clockHzSet() {
            try
            {
                let clockDisp = document.getElementById('clock-freq-hz');
                let clockHz = Math.floor(clockDisp.value * 1000000);
                if ((clockHz > 0) && (clockHz < 1000000000))
                {
                    const clockHzJson = {clockHz:clockHz};
                    ajaxPost('/api/targetcmd/clockhzset', JSON.stringify(clockHzJson));
                }
            }
            catch (e)
            {

            }        
        }

        updateMainDiv(docElem) {
            docElem.innerHTML = 
            `
            <div id="machine-select-area" class="layout-region">
                <div class="uiPanelSub">
                    <form class="machine-select-form">
                        <p>Select target machine</p>
                        <div class="compound-line">
                            <div class="compound-line-first select-style">
                                <select id="machine-select">
                                </select>
                            </div>
                            <div>
                                <a id="btn-mc-config-toggle" class="button btn-config tooltip">
                                    <svg class="icon-config">
                                        <use xlink:href="#icon-config"></use>
                                    </svg>
                                    <span class="tooltiptext">Configure Machine</span>
                                </a>
                            </div>
                        </div>
                        <!-- Machine config -->
                        <div id="popup-machine" class="popup-machine popup-hidden">
                            <textarea id='popup-machine-text'>{}</textarea>
                            <div>
                                <button id='popup-machine-ok' class="button btn-ok">Ok</button>
                                <button id='btn-mc-config-cancel' class="button btn-cancel">Cancel</button>
                                <label id="popup-machine-status"></label>
                            </div>
                        </div>
                    </form>
                </div>
                <div class="uiPanelSub">
                    <div class="clock-setting">
                        <label>Clock</label>
                        <input id="clock-freq-hz" type="number" max=50.0 min=0 value=1.0 step=0.1 class="clock-freq"">
                        <label>MHz</label>
                    </div>
                    <span id="btn-term-show" class="button radio">Show Terminal</span>
                    <span id="btn-debugger-show" class="button radio">Show Debugger</span>
                </div>
            </div>
            `;
            document.getElementById('btn-mc-config-toggle').addEventListener('click', (event) => this.machinePopupToggle(event));
            document.getElementById('popup-machine-ok').addEventListener('click', (event) => { event.preventDefault(); this.machineConfigOk(event); });
            document.getElementById('btn-mc-config-cancel').addEventListener('click', (event) => this.machineConfigCancel(event));
            document.getElementById('btn-term-show').addEventListener('click', (event) => { 
                if (this.showTerminalCB) 
                    this.showTerminalCB(event); 
            });
            document.getElementById('btn-debugger-show').addEventListener('click', (event) => { 
                if (this.showDebuggerCB) 
                    this.showDebuggerCB(event); 
            });
            document.getElementById('clock-freq-hz').addEventListener('change', (event) => this.clockHzSet(event));
        }
    }