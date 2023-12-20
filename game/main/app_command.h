#ifndef GUARD_APP_COMMAND_H
#define GUARD_APP_COMMAND_H

#include "lownet.h"

#define CMD_BLOCK_SIZE 256
#define CMD_HASH_SIZE 32

#define COMMAND_TIME 0x01
#define COMMAND_TEST 0x02

#define CMD_HEADER_SIZE    12  // after which contents start

#define RESERVED_SOURCE		0
#define RESERVED_LENGTH		1

typedef struct __attribute__((__packed__))
{
	uint64_t	sequence;         /* 8 octets */
	uint8_t		type;             /* 1 octet  */
	uint8_t 	reserved[3];      /* 3 octets */
	uint8_t 	contents[180];
} cmd_packet_t;

typedef struct __attribute__((__packed__))
{
	uint8_t		hash_key[CMD_HASH_SIZE];
	uint8_t		hash_msg[CMD_HASH_SIZE];
	uint8_t		sig_part[CMD_BLOCK_SIZE / 2];
} cmd_signature_t;

void cmd_init(const char* rsa_public_key);
void cmd_free();

// Compute the SHA256 on data[size], result to out[32]
int cmd_hash(const uint8_t* data, size_t size, uint8_t* out);


void cmd_inbound(const lownet_frame_t* frame);
void cmd_dispatch(const cmd_packet_t* command);

#endif
