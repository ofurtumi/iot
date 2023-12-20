
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
#include "autotest.h"

/*
 * Copies string of length len from src to dst,
 * omitting the non-printable characters
 */
int chat_strcpy( char *dst, const char *src, int len) 
{
    int cnt = 0;
    for(int i=0; i<len; i++)
    {
        if ( util_printable(src[i]) )
            dst[cnt++] = src[i];
    }
    return cnt;
}

void chat_receive(const lownet_frame_t* frame) {
	// Esa
	char buf[  81 ]; // +1 for null-terminator
	char bu2[ 200 ]; // more than enough, lazy!
	int  n = frame->length < 80 ? frame->length : 80;
	int  m = chat_strcpy(buf, (const char*)frame->payload, n );
        buf[m++] = '\0';
            
	if (frame->destination == lownet_get_device_id()) {
		// This is a tell message, just for us!
		snprintf(bu2, 190, "%02X tells you '%s'", (unsigned int)frame->source, buf );
		serial_write_line( bu2 );
                test_ok( frame->source, TESTBIT_TELL );
	} else {
		// This is a broadcast shout message.
		snprintf(bu2, 190, "%02X chats '%s'", (unsigned int)frame->source, buf );
		serial_write_line( bu2 );
                test_ok( frame->source, TESTBIT_CHAT );
	}

        if ( !strcmp(buf,"report") )
            test_report( frame->source );

}

void chat_send(const char* message, uint8_t destination) {
    static lownet_frame_t pkt;
    int n = strlen( message );
    n = n < 80 ? n : 80;  // some bounds, 80 < max payload
    pkt.source      = lownet_get_device_id();
    pkt.destination = destination;
    pkt.protocol    = LOWNET_PROTOCOL_CHAT;
    n = chat_strcpy( (char *)pkt.payload, message, n );
    pkt.length      = n;
    lownet_send( &pkt );
}

void chat_tell(const char* message, uint8_t destination) {
    chat_send( message, destination );
    serial_write_line( "Private message sent!" );
}

void chat_shout(const char* message) {
    chat_send( message, 0xff );
    serial_write_line( "Chat message sent!" );
}

