import flask
from flask import request, jsonify, render_template, send_file, send_from_directory
from os.path import isfile, join
import os

app = flask.Flask(__name__)
app.config["DEBUG"] = True
app.config['SEND_FILE_MAX_AGE_DEFAULT'] = 0

curState = {
        "status":
        {
            "machineList": ["Serial Terminal", "Rob's Z80", "TRS80", "ZX Spectrum"],
            "machineCur": "TRS80"
        }
        # ,
        # "files":
        # {
        #     "SPIFFS":
        #     {
        #         "rslt":"ok","fsName":"spiffs",pip"fsBase":"spiffs","diskSize":"123456","diskUsed":"123124","folder":"/",
        #         "files": [ 
        #             {"name":"testFile1.bin","size":"1234","content":""},
        #             {"name":"testFile2.bin","size":"2345","content":""},
        #             {"name":"Serial Terminal.json","size":"2345","content":""},

        #         ]
        #     }
        # }
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
            # {"name": "/test1.gcode", "size": 79},
            # {"name": "/pattern.param", "size": 200},
            # {"name": "/robot.json", "size": 47}
        ]

        }


@app.route('/', methods=['GET'])
def apiHome():
    return send_file('../GenResources/StdUI/index.html')

@app.route('/favicon.ico', methods=['GET'])
def apiFavicon():
    return send_file('../GenResources/StdUI/favicon.ico')

# @app.route('/<path:path>', methods=['GET'])
# def apiRootFolder(path):
#     print(path)
#     return send_file('../GenResources/StdUI/' + path)

@app.route('/querystatus', methods=['GET'])
def apiQueryStatus():
    print('querystatus')
    return jsonify(curState["status"])

# @app.route('/filelist/<path:path>', methods=['GET'])
# def apiFilelist(path):
#     print('filelist', path)
#     return jsonify(curState["files"][path])

@app.route('/files/sd/<path:path>', methods=['GET'])
def staticSd(path):
    return(send_from_directory("./testfiles/sd", path))

@app.route('/files/SPIFFS/<path:path>', methods=['GET'])
def staticSpiffs(path):
    print("Sending file from SPIFFS", path)
    return(send_from_directory("./testfiles/SPIFFS", path, cache_timeout=-1))

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

@app.route('/filelist/sd', methods=['GET'])
def filelistSd():
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    print(baseFolder)
    testFilePath = os.path.join(baseFolder, "testfiles/sd/")
    stFileInfo["files"] = [{"name":f,"size":1234} for f in os.listdir(testFilePath) if isfile(join(testFilePath, f))]
    return jsonify(stFileInfo)

@app.route('/filelist/SPIFFS', methods=['GET'])
def filelistSpiffs():
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    print(baseFolder)
    testFilePath = os.path.join(baseFolder, "testfiles/SPIFFS/")
    stFileInfo["files"] = [{"name":f,"size":1234} for f in os.listdir(testFilePath) if isfile(join(testFilePath, f))]
    return jsonify(stFileInfo)

@app.route('/uploadtofileman', methods=['POST'])
def apiUploadToFileMan():
    print('uploadToFileMan', request.files['file'])
    f = request.files['file']
    baseFolder = os.path.dirname(os.path.realpath(__file__))
    f.save(os.path.join(baseFolder, "testfiles/SPIFFS/", f.filename))
    return jsonify({"rslt":"ok"})

@app.route('/setmcjson', methods=['POST'])
def apiSetMcJson():
    print('setMcJson', request.get_json())
    return jsonify({"rslt":"ok"})

@app.route('/targetcmd/<path:path>', methods=['GET'])
def apiTargetCmd(path):
    print('targetCmd', path)
    return jsonify({"rslt":"ok"})

# @app.route('/api/', methods=['GET'])
# def api_all():
#     return jsonify(books)

app.run()

