/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolMarty1SCDecoder
// Command decoder for Marty1 Short Codes protocol
//
// Sandy Enoch 2017-2020
// Was CommandDecoder in original Marty1 firmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "ProtocolMarty1SCDecoder.h"

ProtocolMarty1SCDecoder::ProtocolMarty1SCDecoder(int id, size_t length) {
    buf = new uint8_t[length];
    buf_length = length;
    start_pos = 0;
    end_pos = 0;
    decoder_id = id;
};

ProtocolMarty1SCDecoder::~ProtocolMarty1SCDecoder() {
    delete buf;
}

void ProtocolMarty1SCDecoder::feed(const uint8_t* source, size_t length) {
    const uint8_t* source_end = source + length;

    while (source < source_end) {
        // Read as much as we can from the source buffer 
        while (source < source_end && !bufferFull()) {
            buf[end_pos] = *source;
            source++;
            end_pos = (end_pos + 1) % buf_length;
        }

        // Parse as much as we can
        while (tryParse());
    }
}

bool ProtocolMarty1SCDecoder::bufferFull() {
    return (mod(start_pos - end_pos, buf_length) == 1);
}

bool ProtocolMarty1SCDecoder::bufferEmpty() {
    return (start_pos == end_pos);
}

size_t ProtocolMarty1SCDecoder::bufferElementCount() {
    return mod(end_pos - start_pos, buf_length);
}

void ProtocolMarty1SCDecoder::invalidate(uint8_t amount) {
    start_pos = (start_pos + amount) % buf_length;
}

bool ProtocolMarty1SCDecoder::tryParse() {
    // couldn't have a complete command packet smaller than 3 bytes.
    if (bufferElementCount() < 3) return false; 
   
    uint8_t b0 = buf[(start_pos + 0) % buf_length];
    uint8_t b1 = buf[(start_pos + 1) % buf_length];
    uint8_t b2 = buf[(start_pos + 2) % buf_length];

    uint16_t payload_size = b1 + (b2 << 8);

    LOG_V("M1SCD", "b0 %02x payload_size %d payload[0] %02x", b0, payload_size, buf[(start_pos + 3) % buf_length]);

    // Re-position the start ptr so that a read will return just the payload.
    invalidate(3);

    switch (b0) {
        case 0x01: // GET
            if (sensorMsgCallback) 
                sensorMsgCallback(decoder_id, b1, b2);
            return true;
            break;
        case 0x02: // COMMAND
            if (bufferElementCount() >= payload_size) {
                if (stdMsgRxCallback) stdMsgRxCallback(decoder_id, (size_t) payload_size); 
                return true;
            } 
            break;
        case 0x03: // RAW ROS
            if (bufferElementCount() >= payload_size) {
                if (rosMsgRxCallback) rosMsgRxCallback(decoder_id, (size_t) payload_size);
                return true;
            } 
            break;
        default:
            return true;
            break;
    }
    return false;
}

void ProtocolMarty1SCDecoder::read(uint8_t* dest, size_t length) {
    for (int i = 0; i < length; i++) {
        dest[i] = buf[(start_pos) % buf_length];
        start_pos = (start_pos + 1) % buf_length;
    }
}

// positive (unsigned) modulo for proper handling of pointer wrap-around
inline int mod(int i, int n) {
    return (i % n + n) % n;
}
