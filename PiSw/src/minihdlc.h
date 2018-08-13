// From: https://github.com/mengguang/minihdlc

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (* sendchar_type) (uint8_t);
typedef void (* frame_handler_type)(const uint8_t *framebuffer, int framelength);

#define MINIHDLC_MAX_FRAME_LENGTH 100000

void minihdlc_init(sendchar_type, frame_handler_type);
void minihdlc_char_receiver(uint8_t data);
void minihdlc_send_frame(const uint8_t *framebuffer, int frame_length);
