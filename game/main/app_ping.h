#ifndef GUARD_APP_PING_H
#define GUARD_APP_PING_H

#include <stdint.h>

#include "lownet.h"

typedef struct __attribute__((__packed__))
{
	lownet_time_t 	timestamp_out;
	lownet_time_t	timestamp_back;
	uint8_t 		origin;
} ping_packet_t;

void ping(uint8_t node);
void ping_ext(uint8_t node, const uint8_t* extra, size_t size);

void ping_receive(const lownet_frame_t* frame);

#endif
