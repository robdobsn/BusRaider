'use strict';

const express = require('express');
const fs = require('fs');
const expressStaticGzip = require("express-static-gzip");
const ws = require('ws');
const url = require('url')
const fileUpload = require('express-fileupload');
const Z80System = require('./Z80System');

let curSettings = {
    "BusRaider": {
    }
};

let curState = {
    "status":
    {
        "machineList": ["Serial Terminal", "Rob's Z80", "TRS80", "ZX Spectrum"],
        "machineCur": "TRS80",
        "clockHz": 1100000
    },
};

let stFileInfo = {
    "rslt": "ok",
    "fsName": "sd",
    "fsBase": "/sd",
    "diskSize": 1374476,
    "diskUsed": 1004,
    "folder": "/sd/",
    "files":
        [
        ]
};

const z80System = new Z80System();
z80System.setMachine(curState.status.machineCur);

const localFSFolder = "../../EspSw2/buildConfigs/BusRaider/FSImage/";
const sdFSFolder = "./SDFSImage/";

run().catch(err => console.log(err));

// Websocket server
const wsServer = new ws.Server({ noServer: true, path: '/ws' });
wsServer.on('connection', socket => {
    socket.on('message', message => console.log("ws " + message));
    console.log("wsServer connection open");

    // Cache info for screen mirroring
    let mirrorScreenCache = new Uint8Array();

    function screenUpdateOnTimer(screenCache) {
        // Start timer to update mirror screen
        let mirrorScreenTimer = setInterval(() => {
            // console.log('update mirror screen');
            z80System.updateMirrorScreen(socket, screenCache);
            console.log(`mirrorScreenCache ${screenCache}`);
        }, 1000);
    }
    screenUpdateOnTimer(mirrorScreenCache);

    socket.on('close', () => {
        console.log('ws close');
        clearInterval(mirrorScreenTimer);
    });
});

async function uploadFile(req, res) {
    try {
        if(!req.files) {
            res.send({
                status: false,
                message: 'No file uploaded'
            });
        } else {
            // Name of the input field
            let fileName = req.files.file;

            //Use the mv() method to place the file in upload directory (i.e. "uploads")
            fileName.mv(localFSFolder + fileName.name);

            console.log("File uploaded " + localFSFolder + fileName.name);

            //send response
            res.send({
                status: true,
                message: 'File is uploaded',
                data: {
                    name: fileName.name,
                    mimetype: fileName.mimetype,
                    size: fileName.size
                }
            });
        }
    } catch (err) {
        res.status(500).send(err);
    }
}

function getFilePath(fs, filename) {
    let fsFolder = localFSFolder;
    if (fs === "sd") {
        fsFolder = sdFSFolder;
    }
    let filePath = fsFolder + filename;
    return filePath;
}

