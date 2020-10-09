/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandFile
//
// Rob Dobson and Declan Shafi 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <CommandFile.h>
#include <Utils.h>
#include <FileSystem.h>
#include <RestAPIEndpointManager.h>

static const char *MODULE_PREFIX = "CommandFile";

// Debug
// #define DEBUG_COMMAND_FILE 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandFile::CommandFile(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _curState = API_STATE_IDLE;
}

CommandFile::~CommandFile()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandFile::setup()
{
    // Apply config
    applySetup();
}

void CommandFile::applySetup()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandFile::service()
{
    // Cycle goes IDLE <-> PROCESSING <-> SLEEPING 
    // IDLE - Nothing to process
    // SLEEPING - Waiting for current request to time out before sending next one
    // PROCESSING - Ready to send next request
    switch(_curState)
    {
        case API_STATE_IDLE:
        {
            break;
        }
        case API_STATE_SLEEPING:
        {
            if (Utils::isTimeout(millis(), _stateTimerMilis, _sleepTimeMilis))
            {
                _curState = API_STATE_PROCESSING;
            }
            break;
        }
        case API_STATE_PROCESSING:
        {
            //If no more reps left process next line
            if (_repsLeft == 0) {
                processAPILine();
            } else {
                exec();
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandFile::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Run a file
    endpointManager.addEndpoint("filerun", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&CommandFile::apiFileRun, this, std::placeholders::_1, std::placeholders::_2), 
                "Run a file");

    _pRestAPIEndpointManager = &endpointManager;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Run a file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandFile::apiFileRun(String &reqStr, String& respStr)
{
    // File
    String fileName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    if (extraPath.length() > 0)
        fileName += "/" + extraPath;
    fileName.replace("~", "/");

#ifdef DEBUG_COMMAND_FILE
    LOG_I(MODULE_PREFIX, "apiFileRun req %s filename %s", reqStr.c_str(), fileName.c_str());
#endif

	// Get file type from extension
	String fileExt = "";
	int dotPos = String(fileName).lastIndexOf('.');
	if (dotPos >= 0)
		fileExt = fileName.substring(dotPos);

	// Handle different extensions
    bool rslt = false;
	if (fileExt.equalsIgnoreCase(".raw"))
	{
        // Send command to robot controller to play the file
        char jsonPlayFile[200];
        snprintf(jsonPlayFile, sizeof(jsonPlayFile), R"("cmd":"playSound","fileName":"%s")", fileName.c_str());
        sysModSendCmdJSON("RobotController", jsonPlayFile);
        rslt = true;
	}
    else if (fileExt.equalsIgnoreCase(".api"))
	{
        // Run the .api file
        rslt = handleAPIFile(fileName);
	}
    else if (fileExt.equalsIgnoreCase(".py"))
	{
        // Send command to robot controller to play the file
        char jsonExecFile[200];
        snprintf(jsonExecFile, sizeof(jsonExecFile), R"("cmd":"pyrun","fileName":"%s")", fileName.c_str());
        sysModSendCmdJSON("uPy", jsonExecFile);
        rslt = true;
	}

    Utils::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start handling an API file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandFile::handleAPIFile(String& fileName)
{
    // Read contents
    _APICode = fileSystem.getFileContents("", fileName.c_str(), 
                MAX_API_FILE_LENGTH, true);

    // Start the processing of the file
    _curState = API_STATE_PROCESSING;
    _curLine = 0;
    _curPosition = 0;

#ifdef DEBUG_COMMAND_FILE
    LOG_I(MODULE_PREFIX, "handleAPIFile fileName %s", fileName.c_str());
    LOG_I(MODULE_PREFIX, "handleAPIFile _APICode %s", _APICode.c_str());
#endif

    // Process the first line
    return processAPILine();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process one line
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandFile::processAPILine() 
{
    // Clean slate with each line
    _curCommand = "";
    String curTimer = "";
    String curRep = "";

    // Which stage are we in
    enum Part {
        COMMAND,
        TIMER,
        REP
    };

    Part part = COMMAND;

    // Parse Line
    char c= 0;
    while (_curPosition < _APICode.length() && c != '\r' && c != '\n') {

        // Set char
        c = _APICode[_curPosition];
        // Update current position
        _curPosition++;

        // On space we switch to next stage
        if (c == ' ') {
            if (part == COMMAND) {
                part = TIMER;
            } else {
                part = REP;
            }
            continue;
        }

        // Add current char to appropriate variable
        if (part == COMMAND) {
            _curCommand.concat(c);
        } else if (part == TIMER) {
            curTimer.concat(c);
        } else {
            curRep.concat(c);
        }
    }

    // Set sleep time between each rep
    _sleepTimeMilis = curTimer.toInt();

    // Number of times to run - default is one
    if (curRep == "") {
        _repsLeft = 1;
    } else {
        _repsLeft = curRep.toInt();
        if (_repsLeft < 1) {
            _repsLeft = 0;
            LOG_E(MODULE_PREFIX, "processAPILine Need all reps > 0 where specified");
            return false;
        }
    }

    //Next line
    _curLine++;

    //Execute processed line
    return exec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execute current command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CommandFile::exec() {
    _repsLeft -= 1;

    // Send API request
    String s = "";
    if (_pRestAPIEndpointManager)
        _pRestAPIEndpointManager->handleApiRequest(_curCommand.c_str(), s);

    // Change API State
    if (_curPosition == _APICode.length() && _repsLeft == 0) {
        _curState = API_STATE_IDLE;
    } else {
        _curState = API_STATE_SLEEPING;
        _stateTimerMilis = millis();
    }

#ifdef DEBUG_COMMAND_FILE
    LOG_I(MODULE_PREFIX, "exec _curCommand %s", _curCommand.c_str());
    LOG_I(MODULE_PREFIX, "exec s %s", s.c_str());
#endif

    return true;
}