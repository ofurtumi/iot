
#include <stdint.h>
#include <string.h>

#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "lownet.h"
#include "utility.h"

#include "app_command.h"
#include "app_ping.h"

#define STATE_IDLE			0
#define STATE_LISTENING		1
#define STATE_SIG_RECV		2

#define SIG_UNSIGNED		0b00
#define SIG_SIGNED			0b01
#define SIG_FRONT 			0b10
#define SIG_BACK 			0b11


// 10 seconds in microseconds -- signed packet window for
//	signature(s) to arrive.
#define CMD_LISTEN_TIMEOUT 10000000ull

#define TAG "app_command.c"

typedef struct {
	volatile uint32_t		state;
	uint8_t					hash_key[CMD_HASH_SIZE];
	uint8_t					hash_message[CMD_HASH_SIZE];
	uint8_t					signature[CMD_BLOCK_SIZE];
	uint8_t					decrypted[CMD_BLOCK_SIZE];
	cmd_packet_t			command;
	esp_timer_handle_t		timeout;
	mbedtls_pk_context		pk;
	mbedtls_sha256_context	sha;
	uint64_t 				last_sequence;
} command_context_t;

command_context_t* local = NULL;


// Include the helper functions inline.
#include "app_command.inl"

/*
 *  Compute SHA256 for data, 32-byte result goes to out
 */
int cmd_hash(const uint8_t* data, size_t size, uint8_t* out)
{
	if (!data || !size || !out) {
		return -1;
	}

	int inter = 0;
	inter = mbedtls_sha256_starts(&local->sha, 0);
	if (inter < 0) { return inter; }
	inter = mbedtls_sha256_update(&local->sha, data, size);
	if (inter < 0) { return inter; }
	inter = mbedtls_sha256_finish(&local->sha, out);
	return inter;
}


void cmd_init(const char* rsa_public_key)
{
	if (!rsa_public_key || local) { return; }

	// Sanity check -- command packets shall fit precisely into
	//	lownet frame payload segment.
	if (	sizeof(cmd_packet_t) != LOWNET_PAYLOAD_SIZE
		||	sizeof(cmd_signature_t) != LOWNET_PAYLOAD_SIZE
	) {
		ESP_LOGE(TAG, "Packet structure invariant violation");
		return;
	}

	local = malloc(sizeof(command_context_t));
	if (!local) {
		ESP_LOGE(TAG, "Failed to allocate command context");
		return;
	} else {
		ESP_LOGW(TAG, "Allocated: %p %p", local, &local->pk);
	}

	memset(local, 0, sizeof(command_context_t));

	// Initialize SHA256.
	mbedtls_sha256_init(&local->sha);
	
	// Load the RSA public key.
	mbedtls_pk_init(&local->pk);
	int result = mbedtls_pk_parse_public_key(
		&local->pk,
		(const uint8_t*)rsa_public_key,
		strlen(rsa_public_key) + 1
	);
	if (result) {
		ESP_LOGE(TAG, "Failed to load public key: %s", "foo" );
		cmd_free();
		return;
	}
	// ..and take the hash of the key for comparison later.
	if (cmd_hash((const uint8_t*)rsa_public_key, strlen(rsa_public_key), local->hash_key)) {
		ESP_LOGE(TAG, "Failed to hash public key");
		cmd_free();
		return;
	}
    printf( "debug: hash_key: %02x %02x ...\n", (unsigned)local->hash_key[0], (unsigned)local->hash_key[1] );

	// Prepare the listening timer.
	esp_timer_create_args_t timer_init;
	timer_init.callback = cmd_abandon_cb;
	timer_init.arg = local;
	timer_init.dispatch_method = ESP_TIMER_TASK;
	timer_init.name = "CmdTimeout";
	if (esp_timer_create(&timer_init, &local->timeout) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create command timer");
		cmd_free();
		return;
	}
}


void cmd_free() {
	if (local) {
		cmd_abandon();

		mbedtls_pk_free(&local->pk);
		mbedtls_sha256_free(&local->sha);

		if (local->timeout) {
			esp_timer_delete(local->timeout);
		}

		free(local);
		local = NULL;
	}
}

