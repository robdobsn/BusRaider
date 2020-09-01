// Bus Raider
// Rob Dobson 2019-2020

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../TargetBus/TargetRegisters.h"

class CommandHandler;
class McManager;

enum DZRP {
	// ZXNext: All Commands available in ZXNext (need to be consecutive)
	CMD_INIT=1,

	CMD_CLOSE=2,
	CMD_GET_REGISTERS=3,
	CMD_SET_REGISTER=4,
	CMD_WRITE_BANK=5,
	CMD_CONTINUE=6,
	CMD_PAUSE=7,
	CMD_READ_MEM=8,
	CMD_WRITE_MEM=9,
	CMD_GET_SLOTS=10,
	CMD_SET_SLOT=11,
	CMD_GET_TBBLUE_REG=12,
	CMD_SET_BORDER=13,
	CMD_SET_BREAKPOINTS=14,
	CMD_RESTORE_MEM=15,
	CMD_LOOPBACK=16,
	CMD_GET_SPRITES_PALETTE=17,
	CMD_GET_SPRITES_CLIP_WINDOW_AND_CONTROL=18,

	// Sprites
	CMD_GET_SPRITES=19,
	CMD_GET_SPRITE_PATTERNS=20,

	// Breakpoint
	CMD_ADD_BREAKPOINT=40,
	CMD_REMOVE_BREAKPOINT=41,

	CMD_ADD_WATCHPOINT=42,
	CMD_REMOVE_WATCHPOINT=43,

	// State
	CMD_READ_STATE=50,
	CMD_WRITE_STATE=51,
};

class DeZogInterface
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;
    static const int DEZOG_NUM_MEMBANK_SLOTS = 8;

    DeZogInterface(CommandHandler& commandHandler, McManager& mcManager);
    void init();

    // Service
    void service();

private:

    // Singleton instance
    static DeZogInterface* _pThisInstance;

    // Command handler
    CommandHandler& _commandHandler;

    // Machine Manager
    McManager& _mcManager;

    // Comms socket we're attached to and setup info
    int _commsSocketId;

    // Buffer for received commands and responses
    static const int DEZOG_CMD_MAX_LEN = 0x10000;
    static const int DEZOG_RESP_MAX_LEN = 0x10000;
    uint32_t _rxCmdBufferPos;
    uint8_t _rxCmdBuffer[DEZOG_CMD_MAX_LEN];
    uint32_t _respBufferPos;
    uint8_t _respBuffer[DEZOG_RESP_MAX_LEN];

    // Dezog command details
    uint32_t _rxCmdExpectedMsgLen;
    uint32_t _rxCmdSeqNum;
    uint32_t _rxCmdOrResp;

    // Flag to indicated registers need to be injected
    bool _regsInjectRequired;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsgStatic(void* pObject, const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
    bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    bool handleDeZogCmd(const char* pJsonCmd, const uint8_t* pDezogMsg, uint32_t dezogMsgLen, uint32_t dezogIndex);
    enum ProcCmdRslt
    {
        PROC_CMD_FAILED,
        PROC_CMD_PARAM_ERROR,
        PROC_CMD_SEND_RESPONSE,
        PROC_CMD_NO_RESPONSE
    };
    ProcCmdRslt processCommand(uint32_t msgIdx);
    bool commandMatch(const char* s1, const char* s2); 
    // void addPromptMsg(char* pResponse, int maxResponseLen);
    void mungeDisassembly(char* pText);
    // bool getSemVerFromStr(const char* inStr, int& major, int& minor, int& patch);
    void setRegister(Z80Registers& regs, uint32_t regNumber, uint32_t val);
    uint32_t getRegisters(Z80Registers& regs, uint32_t bufPos);
    uint32_t getAddrFrom8KBank_InitZXSpectrumNext(uint32_t bankNumber8K);

    // Command codes
    static const char* getDeZogCmdCode(uint32_t cmdCode)
    {
        switch(cmdCode)
        {
            case CMD_INIT: { return "Init"; }
            case CMD_CLOSE: { return "Close"; }
            case CMD_GET_REGISTERS: { return "GetRegisters"; }
            case CMD_SET_REGISTER: { return "SetRegister"; }
            case CMD_WRITE_BANK: { return "WriteBank"; }
            case CMD_CONTINUE: { return "Continue"; }
            case CMD_PAUSE: { return "Pause"; }
            case CMD_READ_MEM: { return "ReadMem"; }
            case CMD_WRITE_MEM: { return "WriteMem"; }
            case CMD_GET_SLOTS: { return "GetSlots"; }
            case CMD_SET_SLOT: { return "SetSlot"; }
            case CMD_GET_TBBLUE_REG: { return "GetTBBlueReg"; }
            case CMD_SET_BORDER: { return "SetBorder"; }
            case CMD_SET_BREAKPOINTS: { return "SetBreakpoint"; }
            case CMD_RESTORE_MEM: { return "RestoreMem"; }
            case CMD_LOOPBACK: { return "Loopback"; }
            case CMD_GET_SPRITES_PALETTE: { return "GetSpritesPalette"; }
            case CMD_GET_SPRITES_CLIP_WINDOW_AND_CONTROL: { return "GetSpritesClipWindowAndControl"; }
            case CMD_GET_SPRITES: { return "GetSprites"; }
            case CMD_GET_SPRITE_PATTERNS: { return "GetSpritePatterns"; }
            case CMD_ADD_BREAKPOINT: { return "AddBreakpoint"; }
            case CMD_REMOVE_BREAKPOINT: { return "RemoveBreakpoint"; }
            case CMD_ADD_WATCHPOINT: { return "AddWatchpoint"; }
            case CMD_REMOVE_WATCHPOINT: { return "RemoveWatchpoint"; }
            case CMD_READ_STATE: { return "ReadState"; }
            case CMD_WRITE_STATE: { return "WriteState"; }
        }
        return "UnknownDeZogCommand";
    }

    // Smartload state
    uint32_t _smartloadStartUs;
    bool _smartloadInProgress;
    uint32_t _smartloadMsgIdx;
    bool _smartloadStartDetected;
    static const int MAX_SMART_LOAD_TIME_US = 4000000;
    static const int MAX_TIME_BEFORE_START_DETECT_US = 1000000;

    // Reset pending
    bool _resetPending;
    uint32_t _resetPendingTimeUs;
    static const int MAX_TIME_RESET_PENDING_US = 100000;

    // Event pending
    bool _stepCompletionPending;
};

