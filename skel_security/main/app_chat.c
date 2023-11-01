
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string.h>

#include "lownet.h"
#include "serial_io.h"

#include "app_chat.h"

void chat_receive(const lownet_frame_t *frame) {
  char buffer[MSG_BUFFER_LENGTH];
  memset(buffer, 0, MSG_BUFFER_LENGTH);

  if (frame->destination == lownet_get_device_id()) {
    // This is a tell message, just for us!
    sprintf(buffer, "[TELL 0x%02X] ", frame->source);
    size_t to_copy = (strlen(buffer) + frame->length >= MSG_BUFFER_LENGTH
                          ? MSG_BUFFER_LENGTH - (strlen(buffer) + 1)
                          : frame->length);
    memcpy(buffer + strlen(buffer), frame->payload, to_copy);
    serial_write_line(buffer);
  } else {
    // This is a broadcast shout message.
    sprintf(buffer, "[CHAT 0x%02X] ", frame->source);
    size_t to_copy = (strlen(buffer) + frame->length >= MSG_BUFFER_LENGTH
                          ? MSG_BUFFER_LENGTH - (strlen(buffer) + 1)
                          : frame->length);
    memcpy(buffer + strlen(buffer), frame->payload, to_copy);
    serial_write_line(buffer);
  }
}

void chat_shout(const char *message) { chat_tell(message, 0xFF); }

void chat_tell(const char *message, uint8_t destination) {
  size_t to_copy = (strlen(message) >= LOWNET_PAYLOAD_SIZE ? LOWNET_PAYLOAD_SIZE
                                                           : strlen(message));

  lownet_frame_t out;
  memset(&out, 0, sizeof(out));

  out.source = lownet_get_device_id();
  out.destination = destination;
  out.protocol = LOWNET_PROTOCOL_CHAT;
  out.length = to_copy;
  memcpy(out.payload, message, to_copy);

  // Send the constructed frame out via LowNet.
  lownet_send(&out);

  // Write a copy of our sent message to serial output as well for convenience.
  char buffer[MSG_BUFFER_LENGTH];
  memset(buffer, 0, MSG_BUFFER_LENGTH);

  sprintf(buffer, "[SEND 0x%02X] ", out.destination);
  to_copy = (strlen(buffer) + out.length >= MSG_BUFFER_LENGTH
                 ? MSG_BUFFER_LENGTH - (strlen(buffer) + 1)
                 : out.length);
  memcpy(buffer + strlen(buffer), out.payload, to_copy);
  serial_write_line(buffer);
}
