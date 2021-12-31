
class FileSystem {
    constructor() {
        this.state = {
            uploadProgress: [],
            latestFileInfo: null,
            fsDefault: "local",
        }
        this.scaderName = "FileSystem";
        this.friendlyName = "File System";
        this.configObj = {};
        this.objGlobalStr = "";
    }

    postInit() {
        // Init drag and drop
        this.initDragAndDrop();

        // Request file list to display
        this.fileListUpdate();
    }

    getId() {
        return this.scaderName;
    }

    defaultConfig() {
        return {
            enable: false
        }
    }

    webSocketMessage(msgData) {
    }

    initDragAndDrop() {
        // Drag and drop
        let dropZones = document.getElementsByClassName("drop-zone");
        Array.from(dropZones).forEach((dropZone) => {

            // Prevent default drag behaviors
            ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, preventDefaults, false)
                document.body.addEventListener(eventName, preventDefaults, false)
            });

            // Highlight drop area when item is dragged over it
            ['dragenter', 'dragover'].forEach(eventName => {
                dropZone.addEventListener(eventName, highlight, false)
            });

            ['dragleave', 'drop'].forEach(eventName => {
                dropZone.addEventListener(eventName, unhighlight, false)
            });

            // Handle dropped files
            dropZone.addEventListener('drop', this.handleDrop, false);
        });
    }

    fsSelectClick(event) {
        // console.log(event);
        event.srcElement.classList.toggle("radio-on");
        if (event.srcElement.classList.contains("radio-on")) {
            ajaxGet("/api/targetcmd/defaultfs/sd");
            this.state.fsDefault = "sd";
            event.srcElement.innerHTML = "File System = SD";
        }
        else {
            ajaxGet("/api/targetcmd/defaultfs/local");
            this.state.fsDefault = "local";
            event.srcElement.innerHTML = "File System = local";
        }
    }

    fileListUpdate() {
        ajaxGet("/api/filelist/" + this.state.fsDefault + "/",
            (resp) => {
                this.populateFileGrid(resp);
            },
            () => {
                console.log("fileListUpdate - error getting file list");
            });
    }

    fileSizeSI(size) {
        let e = (Math.log(size) / Math.log(1024)) | 0;
        return +(size / Math.pow(1024, e)).toFixed(2) + ' ' + ('kMGTPEZY'[e - 1] || '') + 'B';
    }

    populateFileGrid(resp) {
        // Parse status received
        const fileListInfo = JSON.parse(resp);
        this.state.latestfileListInfo = fileListInfo;

        // Show info
        if (fileListInfo.diskSize) {
            let fileSystemInfo = document.getElementById("file-system-info");
            if (fileSystemInfo) {
                const fsInfoStr = "<b>Disk Size:</b> " + this.fileSizeSI(fileListInfo.diskSize) + ", <b>Free Bytes:</b> " + this.fileSizeSI(fileListInfo.diskSize - fileListInfo.diskUsed);
                fileSystemInfo.innerHTML = fsInfoStr;
            }
        }

        // Clear the table
        let tabFiles = document.getElementById("table_files");
        if (!tabFiles) {
            console.log("populateFileGrid - no table found");
            return;
        }
        let rowCount = tabFiles.rows.length;
        for (let i = rowCount - 1; i > 0; i--)
            tabFiles.deleteRow(i);

        // Add rows
        for (let i = 0; i < fileListInfo.files.length; i++) {
            let filename = fileListInfo.files[i].name;
            filename = filename.replace("/", "");
            // Check for JSON files
            let aux = filename.split('.');
            if (aux.length > 0) {
                let extension = aux[aux.length - 1].toLowerCase();
                if ((extension === "json") && (filename.toLowerCase() !== 'demo.json'))
                    continue;
            }
            let filesize = fileListInfo.files[i].size;
            let row = '<tr data-file="' + filename + '">';
            row += '<td class=file-actions">'
            row += `<a id="btn-file-delete-${i}" class="btn btn-fileaction tooltip">`
            row += '<svg class="icon-action"><use xlink:href="#ico-delete"></use></svg>'
            row += '<span class="tooltiptext">Delete file</span>'
            row += '</a>'
            row += `<a id="btn-file-send-${i}" class="btn btn-fileaction tooltip">`
            row += '<svg class="icon-action"><use xlink:href="#icon-send"></use></svg>'
            row += '<span class="tooltiptext">Send to target buffer</span>'
            row += '</a>'
            row += `<a id="btn-file-append-${i}" class="btn btn-fileaction tooltip">`
            row += '<svg class="icon-action"><use xlink:href="#icon-send-plus"></use></svg>'
            row += '<span class="tooltiptext">Append to target buffer</span>'
            row += '</a>'
            row += `<a id="btn-file-run-${i}" class="btn btn-fileaction tooltip">`
            row += '<svg class="icon-action"><use xlink:href="#ico-play"></use></svg>'
            row += '<span class="tooltiptext">Run file now</span>'
            row += '</a>'
            row += '</td>'
            row += '<td class="file-name">' + filename + '</td>';
            row += '<td class="file-size hidden-xs">' + this.fileSizeSI(filesize) + '</td>';
            row += '</tr>';
            let newRow = tabFiles.insertRow(tabFiles.rows.length);
            newRow.innerHTML = row;
            document.getElementById(`btn-file-delete-${i}`).addEventListener("click", (event) => { this.deleteFile(filename) });
            document.getElementById(`btn-file-send-${i}`).addEventListener("click", (event) => { this.sendFileToTargetBuffer(filename) });
            document.getElementById(`btn-file-append-${i}`).addEventListener("click", (event) => { this.appendFileToTargetBuffer(filename) });
            document.getElementById(`btn-file-run-${i}`).addEventListener("click", (event) => { this.runFileOnTarget(filename) });
        }
    }

    deleteFile(filename) {
        ajaxGet("/api/filedelete/" + this.state.fsDefault + "/" + filename);
        fileListUpdate();
    }

    sendFileToTargetBuffer(filename) {
        ajaxGet("/api/sendfiletotargetbuffer/" + this.state.fsDefault + "/" + filename);
    }

    appendFileToTargetBuffer(filename) {
        ajaxGet("/api/appendfiletotargetbuffer/" + this.state.fsDefault + "/" + filename);
    }

    runFileOnTarget(filename) {
        ajaxGet("/api/runfileontarget/" + this.state.fsDefault + "/" + filename);
    }

    handleDrop(e) {
        var dt = e.dataTransfer;
        var files = dt.files;
        let regionId = e.currentTarget.id;
        if (regionId === "file-manager")
            this.uploadToFileManager(files);
        else
            this.uploadAndRunFiles(files);
    }

    initializeProgress(numFiles, progressBarName) {
        this.state.uploadProgress = []
        let progressBar = document.getElementById(progressBarName);
        progressBar.value = 0;
        for (let i = numFiles; i > 0; i--) {
            this.state.uploadProgress.push(0);
        }
    }

    updateProgress(fileNumber, percent, progressBarName) {
        let progressBar = document.getElementById(progressBarName);
        this.state.uploadProgress[fileNumber] = percent;
        let total = this.state.uploadProgress.reduce((tot, curr) => tot + curr, 0) /
            this.state.uploadProgress.length;
        // console.debug('update', fileNumber, percent, total);
        progressBar.value = total;
    }

    uploadToFileManager(files) {
        const fileArray = Array.from(files);
        this.initializeProgress(fileArray.length, 'progress-bar-file-manager');
        fileArray.forEach((el, i) => this.uploadOnlyFile(el, i));
        document.getElementById("fileElemUpload").value = ""
    }

    uploadOnlyFile(file, i) {
        this.uploadFileSpecUrl("/api/fileupload", 'progress-bar-file-manager', file, i)
    }

    uploadAndRunFiles(files) {
        const fileArray = Array.from(files);
        if (fileArray.length >= 1) {
            this.initializeProgress(1, 'progress-bar-run');
            this.uploadAndRunFile(fileArray[0], 0);
        }
        document.getElementById("fileElemRun").value = ""
    }

    uploadAndRunFile(file, i) {
        this.sendCmdToTarget("api/imagerClear");
        this.uploadFileSpecUrl("/api/uploadandrun", 'progress-bar-run', file, i);
    }

    uploadFileSpecUrl(url, progressBarName, file, i) {
        const xhr = new XMLHttpRequest();
        const formData = new FormData();
        xhr.open('POST', url, true);
        xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest');

        // Update progress (can be used to show progress indicator)
        xhr.upload.addEventListener("progress", (e) => {
            this.updateProgress(i, (e.loaded * 100.0 / e.total) || 100, progressBarName);
            // console.log("Progress" + e.loaded + ", " + e.total);
        });

        xhr.addEventListener('readystatechange', (e) => {
            if (xhr.readyState === 4 && xhr.status === 200) {
                this.updateProgress(i, 100, progressBarName)
                this.fileListUpdate();
            } else if (xhr.readyState === 4 && xhr.status != 200) {
                // Error
            }
        });

        formData.append('file', file);
        xhr.send(formData);
    }

    updateMainDiv(docElem, urlParams) {
        if (urlParams.get("filesys") === '0') {
            return;
        }
        docElem.innerHTML =
            `
            <!--
            <div id="upload-only" class="layout-region drop-zone">
                <form class="file-upload-form">
                    <p>Drop files to upload to the Bus-Raider's buffer</p>
                    <input type="file" id="fileElem">
                    <label class="button" for="fileElem">Select a file to upload</label>
                </form>
                <progress id="progress-bar-buffer" max=100 value=0></progress>

                <span id="imagerWrite" class="button">Send to target</span>

                <span id="ioclrTarget" class="button">Clear all RC2014 IO</span> 
            </div>
            -->
            <div id="file-manager" class="layout-region drop-zone">
                <form class="file-upload-form">
                    <div class=uiPanelSub>
                        <p>Drop files to upload to ESP32 file system</p>
                        <input type="file" id="fileElemUpload">
                        <label class="button" for="fileElemUpload">Select a file to upload</label>
                        <!-- <span class="button radio" onclick="fsSelectClick(event)">File System = SPIFFS</span> -->
                    </div>
                    <progress id="progress-bar-file-manager" max=100 value=0></progress>
                    <div id="file-system-info" class="uiPanelSub"><span></span></div>
                    <table class="uiTable uiTableBordered uiTableFiles uiTableStripe" id="table_files">
                        <thead>
                            <tr>
                                <th><a href="#" data-attribute="action">Actions</a>
                                </th>
                                <th><a href="#" data-attribute="filename">File Name</a>
                                </th>
                                <th><a href="#" data-attribute="size">Size</a>
                                </th>
                                </th>
                            </tr>
                        </thead>
                        <tbody>
                        </tbody>
                    </table>
                </form>
            </div>
            <div id="upload-and-run" class="layout-region drop-zone">
                <div class="uiPanelSub">
                    <form class="file-upload-form">
                        <p>Drop files to immediately run on the target</p>
                        <label id="fileElemRun" class="button" for="fileElemRun">Select a file to immediately run</label>
                        <input type="file" id="fileElemRun">
                    </form>
                </div>
                <progress id="progress-bar-run" max=100 value=0></progress>
            </div>
            `;
        document.getElementById("fileElemRun").addEventListener("change", (event) => { this.uploadAndRunFiles(event.target.files); });
        // document.getElementById("fileElem").addEventListener("change", (files) => { this.uploadToFileManager(files); });
        // document.getElementById("imagerWrite").addEventListener("change", (files) => { this.sendCmdToTarget('imagerWrite'); });
        // document.getElementById("ioclrTarget").addEventListener("change", (files) => { this.sendCmdToTarget('ioclrtarget'); });
        document.getElementById("fileElemUpload").addEventListener("change", (event) => { this.uploadToFileManager(event.target.files); });
    }
}