async function run() {
    const app = express();
    const port = process.env.PORT || 3000;

    app.use(express.json());

    // enable files upload
    app.use(fileUpload({
        createParentPath: true
    }));

    app.post('/api/fileupload', async (req, res) => {
        console.log(`fileupload ${req.files}`)
        uploadFile(req, res);
    });

    app.get('/api/runfileontarget/:fs/:filename', async (req, res) => {
        console.log(`runfileontarget ${req.params.fs} ${req.params.filename}`)
        const filePath = getFilePath(req.params.fs, req.params.filename);
        z80System.exec(filePath);
        res.json({ "rslt": "ok" });
    });

    app.get('/api/getsettings/:settings?', async function (req, res) {
        res.json({ "nv": curSettings });
    });

    app.post('/api/postsettings', async function (req, res) {
        console.log("postsettings", req.body)
        if ("body" in req) {
            curSettings = req.body;
        }
        res.json({ "rslt": "ok" });
    });

    app.get('/api/reset', async function (req, res) {
        console.log("Reset ESP32");
        res.json({ "rslt": "ok" })
    });

    app.get('/api/querystatus', async function (req, res) {
        const stateResp = Object.assign(curState, { "rslt": "ok" });
        console.log('querystatus', req.params, `resp: ${JSON.stringify(stateResp)}`);
        res.json(stateResp);
    });

    app.get('/api/filelist/:fs?', async function (req, res) {
        stFileInfo.folder = req.params.fs || "/local/";
        stFileInfo.fsName = req.params.fs || "local";
        stFileInfo.fsBase = req.params.fs || "/local";
        let fsFolder = localFSFolder;
        if (req.params.fs === "sd") {
            fsFolder = sdFSFolder;
        }
        let fileList = [];
        for (let f of fs.readdirSync(fsFolder)) {
            fileList.push({ "name": f, "size": fs.statSync(fsFolder + f).size });
        }
        stFileInfo.files = fileList;
        console.log("filelist", req.params, `resp: ${JSON.stringify(stFileInfo)}`);
        res.json(stFileInfo);
    });

    app.post('/api/setmcjson', async function (req, res) {
        console.log("setmcjson", req.body)
        if ("clockHz" in req.body) {
            curState.status.clockHz = req.body.clockHz;
        }
        if ("name" in req.body) {
            curState.status.machineCur = req.body.name;
            z80System.setMachine(req.body.name);
        }
        res.json({ "rslt": "ok" })
    });

    app.post('/api/targetcmd/clockhzset', async function (req, res) {
        console.log("clockhzset", req.params, req.body)
        if ("clockHz" in req.body) {
            curState.status.clockHz = req.body.clockHz;
        }
        res.json({ "rslt": "ok" })
    });

    app.get('/api/targetcmd/mirrorscreenon', async function (req, res) {
        console.log(`mirrorscreenon ${req.params}`)
        res.json({ "rslt": "ok" })
    });

    app.use("/api/files/local", express.static(localFSFolder));
    app.use("/api/files/sd", express.static(sdFSFolder));

    app.use("/", expressStaticGzip(localFSFolder));

    // app.get('/api/files/:fs/:fname', async function (req, res) {
    //     let fsFolder = localFSFolder;
    //     if (req.params.fs === "sd") {
    //         fsFolder = sdFSFolder;
    //     }
    //     const fname = req.params.fname;
    //     const fpath = fsFolder + (fsFolder.endsWith('/') ? "" : "/") + fname;
    //     console.log(fpath);
    //     if (fs.existsSync(fpath)) {
    //         res.sendFile(fpath, { root: __dirname });
    //     } else {
    //         res.status(404).send("File not found");
    //     }
    // });

    //   app.get('/events', async function(req, res) {
    //     console.log('Got /events');
    //     res.set({
    //       'Cache-Control': 'no-cache',
    //       'Content-Type': 'text/event-stream',
    //       'Connection': 'keep-alive'
    //     });
    //     res.flushHeaders();

    //     // Tell the client to retry every 10 seconds if connectivity is lost
    //     res.write('retry: 10000\n\n');
    //     let count = 0;

    //     while (true) {
    //       await new Promise(resolve => setTimeout(resolve, 1000));

    //       console.log('Emit', ++count);
    //       // Emit an SSE that contains the current 'count' as a string
    //       res.write(`data: ${count}\n\n`);
    //     }
    //   });

    // app.use("/", express.static('./buildConfigs/Scader/FSImage/'))
    // const index = fs.readFileSync('./buildConfigs/Common/Scader/FSImage/index.html', 'utf8');
    // app.get('/', (req, res) => res.send(index));

    // `server` is a vanilla Node.js HTTP server, so use
    // the same ws upgrade process described here:
    // https://www.npmjs.com/package/ws#multiple-servers-sharing-a-single-https-server
    const server = app.listen(port, () => {
        console.log(`Listening at http://localhost:${port}`);
    });

    server.on('upgrade', (request, socket, head) => {
        const baseURL =  request.protocol + '://' + request.headers.host + '/';
        const pathName = new URL(request.url, baseURL).pathname;
        console.log('pathname ' + pathName)
        if (pathName === "/ws") {
            wsServer.handleUpgrade(request, socket, head, socket => {
                wsServer.emit('connection', socket, request);
            });
        }
    });
}
