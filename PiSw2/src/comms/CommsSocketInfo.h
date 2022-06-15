#pragma once

#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef bool CmdHandlerHandleRxMsgFnType(void* pObject, const char* pCmdJson, 
                    const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);
typedef bool CmdHandlerOTAUpdateFnType(const uint8_t* pData, unsigned dataLen);
typedef bool CmdHandlerTargetFileFnType(void* pObject, const char* rxFileInfo, 
                    const uint8_t* pData, unsigned dataLen);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms Socket Info - this is used to plug-in to the CommmandHandler layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommsSocketInfo
{
public:
    void set(void* pSourceObject,
                bool enabled, CmdHandlerHandleRxMsgFnType* handleRxMsg, 
                CmdHandlerOTAUpdateFnType* otaUpdateFn,
                CmdHandlerTargetFileFnType* receivedFileFn)
    {
        this->pSourceObject = pSourceObject;
        this->enabled = enabled;
        this->handleRxMsg = handleRxMsg;
        this->otaUpdateFn = otaUpdateFn;
        this->receivedFileFn = receivedFileFn;
    }

    // Source object
    void* pSourceObject;

    // Socket enablement
    bool enabled;

    // Callbacks
    CmdHandlerHandleRxMsgFnType* handleRxMsg;
    CmdHandlerOTAUpdateFnType* otaUpdateFn;
    CmdHandlerTargetFileFnType* receivedFileFn;
};

