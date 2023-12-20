
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string.h>

#include "lownet.h"
#include "serial_io.h"

#include "app_chat.h"

// autograding needs the below
#include "utility.h"


void chat_receive(const lownet_frame_t* frame) {
    char msg[100];
    int  n;

    n = chat_strcpy( msg, 100-1, (const char *)frame->payload, frame->length );
    msg[n] = '\0';
    
    {
        char buffer[MSG_BUFFER_LENGTH];
        n = snprintf(buffer, MSG_BUFFER_LENGTH, "[%s 0x%02X] %s",
                 frame->destination == lownet_get_device_id() ? "TELL" : "CHAT",
                 frame->source, msg );
        if ( n >= MSG_BUFFER_LENGTH )
            buffer[ MSG_BUFFER_LENGTH-1 ] = '\0'; // simply truncate ...
        serial_write_line(buffer);
    }    
}

void chat_shout(const char* message) {
	chat_tell(message, 0xFF);
}


/*
 *  So that we can send msgs without them appearing to local screen
 */
void send_tell(uint8_t dst, const char*buf, int len ) {
	lownet_frame_t out;

    len = len < LOWNET_PAYLOAD_SIZE ? len : LOWNET_PAYLOAD_SIZE;
	memset(&out, 0, sizeof(out));

	out.source      = lownet_get_device_id();
	out.destination = dst;
	out.protocol    = LOWNET_PROTOCOL_CHAT;
	out.length      = len;
	memcpy(out.payload, buf, len);
	lownet_send(&out);
}

void chat_tell(const char* message, uint8_t dst) {
	char buf[ 100 ];
    int n = chat_strcpy( buf, 100, message, strlen(message) );
    send_tell( dst, buf, n );
    
	// Write a copy of our sent message to serial output as well for convenience.
	snprintf(buf, 100, "[%s 0x%02X] %s",
             dst==0xFF ? "CHAT" : "SEND",
             dst, message );
    buf[99] = '\0';
	serial_write_line(buf);
}
