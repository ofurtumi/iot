#ifndef GUARD_APP_CHAT_H
#define GUARD_APP_CHAT_H

#include <stdint.h>

#include "lownet.h"

void chat_receive(const lownet_frame_t* frame);

void send_tell(uint8_t dst, const char*buf, int len );

void chat_shout(const char* message);
void chat_tell(const char* message, uint8_t destination);

#endif
