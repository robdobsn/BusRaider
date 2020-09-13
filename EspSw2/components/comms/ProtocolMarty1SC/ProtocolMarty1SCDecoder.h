/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolMarty1SCDecoder
// Command decoder for Marty1 Short Codes protocol
//
// Sandy Enoch 2017-2020
// Was CommandDecoder in original ESP8266 Marty1 firmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <cstring>
#include <cstddef>
#include <functional>

// Standard message receive CB
typedef std::function<void(int id, size_t length)> M1SCStandardMsgCB;
// ROS message receive CB
typedef std::function<void(int id, size_t length)> M1SCROSMsgCB;
// Sensor reading CB
typedef std::function<void(int id, uint8_t sensor, uint8_t sensor_id)> M1SCSensorMsgCB;

#pragma once

inline int mod(int i, int n);

class ProtocolMarty1SCDecoder {

    public:

        ProtocolMarty1SCDecoder(int id, size_t length);
        ~ProtocolMarty1SCDecoder();

        void feed(const uint8_t* source, size_t length);
        void read(uint8_t* dest, size_t length);

        // Called when a new managed packet is ready to be sent
        M1SCStandardMsgCB stdMsgRxCallback;

        // Called when a new raw ROS packet is ready to be sent
        M1SCROSMsgCB rosMsgRxCallback;

        // Called when the command was a GET request
        // format is (decoder_id, sensor, sensor_id)
        M1SCSensorMsgCB sensorMsgCallback;

        void setId(int id);

    private:
       
        int decoder_id;

        uint8_t* buf;
        size_t buf_length;

        size_t start_pos;
        size_t end_pos;

        bool tryParse();

        
        void invalidate(uint8_t amount);
        
        bool bufferFull();
        bool bufferEmpty();
        size_t bufferElementCount();
};
