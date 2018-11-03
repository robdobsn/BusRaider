// CommandSerial
// Rob Dobson 2018

#include "CommandSerial.h"

void CommandSerial::setup(ConfigBase& config)
{
    // Get config
    ConfigBase csConfig(config.getString("commandSerial", "").c_str());
    Log.notice("CommandSerial: config %s\n", csConfig.getConfigData());

    // Get serial port
    _serialPortNum = csConfig.getLong("portNum", -1);
    _baudRate = csConfig.getLong("baudRate", 115200);

    // Setup port
    if (_serialPortNum == -1)
        return;

    // Setup serial port
    _pSerial = new HardwareSerial(_serialPortNum);
    if (_pSerial)
    {
        if (_serialPortNum == 1)
        {
            _pSerial->begin(_baudRate, SERIAL_8N1, 16, 17, false);
            Log.notice("CommandSerial: portNum %d, baudRate %d, rxPin %d, txPin %d\n",
                            _serialPortNum, _baudRate, 16, 17);
        }
        else if (_serialPortNum == 2)
        {
            _pSerial->begin(_baudRate, SERIAL_8N1, 26, 25, false);
            Log.notice("CommandSerial: portNum %d, baudRate %d, rxPin %d, txPin %d\n",
                            _serialPortNum, _baudRate, 26, 25);
        }
        else
        {
            _pSerial->begin(_baudRate);
            Log.notice("CommandSerial: portNum %d, baudRate %d, rxPin %d, txPin %d\n",
                            _serialPortNum, _baudRate, 3, 1);
        }
    }
    else
    {
        Log.notice("CommandSerial: failed portNum %d, baudRate %d\n",
                        _serialPortNum, _baudRate);
    }
}

// Set callback on frame received
void CommandSerial::setCallbackOnRxFrame(CommandSerialFrameRxFnType frameRxCallback)
{
    _frameRxCallback = frameRxCallback;
}

