// CSTDLIB includes.
#include <stdio.h>
#include <string.h>

// FreeRTOS includes.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Custom includes.
#include "serial_io.h"


// Constants and definitions.
const TickType_t 	read_delay = 50 / portTICK_PERIOD_MS;
const char* 		PROMPT_TOKEN = "> ";

// Internal data structures.
char msg_buffer[MSG_BUFFER_LENGTH];


// Serial IO function implementations.

// Writes a string to the serial output.  Appends a newline character
//	to the provided input string.
void serial_write_line(const char* string) {
	// Pre-condition: Input string may not be NULL.
	if (string == NULL) { return; }

	// Zero the internal message buffer.
	memset(msg_buffer, 0, MSG_BUFFER_LENGTH);

	// We can only write up to LEN - 2 characters; penultimate char would
	//	be newline character '\n' and ultimate char is null terminator '\0'.
	int end = strlen(string);
	if (end > MSG_BUFFER_LENGTH - 2) {
		end = MSG_BUFFER_LENGTH - 2;
	}
	strncpy(msg_buffer, string, end);

	// Add a newline and null terminator to the input.
	msg_buffer[end] = '\n';
	msg_buffer[end+1] = '\0';

	// Write the contents of the internal buffer to standard out (serial output
	//	in this case).
	printf(msg_buffer);
	fflush(stdout);
}

// Writes the prompt string to the serial output, indicating that the
//	firmware is ready to receive input.
void serial_write_prompt() {
	printf(PROMPT_TOKEN);
	fflush(stdout);
}

// Reads a line of input from the serial input, up to MSG_BUFFER_LENGTH - 1
//	characters.  Line is first read to an internal buffer, and then copied to
//	the provided input buffer.
// Trailing newline character is removed from input before copy to buffer.
//
// Returns number of characters read if input length was valid, otherwise
//	negative total number of characters read.  Observe that this number of
//	characters does NOT include the trailing newline.
int serial_read_line(char* buffer) {
	// Precondition: output buffer may not be NULL.
	if (buffer == NULL) { return 0; }

	// Zero the internal message buffer.
	memset(msg_buffer, 0, MSG_BUFFER_LENGTH);

	int done = 0;
	int at = 0;
	while (!done) {
		// Grab the next char from the serial input stream.
		int next = fgetc(stdin);

		// If the result is EOF, then there was nothing there (yet).  In this
		//	circumstance, sleep a while before checking again.
		if (next == EOF) {
			vTaskDelay(read_delay);
			continue;
		}

		if ((char)next == '\n') {
			// Terminating newline character found.
			done = 1;
		} else if (at < (MSG_BUFFER_LENGTH - 1)) {
			msg_buffer[at++] = (char)next;
		} else {
			// We've overrun the internal buffer.  Additional characters
			//	read until a newline is encountered are dumped.
			at++;
		}
	}

	int copy_len = (at >= MSG_BUFFER_LENGTH ? MSG_BUFFER_LENGTH - 1 : at);
	strncpy(buffer, msg_buffer, copy_len);

	if (copy_len != at) {
		return -at;
	}
	return at;
}