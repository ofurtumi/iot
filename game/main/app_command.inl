
void cmd_abandon_cb(void* param) {
	if (!local) { return; }

	// Abandon any listening in progress and revert to IDLE.
	if (!param && local->state != STATE_IDLE) {
		esp_timer_stop(local->timeout);
	}
	local->state = STATE_IDLE;
	memset(local->hash_message, 0, CMD_HASH_SIZE);
	memset(local->signature, 0, CMD_BLOCK_SIZE);
	memset(local->decrypted, 0, CMD_BLOCK_SIZE);
	memset((uint8_t*)&local->command, 0, sizeof(cmd_packet_t));
}

void cmd_abandon() {
	cmd_abandon_cb(NULL);
}

uint8_t cmd_signing_header(uint8_t proto) {
	if ((proto & 0b00111111) != LOWNET_PROTOCOL_COMMAND) { return 0; }
	return ((proto & 0b11000000) >> 6);
}


// Returns 0 on equality.
inline int hash_compare(const uint8_t* hash, const uint8_t* ref) {
	return memcmp(hash, ref, CMD_HASH_SIZE);
}