void cmd_inbound(const lownet_frame_t* frame) {
	if (!local) { return; }

	uint8_t sig_bits = cmd_signing_header(frame->protocol);

    printf( "debug: command packet received, proto %02x\n", (unsigned int)frame->protocol );
    
	if (sig_bits == SIG_UNSIGNED) {
		// Unsigned command packet -- discard.
        printf( "debug: unsigned -- discarded\n" );
		return;
	}
    else if (sig_bits == SIG_SIGNED && local->state == STATE_IDLE)
    {
		// Signed packet when we are in the idle state.
		const cmd_packet_t* command = (const cmd_packet_t*)frame->payload;

        printf( "debug: signed packet received (seq: %lu / %lu)\n",
                (unsigned long)command->sequence,
                (unsigned long)local->last_sequence );

		// Validation: Check the sequence number -- must be strictly greater than last received.
		if (command->sequence <= local->last_sequence) { return; }

		// Store a copy of the command packet; modified to include info from lownet frame (src & len)
		memcpy(&local->command, command, sizeof(cmd_packet_t));
		local->command.reserved[RESERVED_SOURCE] = frame->source;
		local->command.reserved[RESERVED_LENGTH] = frame->length;

		// Generate and store a hash of the _frame_.
		if (cmd_hash((const uint8_t*)frame, sizeof(lownet_frame_t), local->hash_message)) {
			ESP_LOGE(TAG, "Failed to hash command frame");
			cmd_abandon();
			return;
		}

        printf( "debug: signed packet listening ... hash_msg: %02x %02x ...\n",
                (unsigned)local->hash_message[0],
                (unsigned)local->hash_message[1] );
        
		// Update our state from IDLE to LISTENING.
		local->state = STATE_LISTENING;
		if (esp_timer_start_once(local->timeout, CMD_LISTEN_TIMEOUT) != ESP_OK) {
			ESP_LOGE(TAG, "Failed to start listening timer");
			cmd_abandon();
			return;
		}

	} else if (sig_bits == SIG_FRONT && local->state == STATE_LISTENING) {
		// Front half of the signature, listening state (no sig parts received yet).
		const cmd_signature_t* sig = (const cmd_signature_t*)frame->payload;

        printf( "debug: signed packet front: pkt hash_key: %02x %02x ...\n",
                sig->hash_key[0], sig->hash_key[1] );
        printf( "debug: signed packet front: pkt hash_msg: %02x %02x ...\n",
                sig->hash_msg[0], sig->hash_msg[1] );
		// Validation: key hash must match ours.
		if ( hash_compare(sig->hash_key, local->hash_key)) { return; }
        
        printf( "debug: key hash passed ...\n" );

		// Validation: message hash must match ours.
		if (hash_compare(sig->hash_msg, local->hash_message)) { return; }

        printf( "debug: msg hash passed ...\n" );

		// Hashes match -- store the front of the signature block and update our state.
		memcpy(local->signature, sig->sig_part, CMD_BLOCK_SIZE / 2);

		// There is a slight chance we timed out _during_ processing here.
		local->state = (local->state == STATE_LISTENING ? STATE_SIG_RECV : local->state);
        
	} else if (sig_bits == SIG_BACK && local->state == STATE_SIG_RECV) {
        printf( "debug: signed packet back ...\n" );
		// Back half of the signature, listening state(front sig part received).
		const cmd_signature_t* sig = (const cmd_signature_t*)frame->payload;

		// Validation: key hash must match ours.
		if (hash_compare(sig->hash_key, local->hash_key)) { return; }

		// Validation: message hash must match ours.
		if (hash_compare(sig->hash_msg, local->hash_message)) { return; }

		// Stop the timeout timer -- we're done listening.
		esp_timer_stop(local->timeout);

		// Hashes match -- store the back of the signature block.
		memcpy(local->signature + (CMD_BLOCK_SIZE / 2), sig->sig_part, CMD_BLOCK_SIZE / 2);

        printf( "debug: verifying RSA signature\n" );
        
		// Confirm the authenticity of the signature.
		mbedtls_rsa_public(mbedtls_pk_rsa(local->pk), local->signature, local->decrypted);
		if ( /* partial check */
            local->decrypted[0]   == 0 &&
            local->decrypted[220] == 1 &&
            hash_compare(local->decrypted+(256-32), local->hash_message) )
        {
			// Invalid signature.
			ESP_LOGE(TAG, "Invalid signature");
            printf( "debug: wrong RSA signature\n" );
			cmd_abandon();
			return;
		}
        printf( "debug: valid RSA signature\n" );
        ESP_LOGE(TAG, "VALID signature");  // poista ESA

		// Update our last seen sequence number, dispatch the command for handling, and
		//	revert to the idle state.
		local->last_sequence = local->command.sequence;
		cmd_dispatch(&local->command );
		cmd_abandon();
	}
}

/*
 *  This is called only once signature has been verified
 *
 *  len = from lownet frame length field
 */
void cmd_dispatch(const cmd_packet_t* command ) {
	switch(command->type) {
		case COMMAND_TIME:
			lownet_time_t network_time;
			memcpy((uint8_t*)&network_time, command->contents, sizeof(lownet_time_t));
			lownet_set_time(&network_time);
			ESP_LOGI(TAG, "Network time set: %lu.%02X", network_time.seconds, network_time.parts);
			break;

            /*
             * time:  valid time + broadcast ping + check timestamp
             *        invalid time (+1 year) + broadcast ping ...
             */
            /*
             * testing: content A: with correct signature
             *          content B: with invalid signature
             */
		case COMMAND_TEST:
            uint8_t src = command->reserved[RESERVED_SOURCE];  // src node id
            uint8_t len = command->reserved[RESERVED_LENGTH];  // payload length
            len = len > CMD_HEADER_SIZE ? len - CMD_HEADER_SIZE : 0;
            ping_ext( src, command->contents, len ); // tol103x + null
            {
                char b[10];
                int  n = len < 8 ? len : 8;
                n = chat_strcpy( b, 8, (const char*)command->contents, n );
                b[n] = '\0';
                printf( "TEST: Test ping cmd => responded '%s'\n", b );
            }
			ESP_LOGI(TAG, "Test command received and responded");
			break;

		default:
			ESP_LOGW(TAG, "Unrecognized command type %02X", command->type);
	}
}
