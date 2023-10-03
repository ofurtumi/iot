// CSTDLIB includes.
#include <stdio.h>
#include <string.h>

// LowNet includes.
#include "lownet.h"

#include "serial_io.h"
#include "utility.h"

#include "app_chat.c"
#include "app_ping.c"

const char *ERROR_OVERRUN = "ERROR // INPUT OVERRUN";
const char *ERROR_UNKNOWN = "ERROR // PROCESSING FAILURE";

const char *ERROR_COMMAND = "Command error";
const char *ERROR_ARGUMENT = "Argument error";

void app_frame_dispatch(const lownet_frame_t *frame) {
  switch (frame->protocol) {
  case LOWNET_PROTOCOL_CHAT:
    chat_receive(frame);
    break;

  case LOWNET_PROTOCOL_PING:
    ping_receive(frame);
    break;
  }
}

/**
 * Simple utility function to get the device id and print it to the serial
 *
 * thank you @kjartanoli
 */
void id() {
  char buffer[5];
  sprintf(buffer, "%x", lownet_get_device_id());
  serial_write_line(buffer);
}

/**
 * Function to respond to the date command, prints the time since the course
 * started onto the serial console
 */
void date() {
  lownet_time_t time = lownet_get_time();
  // make sure that if both seconds and parts are 0 we print an error and return
  if (time.seconds + time.parts == 0) {
    serial_write_line("Network time is not available.\n");
    return;
  }

  // output string shold be on the following form:
  // 100000.5 sec since the course started.
  char output[11 + 3 + 32];
  sprintf(output, "%lu.%u sec since the course started.\n", time.seconds,
          time.parts);
  serial_write_line(output);
}

/**
 * Lets take the the string id and convert it to a decimal value
 */
void parse_id(char *str, uint8_t *dest) {
  if (str) {
    // lets check if the string is valid then parse it to a long
    long temp = strtol(str, NULL, 16);
    // we also make sure we won't overflow the uint8_t
    if (temp >= 0 || temp <= 255) {
      *dest = temp;
    }
  }
}

void app_main(void) {
  char msg_in[MSG_BUFFER_LENGTH];
  char msg_out[MSG_BUFFER_LENGTH];

  // Initialize the serial services.
  init_serial_service();

  // Initialize the LowNet services.
  lownet_init(app_frame_dispatch);

  while (true) {
    memset(msg_in, 0, MSG_BUFFER_LENGTH);
    memset(msg_out, 0, MSG_BUFFER_LENGTH);

    if (!serial_read_line(msg_in)) {
      // Quick & dirty input parse.  Presume input length > 0.
      if (msg_in[0] == '/') {

        // we know / commands are split into <cmd> <arg>
        // first we get the command
        char *cmd = strtok(msg_in + 1, " ");
        // now since the command should include
        // an argument we continue splitting
        char *arg = strtok(NULL, " ");

        // check what command we got
        if (!strcmp(cmd, "ping")) {

          // make sure we have a valid argument
          if (arg) {
            uint8_t dest = (uint8_t)hex_to_dec(arg + 2);

            // make sure we have a valid node
            if (dest == 0) {
              serial_write_line("Ping error: 0x0 is not a valid node\n");
              continue;
            }
            ping(dest);
          } else {
            serial_write_line("Ping error: A valid node must be provided\n");
            continue;
          }
        } else if (!strcmp(cmd, "id")) {
          id(); // we get the id command we call the id function
        } else if (!strcmp(cmd, "date")) {
          date(); // we get the date command we call the date function
        }

      } else if (msg_in[0] == '@') { // now we are dealing with direct messages

        // we split these commands in a similar way we did with the / commands
        char *dest = strtok(msg_in + 1, " ");
        char *msg = strtok(NULL, "\n");

        if (!msg) {
          serial_write_line("Message error: Some message must be provided\n");
          continue;
        }

        uint8_t dest_dec = (uint8_t)hex_to_dec(dest + 2);
        if (dest_dec == 0) {
          serial_write_line("Message error: 0x0 is not a valid node\n");
          continue;
        }
        chat_tell(msg, dest_dec);
      } else {
        // Default, chat broadcast message.
        chat_shout(msg_in);
      }
    }
  }
}
