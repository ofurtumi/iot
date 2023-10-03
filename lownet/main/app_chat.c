#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string.h>

#include "lownet.h"
#include "serial_io.h"
#include "utility.h"

#include "app_chat.h"

/**
 * Function to check if a message can be sent
 *
 * @param message - The message about to be sent
 * @return true if the message can be sent, false otherwise
 */
bool can_send(const char *message) {
  size_t length = strlen(message);
  // check if the message is too long
  // (longer than the predifined payload size)
  if (length > LOWNET_PAYLOAD_SIZE) {
    return false;
  }
  for (int i = 0; message[i]; i++) {
    // check if the message contains non-printable characters
    if (!util_printable(message[i]))
      return false;
  }

  // the message can be sent hooray
  return true;
}

/**
 * Function to handle recieving chat messages
 * parses the message and prints it to serial
 * output if it is directed to us
 *
 * @param frame The frame that was received
 */
void chat_receive(const lownet_frame_t *frame) {
  // the chat is directed towards us
  if (frame->destination == lownet_get_device_id()) {
    // add a small prefix to the message
    char prefix[16]; // msg from userid:
    sprintf(prefix, "msg from 0x%x: ", frame->source);

    // print the message to serial output
    serial_write_line(prefix);
  } else if (frame->destination == 0xFF) {
    // the chat is directed towards everyone
    char prefix[18]; // shout from userid:
    sprintf(prefix, "shout from 0x%x: ", frame->source);

    // print the message to serial output
    serial_write_line(prefix);
  } else {
    // the chat is directed towards someone else
    return;
  }

  // now since we know the message is directed to us in one way or another we
  // can print it out to the serial output

  // we need to add a null terminator to the end of the message
  char message[frame->length + 1];
  // now we simply copy the payload into the message buffer
  memcpy(&message, &frame->payload, frame->length);
  // add the null terminator
  message[frame->length] = '\0';
  // and finally print the message
  serial_write_line(message);
}

/**
 * Function to broadcast a message to all available nodes
 *
 * @param message - The message to broadcast
 */
void chat_shout(const char *message) {
  // if we have no message or the message is invalid, we don't send anything
  if (!can_send(message) || !message) {
    return;
  }

  // we'll need the length of the message later
  size_t length = strlen(message);

  // let's generate a frame to send
  lownet_frame_t frame;
  frame.source = lownet_get_device_id();
  frame.destination = 0xFF;
  frame.protocol = LOWNET_PROTOCOL_CHAT;
  frame.length = length;

  // copy the message into the payload and send it
  memcpy(&frame.payload, message, length);
  frame.payload[length] = '\0'; // add a null terminator
  lownet_send(&frame);
}

/**
 * Function to send a message to a specific node
 *
 * @param message - The message to send
 * @param destination - The two digit hex id of the node to send the message to
 */
void chat_tell(const char *message, uint8_t destination) {
  // if we have no message or the message is invalid, we don't send anything
  if (!can_send(message) || !message) {
    return;
  }

  // we'll need the length of the message later
  size_t length = strlen(message);

  // let's generate a frame to send
  lownet_frame_t frame;
  frame.source = lownet_get_device_id();
  frame.destination = destination;
  frame.protocol = LOWNET_PROTOCOL_CHAT;
  frame.length = length;

  // copy the message into the payload and send it
  memcpy(&frame.payload, message, length);
  frame.payload[length] = '\0';
  lownet_send(&frame);
}
