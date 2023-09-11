// CSTDLIB includes.
#include <ctype.h>
#include <string.h>

// ESP32 SDK includes.
#include <esp_mac.h>		// Needed for esp_read_mac(..)

// Custom includes.
#include "commands.h"
#include "serial_io.h"

enum CommandToken {
	COMMAND_UNKNOWN,
	COMMAND_MAC,
	COMMAND_ID,
	COMMAND_STATUS,
	COMMAND_DEC
};

typedef struct Command {
	enum CommandToken key;
	char command[MSG_BUFFER_LENGTH];
	char argument[MSG_BUFFER_LENGTH];
} Command;

void init_Command(Command * cmd) {
	if (!cmd) { return; }

	cmd->key = COMMAND_UNKNOWN;
	memset(cmd->command, 0, MSG_BUFFER_LENGTH);
	memset(cmd->argument, 0, MSG_BUFFER_LENGTH);
}


// Helper function forward declarations.
int parse_input(const char* in_msg, Command* out_cmd);

int process_cmd_mac(const Command* in_cmd, char* out_msg);
int process_cmd_status(const Command* in_cmd, char* out_msg);
int process_cmd_dec(const Command* in_cmd, char* out_msg);

void mac_to_string(const uint8_t* mac, char* out);

// Implement a naive command dispatcher.  Better mechanism would be to
//	standardize definitions of commands (command token, number of args, etc.) and
//	present an interface to define/register commands during initialization.
int process_command(const char* in_msg, char* out_msg) {
	// Pre-condition: input and output char buffers must not be NULL.
	if (!in_msg || !out_msg) { return -1; } 

	// Keep an internal buffer for composing output string.
	char str_buffer[MSG_BUFFER_LENGTH];
	memset(str_buffer, 0, MSG_BUFFER_LENGTH);

	Command cmd;
	init_Command(&cmd);
	parse_input(in_msg, &cmd);
	int command_result = 0;

	switch(cmd.key) {
		case COMMAND_MAC: 
			command_result = process_cmd_mac(&cmd, str_buffer);
			break;

		case COMMAND_ID:
			// ...
			break;

		case COMMAND_STATUS:
			command_result = process_cmd_status(&cmd, str_buffer);
			break;

		case COMMAND_DEC:
			command_result = process_cmd_dec(&cmd, str_buffer);
			break;

		default:
			command_result = 1;	// Command error.
	}

	if (command_result) { return command_result; }

	// For safety, overwrite a null terminator onto our transient buffer,
	//	regardless of whether one was there or not previously.
	str_buffer[MSG_BUFFER_LENGTH - 1] = '\0';
	strncpy(out_msg, str_buffer, MSG_BUFFER_LENGTH);
	return 0;
}

int parse_input(const char* in_msg, Command* out_cmd) {
	// Precondition: in_msg and out_cmd may not be NULL.
	if (!in_msg || !out_cmd) { return -1; }

	const size_t len = strlen(in_msg);
	if (len >= MSG_BUFFER_LENGTH) {
		// Take an out here -- if somehow the input message is longer than the
		//	input message buffer then something has run amok.
		return -2;
	}

	// Zero the Command structure up front.
	init_Command(out_cmd);

	int at_src = -1;
	int at_dst = 0;
	int has_split = 0;
	char* target = out_cmd->command;
	while (++at_src < len) {
		switch(in_msg[at_src]) {
			case ' ':
			case '\t':
				if (!has_split) {
					has_split = 1;
					target = out_cmd->argument;
					at_dst = 0;
					break;
				}
				// Note intentional fallthrough here.  If whitespace is not being used to
				//	split command and argument, then it should be included in whatever
				//	target we're building.

			default:
				// If we are still filling the command token, force characters to lower-case
				//	to implement case insensitivity.
				target[at_dst++] = (has_split ? in_msg[at_src] : (char)tolower(in_msg[at_src]));
		}
	}
	
	if (!strcmp(out_cmd->command, "mac")) 			{ out_cmd->key = COMMAND_MAC; }
	else if (!strcmp(out_cmd->command, "id")) 		{ out_cmd->key = COMMAND_ID; }
	else if (!strcmp(out_cmd->command, "status")) 	{ out_cmd->key = COMMAND_STATUS; }
	else if (!strcmp(out_cmd->command, "dec")) 		{ out_cmd->key = COMMAND_DEC; }

	return 0;
}

int process_cmd_mac(const Command* in_cmd, char* out_msg) {
	uint8_t local_mac[6];
	const char* prefix = "MAC ";
	esp_read_mac(local_mac, ESP_MAC_WIFI_STA);
	strcpy(out_msg, prefix);
	mac_to_string(local_mac, out_msg + strlen(prefix));
	return 0;
}

void mac_to_string(const uint8_t* mac, char* out) {
	// Pre-condition: mac bytes and output string must not be NULL.
	if (!mac || !out) { return; }

	const char* hexmap = "0123456789ABCDEF";
	char sep = ':';

	int at = 0;
	for (int i = 0; i < 6; ++i) {
		int lo = (mac[i] & 0x0F);
		int hi = ((mac[i] & 0xF0) >> 4);
		out[at++] = hexmap[hi];
		out[at++] = hexmap[lo];
		if (i < 5) {
			out[at++] = sep;
		}
	}
}


int process_cmd_status(const Command* in_cmd, char* out_msg) {
	strcpy(out_msg, "IMPLEMENT..");
	return 0;
}


int process_cmd_dec(const Command* in_cmd, char* out_msg) {
	strcpy(out_msg, "IMPLEMENT..");
	return 0;
}
