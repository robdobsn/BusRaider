// Pi Communications
// Rob Dobson 2018

#pragma once

#include <Arduino.h>
#include "ConfigBase.h"
#include "Utils.h"
#include "minihdlc.h"

class PiComms
{
  public:
    static HardwareSerial* _pSerial;
    bool _uploadInProgress;
    int _blockCount;
    unsigned long _uploadStartMs;
    unsigned long _uploadLastBlockMs;
    static const int MAX_UPLOAD_MS = 600000;
    static const int MAX_BETWEEN_BLOCKS_MS = 20000;

    PiComms()
    {
        _pSerial = NULL;
        _uploadInProgress = false;
        _blockCount = 0;
        pinMode(0,OUTPUT);
    }

    static void sendChar(uint8_t ch)
    {
        if (_pSerial)
            _pSerial->write(ch);
        digitalWrite(0, !digitalRead(0));
    }

    static void frameHandler(const uint8_t *framebuffer, int framelength)
    {
        Log.notice("HDLC frame received, len %d\n", framelength);
    }

    void setup(ConfigBase& hwConfig)
    {
        ConfigBase consoleConfig(hwConfig.getString("piComms", "").c_str());
        int portNum = consoleConfig.getLong("portNum", 0);
        _pSerial = NULL;
        if (portNum == 0)
            _pSerial = &Serial;
        else if (portNum == 1)
            _pSerial = &Serial1;
        // HDLC
        minihdlc_init(sendChar, frameHandler);
    }

    void sendFileStartRecord(String& filename, int fileLength)
    {
        String frame = "{\"cmdName\":\"ufStart\",\"fileName\":\"" + filename + "\",\"fileLen\":" + String(fileLength) + "}";
        minihdlc_send_frame((const uint8_t*)frame.c_str(), frame.length());
    }

    void sendFileBlock(size_t index, uint8_t *data, size_t len)
    {
        String header = "{\"cmdName\":\"ufBlock\",\"index\":" + String(index) + ",\"len\":" + String(len) + "}";
        int headerLen = header.length();
        uint8_t* pFrameBuf = new uint8_t[headerLen + len + 1];
        memcpy(pFrameBuf, header.c_str(), headerLen);
        pFrameBuf[headerLen] = 0;
        memcpy(pFrameBuf + headerLen + 1, data, len);
        minihdlc_send_frame(pFrameBuf, headerLen + len + 1);
        delete [] pFrameBuf;
    }

    void sendFileEndRecord(int blockCount)
    {
        String frame = "{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + String(blockCount) + "\"}";
        minihdlc_send_frame((const uint8_t*)frame.c_str(), frame.length());
    }

    void sendTargetCommand(String& targetCmd)
    {
        String frame = "{\"cmdName\":\"" + targetCmd + "\"}";
        minihdlc_send_frame((const uint8_t*)frame.c_str(), frame.length());
    }

    void fileUploadPart(String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool final)
    {
        Log.notice("PiComms: %s, total %d, idx %d, len %d, final %d\n", filename.c_str(), fileLength, index, len, final);

        // Check if first block in an upload
        if (!_uploadInProgress)
        {
            _uploadInProgress = true;
            _uploadStartMs = millis();
            _blockCount = 0;
            sendFileStartRecord(filename, fileLength);
        }

        // Send the block
        sendFileBlock(index, data, len);
        _blockCount++;

        // For timeouts        
        _uploadLastBlockMs = millis();

        // Check if that was the final block
        if (final)
        {
            sendFileEndRecord(_blockCount);
            _uploadInProgress = false;
        }
    }

    void service()
    {
        if (_uploadInProgress)
        {
            // Check for timeouts
            if (Utils::isTimeout(millis(), _uploadLastBlockMs, MAX_BETWEEN_BLOCKS_MS))
            {
                _uploadInProgress = false;
                Log.notice("PiComms: Upload timed out\n");
            }
            if (Utils::isTimeout(millis(), _uploadStartMs, MAX_UPLOAD_MS))
            {
                _uploadInProgress = false;
                Log.notice("PiComms: Upload timed out\n");
            }
        }
    }
};