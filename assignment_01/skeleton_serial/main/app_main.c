// CSTDLIB includes.
#include <stdio.h>
#include <string.h>

// FreeRTOS includes.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Custom includes.
#include "commands.h"
#include "serial_io.h"

const char* MESSAGE_SYNC = "SYNC // FIRMWARE READY";
const char* ERROR_OVERRUN = "ERROR // INPUT OVERRUN";
const char* ERROR_UNKNOWN = "ERROR // PROCESSING FAILURE";

const char* ERROR_COMMAND = "Command error";
const char* ERROR_ARGUMENT = "Argument error";

void app_main(void)
{
	char msg_in[MSG_BUFFER_LENGTH];
	char msg_out[MSG_BUFFER_LENGTH];

	serial_write_line(MESSAGE_SYNC);
	while (true) {
		memset(msg_in, 0, MSG_BUFFER_LENGTH);
		memset(msg_out, 0, MSG_BUFFER_LENGTH);

		serial_write_prompt();
		int count = serial_read_line(msg_in);
		if (count < 0) {
			serial_write_line(ERROR_OVERRUN);
		} else if (count == 0) {
			// Do nothing.  Allow loop to continue and write a new prompt.
		} else { // count > 0
			int processing_result = process_command(msg_in, msg_out);
			switch (processing_result) {
				case 0:	// Successful processing.
					serial_write_line(msg_out);
					break;
				
				case 1: // Command error.
					serial_write_line(ERROR_COMMAND);
					break;

				case 2: // Argument error.
					serial_write_line(ERROR_ARGUMENT);
					break;

				default: // Unrecognized result.
					serial_write_line(ERROR_UNKNOWN);
			}
		}
	}
}
