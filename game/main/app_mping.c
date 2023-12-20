
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_ping.h"

#include "serial_io.h"
#include "autotest.h"

typedef struct __attribute__((__packed__))
{
	lownet_time_t 	timestamp_out;
	lownet_time_t	timestamp_back;
	uint8_t 		origin;
} ping_packet_t;

void ping(uint8_t node) {
    static lownet_time_t  t_zero;  // all zero
    static lownet_frame_t pkt;
    ping_packet_t *ping = (ping_packet_t *)pkt.payload;
    uint8_t        me   = lownet_get_device_id();

    pkt.source      = me;
    pkt.destination = node;
    pkt.protocol    = LOWNET_PROTOCOL_PING;
    pkt.length      = sizeof(ping_packet_t);

    ping->timestamp_out  = lownet_get_time();
    ping->timestamp_back = t_zero;
    ping->origin         = me;

    lownet_send( &pkt );
}

void ping_receive(const lownet_frame_t* frame) {
    
	if (frame->length < sizeof(ping_packet_t)) {
		// Malformed frame.  Discard.
		return;
	}

        const ping_packet_t *p1 = (const ping_packet_t *)frame->payload;
        uint8_t              me = lownet_get_device_id();
        char buf[80];
        
	if ( frame->source == p1->origin )
        {
            static lownet_frame_t f2;
            ping_packet_t *p2 = (ping_packet_t *)f2.payload;
            //sprintf( buf, "Ping request from %02X", frame->source );
            //serial_write_line( buf );

            f2.source      = me;
            f2.destination = frame->source;
            f2.protocol    = LOWNET_PROTOCOL_PING;
            f2.length      = sizeof(ping_packet_t);
            
            p2->timestamp_out  = p1->timestamp_out;
            p2->timestamp_back = lownet_get_time();
            p2->origin         = p1->origin;

            lownet_send( &f2 );
            test_ok( frame->source, TESTBIT_PING2 );
        }
	else if ( me == p1->origin )
        {
            lownet_time_t  t = lownet_get_time();
            int  parts = (int)t.parts - p1->timestamp_out.parts;
            long ms    = t.seconds - p1->timestamp_out.seconds;
            ms = 1000*ms + (1000*parts / 256 );  // round down
            
            sprintf( buf, "ping response from %02X, RTT=%ld ms (%lu:%lu)",
                     frame->source, ms,
                     (unsigned long)p1->timestamp_out.seconds,
                     (unsigned long)p1->timestamp_back.seconds );
            serial_write_line( buf );
            // autotest
            if ( ms < 1000 ) 
            {
                int dt = p1->timestamp_back.seconds;
                dt = dt - p1->timestamp_out.seconds;
                test_ok( frame->source, TESTBIT_PING );
                if ( p1->timestamp_back.seconds > 0 &&
                     dt < 5 && dt > -2 )
                    test_ok( frame->source, TESTBIT_TIME );
            }
            
        }    
	else
        {
            sprintf( buf, "Invalid ping message from %02X", frame->source );
            serial_write_line( buf );
        }
}
