#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_ping.h"

#include "serial_io.h"

typedef struct __attribute__((__packed__)) {
  lownet_time_t timestamp_out;
  lownet_time_t timestamp_back;
  uint8_t origin;
} ping_packet_t;

/**
 * Function to print the ping information,
 * we know the packet is valid so we start by printing
 * what we received, then what we sent back
 * @param frame - The frame containing the ping packet
 */
void ping_info(const lownet_frame_t *frame, const lownet_frame_t *reply) {
  char rec_msg[19 + 8 + 8 + 8];
  sprintf(rec_msg, "Ping from 0x%x to 0x%x at %lu:%d\n", frame->source,
          frame->destination,
          ((ping_packet_t *)&frame->payload)->timestamp_out.seconds,
          ((ping_packet_t *)&frame->payload)->timestamp_out.parts);
  serial_write_line(rec_msg);

  char rep_msg[16 + 8 + 8];
  sprintf(rep_msg, "Replied to %x at %lu.%d\n", reply->destination,
          ((ping_packet_t *)&reply->payload)->timestamp_back.seconds,
          ((ping_packet_t *)&reply->payload)->timestamp_back.parts);
  serial_write_line(rep_msg);
}

/**
 * Sends a ping to a specified node.
 *
 * @param node The id of node to ping.
 */
void ping(uint8_t node) {
  // we create a new packet with the current lownet time and out id
  ping_packet_t packet;
  packet.timestamp_out = lownet_get_time();
  packet.origin = lownet_get_device_id();

  size_t length = sizeof(ping_packet_t);

  // here we use the same methods as in chat.c to send the packet
  lownet_frame_t frame;
  frame.source = lownet_get_device_id();
  frame.destination = node;
  frame.protocol = LOWNET_PROTOCOL_PING;
  frame.length = length;
  memcpy(&frame.payload, &packet, length);
  frame.payload[length] = '\0';
  lownet_send(&frame);
}

/**
 * Function to respond to a ping
 * similar to chat_receive however now we need to also respond
 *
 * @param frame - The frame containing the ping packet
 */
void ping_receive(const lownet_frame_t *frame) {
  // we make sure that the frame is valid via a simple check
  // here we are making sure that the frame is at least as long as a ping packet
  // and also making sure we don't ping infinitely
  if (sizeof(ping_packet_t) > frame->length) {
    return;
  }

  uint8_t receiver = frame->destination;
  // now we make sure this is a ping we should respond
  if (receiver == lownet_get_device_id() || receiver == 0xFF) {
    // this ping was meant for us, lets send a message as we have done before
    lownet_frame_t reply;
    reply.source = lownet_get_device_id();
    reply.destination = frame->source;
    reply.protocol = frame->protocol;
    memcpy(&reply.payload, frame->payload, sizeof(ping_packet_t));
    ((ping_packet_t *)&reply.payload)->timestamp_back = lownet_get_time();
    lownet_send(&reply);

    // this is only for printing purposes
    ping_info(frame, &reply);
  }
}
