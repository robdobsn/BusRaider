import socketio
from MockBusRaider import MockBusRaider
import flask
from flask import request, jsonify, render_template, send_file, send_from_directory
from os.path import isfile, join
import os
import json

fsFiles = "../../EspSw2/buildConfigs/BusRaider/FSImage"
sdFiles = "./SDTestFiles"
htmlIndex = fsFiles + "/index.html"
mockBusRaider = MockBusRaider()

app = flask.Flask(__name__)
app.config["DEBUG"] = True
app.config['SEND_FILE_MAX_AGE_DEFAULT'] = 0

curState = {
        "machineList": ["Serial Terminal", "Rob's Z80", "TRS80", "ZX Spectrum"],
        "machineCur": "TRS80",
        "clockHz": 1100000
    }

stFileInfo = {
    "rslt":"ok",
    "fsName":"sd",
    "fsBase":"/sd",
    "diskSize":1374476,
    "diskUsed":1004,
    "folder":"/sd/",
    "files":
        [
        ]
    }

@app.after_request
def add_header(r):
    """
    Add headers to both force latest IE rendering engine or Chrome Frame,
    and also to cache the rendered page for 10 minutes.
    """
    r.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    r.headers["Pragma"] = "no-cache"
    r.headers["Expires"] = "0"
    r.headers['Cache-Control'] = 'public, max-age=0'
    return r

@app.route('/', methods=['GET'])
def apiHome():
    return send_file(htmlIndex)

@app.route('/querystatus', methods=['GET'])
def apiQueryStatus():
    print('querystatus')
    return jsonify(curState)

@app.route('/files/sd/<path:path>', methods=['GET'])
def staticSd(path):
    return(send_from_directory(sdFiles, path))

@app.route('/files/spiffs/<path:path>', methods=['GET'])
def staticSpiffs(path):
    print("Sending file from spiffs", path)
    return(send_from_directory(fsFiles, path, cache_timeout=-1))

@app.route('/filelist/sd', methods=['GET'])
def filelistSd():
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    print(baseFolder)
    testFilePath = os.path.join(baseFolder, sdFiles)
    stFileInfo["files"] = [{"name":f,"size":1234} for f in os.listdir(testFilePath) if isfile(join(testFilePath, f))]
    return jsonify(stFileInfo)

@app.route('/filelist/spiffs/', methods=['GET'])
def filelistSpiffs():
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    print(baseFolder)
    testFilePath = os.path.join(baseFolder, fsFiles)
    stFileInfo["files"] = [{"name":f,"size":os.stat(join(testFilePath, f)).st_size} for f in os.listdir(testFilePath) if isfile(join(testFilePath, f))]
    return jsonify(stFileInfo)

@app.route('/uploadtofileman', methods=['POST'])
def apiUploadToFileMan():
    print('uploadToFileMan', request.files['file'])
    f = request.files['file']
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    f.save(os.path.join(baseFolder, fsFiles, f.filename))
    return jsonify({"rslt":"ok"})

@app.route('/setmcjson', methods=['POST'])
def apiSetMcJson():
    print('setMcJson', request.get_json())
    return jsonify({"rslt":"ok"})

@app.route('/targetcmd/getclockhz', methods=['GET'])
def apiGetClockHz():
    print('getClockHz')
    return jsonify({"rslt":"ok", "clockHz":curState['clockHz']})

@app.route('/targetcmd/setclockhz', methods=['GET','POST'])
def apiSetClockHz():
    print('setClockHz', request.get_json())
    curState['clockHz'] = json.parse(request.get_json())["clockHz"]
    return jsonify({"rslt":"ok"})

@app.route('/targetcmd/<path:path>', methods=['GET'])
def apiTargetCmd(path):
    print('targetCmd', path)
    return jsonify({"rslt":"ok"})

if __name__ == "__main__":
    app.run()