// Log message
void CommandSerial::logMessage(String& msg)
{
    // Don't use ArduinoLog logging here as CmdSerial can be used by NetLog and this becomes circular
    // Serial.printf("CommandSerial: send frame %s\n", msg.c_str());

    // Log over HDLC
    String frame = "{\"cmdName\":\"logMsg\",\"msg\":\"" + msg + "}\0";
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

// Event message
void CommandSerial::eventMessage(String& msgJson)
{
    // Serial.printf("CommandSerial: event Msg %s\n", msgJson.c_str());

    String frame = "{\"cmdName\":\"eventMsg\"," + msgJson + "}\0";
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

// Event message
void CommandSerial::responseMessage(String& msgJson)
{
    // Serial.printf("CommandSerial: response Msg %s\n", msgJson.c_str());

    String frame;
    if (msgJson.startsWith("{"))
    {
        frame = "{\"cmdName\":\"respMsg\",\"msg\":" + msgJson + "}\0";
    }
    else
    {
        frame = "{\"cmdName\":\"respMsg\"," + msgJson + "}\0";
    }
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

// Upload in progress
bool CommandSerial::uploadInProgress()
{
    return _uploadFromAPIInProgress || _uploadFromFSInProgress;
}

// Service 
void CommandSerial::service()
{
    // Check if port enabled
    if (!_pSerial)
        return;

    // See if characters to be processed
    for (int rxCtr = 0; rxCtr < 1000; rxCtr++)
    {
        int rxCh = _pSerial->read();
        if (rxCh < 0)
            break;

        // Handle char
        _miniHDLC.handleChar(rxCh);
    }

    // Check if there's a file system upload in progress
    if (_uploadFromFSInProgress)
    {
        // See if ready to handle the next chunk
        if (Utils::isTimeout(millis(), _uploadLastBlockMs, DEFAULT_BETWEEN_BLOCKS_MS))
        {
            // Get next chunk of data
            String filename;
            int fileLen = 0; 
            int chunkPos = 0;
            int chunkLen = 0;
            bool finalChunk = false;
            uint8_t* pData = _fileManager.chunkFileNext(filename, fileLen, chunkPos, chunkLen, finalChunk);
            if (pData && (chunkLen != 0))
            {
                // Handle the chunk
                uploadCommonBlockHandler(_uploadFileType.c_str(), filename, fileLen, chunkPos, pData, chunkLen, finalChunk); 
            }
            else
            {
                // Tidy up if finished
                if (!finalChunk)
                    Log.warning("CommandSerial: upload 0 len but not final\n");
                _uploadFromFSInProgress = false;
            }
        }
    }

    // Check for timeouts on any upload
    if (uploadInProgress())
    {
        // Check for timeouts
        if (Utils::isTimeout(millis(), _uploadLastBlockMs, MAX_BETWEEN_BLOCKS_MS))
        {
            _uploadFromFSInProgress = false;
            _uploadFromAPIInProgress = false;
            Log.notice("CommandSerial: Upload block timed out\n");
        }
        if (Utils::isTimeout(millis(), _uploadStartMs, MAX_UPLOAD_MS))
        {
            _uploadFromFSInProgress = false;
            _uploadFromAPIInProgress = false;
            Log.notice("CommandSerial: Upload timed out\n");
        }
    }
}

void CommandSerial::sendFileStartRecord(const char* fileType, String& filename, int fileLength)
{
    String frame = "{\"cmdName\":\"ufStart\",\"fileType\":\"" + String(fileType) + "\",\"fileName\":\"" + filename + "\",\"fileLen\":" + String(fileLength) + "}";
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

void CommandSerial::sendFileBlock(size_t index, uint8_t *data, size_t len)
{
    String header = "{\"cmdName\":\"ufBlock\",\"index\":" + String(index) + ",\"len\":" + String(len) + "}";
    int headerLen = header.length();
    uint8_t* pFrameBuf = new uint8_t[headerLen + len + 1];
    memcpy(pFrameBuf, header.c_str(), headerLen);
    pFrameBuf[headerLen] = 0;
    memcpy(pFrameBuf + headerLen + 1, data, len);
    _miniHDLC.sendFrame(pFrameBuf, headerLen + len + 1);
    delete [] pFrameBuf;
}

void CommandSerial::sendFileEndRecord(int blockCount)
{
    String frame = "{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + String(blockCount) + "\"}";
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

void CommandSerial::sendTargetCommand(const String& targetCmd)
{
    // Serial.printf("CommandSerial: targetCommand Msg %s\n", targetCmd.c_str());

    String frame = "{\"cmdName\":\"" + targetCmd + "\"}\0";
    _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
}

void CommandSerial::sendTargetData(const String& cmdName, const uint8_t* pData, int len, int index)
{
    String header = "{\"cmdName\":\"" + cmdName + "\",\"index\":" + String(index) + ",\"len\":" + String(len) + "}";
    int headerLen = header.length();
    uint8_t* pFrameBuf = new uint8_t[headerLen + len + 1];
    memcpy(pFrameBuf, header.c_str(), headerLen);
    pFrameBuf[headerLen] = 0;
    memcpy(pFrameBuf + headerLen + 1, pData, len);
    _miniHDLC.sendFrame(pFrameBuf, headerLen + len + 1);
    delete [] pFrameBuf;
}

void CommandSerial::uploadCommonBlockHandler(const char* fileType, String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("CommandSerial: %s, total %d, idx %d, len %d, final %d, fs %s api %s\n", 
                filename.c_str(), fileLength, index, len, finalBlock, 
                (_uploadFromFSInProgress ? "yes" : "no"), (_uploadFromAPIInProgress ? "yes" : "no"));

    // Check if first block in an upload
    if (_blockCount == 0)
    {
        _uploadFileType = fileType;
        sendFileStartRecord(fileType, filename, fileLength);
        Log.verbose("CommandSerial: new upload started\n");
    }

    // Send the block
    sendFileBlock(index, data, len);
    Log.verbose("CommandSerial: block sent\n");
    _blockCount++;

    // For timeouts        
    _uploadLastBlockMs = millis();

    // Check if that was the final block
    if (finalBlock)
    {
        sendFileEndRecord(_blockCount);
        Log.verbose("CommandSerial: file end sent\n");
        if (_uploadTargetCommandWhenComplete.length() != 0)
        {
            sendTargetCommand(_uploadTargetCommandWhenComplete);
            Log.verbose("CommandSerial: post-upload target command sent %s\n", _uploadTargetCommandWhenComplete.c_str());
        }
        _uploadTargetCommandWhenComplete = "";
        _uploadFromFSInProgress = false;
        _uploadFromAPIInProgress = false;
    }
}

// Upload from API
void CommandSerial::uploadAPIBlockHandler(const char* fileType, String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool finalBlock)
{
    // Check there isn't an upload in progress from FS
    if (_uploadFromFSInProgress)
    {
        Log.notice("CommandSerial: uploadAPIBlockHandler upload already in progress\n");
        return;
    }

    // Upload now in progress
    _uploadFromAPIInProgress = true;
    _blockCount = 0;
    _uploadStartMs = millis();
    _uploadLastBlockMs = millis();
}

// Upload a file from the file system
bool CommandSerial::startUploadFromFileSystem(String& fileSystemName, String& filename,
                const char* pTargetCmdWhenDone)
{
    // Check no upload is already happening
    if (uploadInProgress())
    {
        Log.notice("CommandSerial: startUploadFromFileSystem upload already in progress\n");
        return false;
    }

    // Start a chunked file session
    if (!_fileManager.chunkedFileStart(fileSystemName, filename, false))
    {
        Log.trace("CommandSerial: startUploadFromFileSystem failed to start %s\n", filename);
        return false;
    }

    // Upload now in progress
    _uploadFromFSInProgress = true;
    _blockCount = 0;
    _uploadStartMs = millis();
    _uploadLastBlockMs = millis();
    if (pTargetCmdWhenDone)
        _uploadTargetCommandWhenComplete = pTargetCmdWhenDone;
    return true;
}

void CommandSerial::sendCharToCmdPort(uint8_t ch)
{
    if (_pSerial)
        _pSerial->write(ch);
}

void CommandSerial::frameHandler(const uint8_t *framebuffer, int framelength)
{
    // Handle received frames
    if (_frameRxCallback)
        _frameRxCallback(framebuffer, framelength);
    // Serial.printf("HDLC frame received, len %d\n", framelength);
}
