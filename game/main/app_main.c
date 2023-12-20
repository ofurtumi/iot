// CSTDLIB includes.
#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_random.h>
#include "esp_system.h"  // restart()

#include <aes/esp_aes.h>

// LowNet includes.
#include "lownet.h"

#include "serial_io.h"
#include "utility.h"
//#include "util_signing.h"

#include "app_chat.h"
#include "app_command.h"
#include "app_ping.h"
#include "esp_timer.h"

#include "gameserver.h"

const char* ERROR_OVERRUN = "ERROR // INPUT OVERRUN";
const char* ERROR_UNKNOWN = "ERROR // PROCESSING FAILURE";

const char* ERROR_COMMAND  = "Command error";
const char* ERROR_ARGUMENT = "Argument error";

const char master_public[] =
	"-----BEGIN PUBLIC KEY-----\n"
	"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxG9VF9wuocepQnwBkxUb\n"
	"4YxCo1NJ1MAKAGoaK2csfPABSRkjlESev42rFVzejGtOp2pxKcyihDXVe1BEzD0q\n"
	"HXxEgtkRy0/bJNhGxoMmWTbikO3BmIMIO9zIk3leaNtyy49U27CKDgUHOPp6zd3c\n"
	"dgD3nE4fIE7tU3mCJ4xh5xMHeyoqa/MV3EkE9VDV2vCTP3KyKDFObYqig6XWydeQ\n"
	"CPmSAr0rRYiriguOvQGGxPeaCWPaUAG+t2W7ydpeju+Dkzl6NHm0q9JdLfpg8zje\n"
	"BgLekdFxyM4jAK2hCX+vswUrYqbm5m9rptxQUuSYpk27Ew7uWRaomAWWeMLIg+zt\n"
	"rwIDAQAB\n"
	"-----END PUBLIC KEY-----";


void app_frame_dispatch(const lownet_frame_t* frame)
{
	// Mask the signing bits.
	switch(frame->protocol & 0b00111111) {
		case LOWNET_PROTOCOL_TIME:
			// Ignore TIME packets, deprecated.
			break;

		case LOWNET_PROTOCOL_CHAT:
			chat_receive(frame);
			break;

		case LOWNET_PROTOCOL_PING:
			ping_receive(frame);
			break;

		case LOWNET_PROTOCOL_COMMAND:
			cmd_inbound(frame);
			break;

		case LOWNET_PROTOCOL_GAME:
			game_receive(frame);
			break;
	}
}

void lownet_decrypt(const lownet_secure_frame_t* cipher, lownet_secure_frame_t* plain) {
	// IMPLEMENT ME
	const uint8_t* aes_key = lownet_get_key()->bytes;
	esp_aes_context aes;
	esp_aes_init(&aes);
	if (esp_aes_setkey(&aes, aes_key, 256)) {
		ESP_LOGE("CRYPT", "Decrypt set key failure");
	}

	uint8_t ivt[LOWNET_IVT_SIZE];
	memcpy(ivt, cipher->ivt, LOWNET_IVT_SIZE);

	if (esp_aes_crypt_cbc(
		&aes,	// Context*
		ESP_AES_DECRYPT, // Mode
		sizeof(lownet_secure_frame_t) - LOWNET_IVT_SIZE, // Length
		ivt, // Initialization vector
		(const uint8_t*)&cipher->frame,	// Source
		(uint8_t*)&plain->frame	// Destination
	)) {
		ESP_LOGE("CRYPT", "Decrypt error");
	}
	esp_aes_free(&aes);
}

void lownet_encrypt(const lownet_secure_frame_t* plain, lownet_secure_frame_t* cipher) {
	// IMPLEMENT ME
	const uint8_t* aes_key = lownet_get_key()->bytes;
	esp_aes_context aes;
	esp_aes_init(&aes);
	if (esp_aes_setkey(&aes, aes_key, 256)) {
		ESP_LOGE("CRYPT", "Encrypt set key failure");
	}

	// Copy over the IVT first, as CBC mode mutates this buffer.
	memcpy(cipher->ivt, plain->ivt, LOWNET_IVT_SIZE);

	uint8_t ivt[LOWNET_IVT_SIZE];
	memcpy(ivt, cipher->ivt, LOWNET_IVT_SIZE);

	if (esp_aes_crypt_cbc(
		&aes,	// Context*
		ESP_AES_ENCRYPT, // Mode
		sizeof(lownet_secure_frame_t) - LOWNET_IVT_SIZE, // Length
		ivt, // Initialization vector
		(const uint8_t*)&plain->frame, // Source
		(uint8_t*)&cipher->frame // Destination
	)) {
		ESP_LOGE("CRYPT", "Decrypt error");
	}
	esp_aes_free(&aes);
}


// Two-way RSA test.

#define BIG_BLOCK 256

int rsa_test(void) {
#if 0
    const char message[] = "RSA test message";  // this msg < N
    uint8_t	 plain[ BIG_BLOCK+1];
    uint8_t	 cipher[BIG_BLOCK+1];
    uint8_t  back[  BIG_BLOCK+1];
    int      r1, r2;

    serial_write_line( "Testing RSA private/public encryption:" );
    
    memset(plain,  0, BIG_BLOCK+1);
    memset(cipher, 0, BIG_BLOCK+1);
    memset(back,   0, BIG_BLOCK+1);
    strcpy((char*)plain, message);

    // sign_init() has been called during init!
    r1 = sign_private( plain,  cipher );
    r2 = sign_public(  cipher, back );

    if ( r1 || r2 || memcmp( plain, back, BIG_BLOCK ) )
    {
        serial_write_line( "  Mismatch/error in decoding" );
        ESP_LOGE("RSA", "Test failure");
    }
    else 
        serial_write_line( "  Plaintest and decoded message identical" );
#endif    
    return 0;
}
    
int aes_two_way_test()
{
	const static char* message = "  result: some text";
	lownet_secure_frame_t plain;
	lownet_secure_frame_t cipher;
	lownet_secure_frame_t back;
    
    if ( !lownet_get_key() )
    {
        serial_write_line( "No AES key set" );
        return -1;
    }

    serial_write_line( "Testing lownet_encrypt() and lownet_decrypt():" );
    
	memset(&plain,  0, sizeof(lownet_secure_frame_t));
	memset(&cipher, 0, sizeof(lownet_secure_frame_t));
	memset(&back,   0, sizeof(lownet_secure_frame_t));

    /* ivt: 16 bytes */
    for( int i=0; i<16; i++ )
        plain.ivt[i] = i+1;
    
    size_t n = strlen(message);

	strcpy((char*)plain.frame.payload, message);
    
    // these functions operate on full frame length,
    // so e.g. payload length is irrelevant
	lownet_encrypt( &plain,  &cipher );  
	lownet_decrypt( &cipher, &back   );
    
	if ( back.frame.payload[n] != '\0' || strlen((char*)back.frame.payload) != n )
		ESP_LOGE( "AES-test", "Length violation");
    else 
		serial_write_line((char*)back.frame.payload);

    /* short speed test */
    {
        uint64_t t0 = esp_timer_get_time();
        char buf[80];
        
        for(int i=0; i<1000; i++)
        {
            plain.ivt[0] = i & 0xff;
            plain.ivt[1] = i >> 8;
            lownet_encrypt( &plain,  &cipher );  
        }
        t0 = esp_timer_get_time() - t0;

        sprintf( buf, "  encryption rate: %lu frame/s",
                 1000000000lu / ((unsigned long)t0) );
        serial_write_line( buf );
    }
    
    return 0;
}


void print_usage( void )
{
    const static char *usage[] = 
        {
            "Available commands:",
            " ----------------------------------------------------------------------",
            " /status       : display device status",
            " /date         : print out date",
            " /enc #        : set AES encoding (0,1,x)",
            " /reboot       : reboot the device",
            " /ping # [msg] : ping node # with [msg] (optional), e.g. /ping 0xf0 foo",
            " /game #       : register to game at server #",
            " ----------------------------------------------------------------------",
            " /aes          : test AES frame encryption (set key first)",
            " /tsign        : test SHA256 and RSA with the public key",
            " /rsa          : test RSA function",
            " /diffie       : test modular exponentiation used in Diffie-Helman",
            " ----------------------------------------------------------------------",
            0
        };
    
    for( int i=0; usage[i]; i++ )
        serial_write_line( usage[i] );
}

void my_hash( const uint8_t *data, size_t len, uint8_t *hash ) 
{
    cmd_hash( data, len, hash);
}

#if 0
void my_rsa(  const uint8_t *data, uint8_t *output )
{
    sign_public( data, output );
}
#endif


int parse_command( const char *msg_in )
{
    // Some kind of command.
    if (!strncmp(msg_in, "/ping 0x", 8) && strlen(msg_in) >= 10) {
        // Ping command, extract destination.
        //uint32_t target = hex_to_dec(msg_in + 8);
        uint32_t    x;
        const char *arg = msg_in + 8;
        uint8_t target;

        arg = hex2dec( arg, &x );
        if ( !arg || !x || x > 0xFF) {
            serial_write_line(ERROR_COMMAND);
            return -1;
        }
        target = x & 0x000000FF;
        while ( *arg==' ' )
            arg++;
        if ( *arg )
        {
            size_t len = strlen( arg );
            len = len < 32 ? len : 32;
            ping_ext(target, (const uint8_t *)arg, len );
        }
        else
            ping((uint8_t)(target ));
        return 0;
    }
    
    if (!strcmp(msg_in, "/date")) {
        char msg_out[MSG_BUFFER_LENGTH];
        // Date command, get the network time and write it to serial somehow.
        lownet_time_t net_time = lownet_get_time();
        // Hack the parts field into a fixed point number of milliseconds.
        uint32_t milli = ((uint32_t)net_time.parts * 1000) / 256;
        sprintf(msg_out, "%lu.%03lu sec since the course started.", net_time.seconds, milli);
        serial_write_line(msg_out);
        return 0;
    } else if (!strncmp(msg_in, "/enc", 4)) {
        lownet_key_t use_key;
        char c = (strlen(msg_in) == 6 ? msg_in[5] : '?');
        switch ( c ) 
        {
            case '0':
                use_key = lownet_keystore_read(0);
                lownet_set_key(&use_key);
                serial_write_line("Set lownet stored key 0.");
                return 0;
            case '1':
                use_key = lownet_keystore_read(1);
                lownet_set_key(&use_key);
                serial_write_line("Set lownet stored key 1.");
                return 0;
            case 'x':
                lownet_set_key(NULL);
                serial_write_line("Disabled lownet encryption.");
                return 0;
            default:
                serial_write_line("Usage: /enc #  where # is 0, 1 or x.");
                return -1;
        }
    }

    if (!strcmp(msg_in, "/aes"    )) { return aes_two_way_test();   }
    if (!strcmp(msg_in, "/reboot" )) { esp_restart(); return -1;    }    
    //if (!strcmp(msg_in, "/tsign"  )) { return signature_test( my_hash, my_rsa ); }

    if (!strncmp(msg_in, "/game 0x", 8)) {
        const char *arg = msg_in + 8;
        uint32_t x;
        if ( (arg=hex2dec( arg, &x )) && x > 0 && x < 0xff )
        {
            game_register( x );
            return 0;
        }
    }
    if (!strcmp(msg_in, "/status")) {
        char buf[80];
        void send_buf( void )
        {
            buf[79] = '\0';  
            serial_write_line( buf );
        }    
        uint64_t      tnow = esp_timer_get_time() / 1000; // in ms
        unsigned long ms   = tnow % 1000;
        unsigned long sec  = tnow / 1000;
        lownet_time_t lnow = lownet_get_time();

        serial_write_line( "Device status" );

        snprintf( buf, 80, " Dev uptime : %lu sec %lu ms", sec, ms );
        send_buf();
        
        sec = lnow.seconds;
        ms  = (lnow.parts*1000ul)/256;
        snprintf( buf, 80, " Lownet time: %lu sec %lu ms", sec, ms );
        send_buf();

        snprintf( buf, 80, " Lownet node: 0x%02X", (unsigned)lownet_get_device_id() );
        send_buf();

        snprintf( buf, 80, " Lownet AES : %sabled", lownet_get_key() ? "en" : "dis" );
        send_buf();

        //if ( is_master()  )
        {
            snprintf( buf, 80, " Game server: %d active games", gameserver_active() );
            send_buf();
        }
        
        return 0;
    }

    serial_write_line(ERROR_COMMAND);
    return -1;
}


void app_main(void)
{
    char msg_in[MSG_BUFFER_LENGTH];
    char msg_out[MSG_BUFFER_LENGTH];

    // Generate 32 bytes of noise up front and dump the HEX out.  REMOVE LATER.
    //uint32_t rand = esp_random();
    uint32_t key_buffer[8];
    for (int i = 0; i < 8; ++i) {
        key_buffer[i] = esp_random();
    }
    ESP_LOG_BUFFER_HEX("Hex", key_buffer, 32);
	
    // Initialize the serial services.
    init_serial_service();

    // Init RSA signatures before lownet
    //sign_init();   

    // Initialize the LowNet services.
    lownet_init(app_frame_dispatch, lownet_encrypt, lownet_decrypt);
    lownet_time_t init_time = {1, 0};
    lownet_set_time(&init_time);

    cmd_init( master_public );

    game_init(); // master_init( );    // master node init / autotest    

    /*
     * Serial UI
     */
    while ( 1 )
    {
        memset(msg_in,  0, MSG_BUFFER_LENGTH);
        memset(msg_out, 0, MSG_BUFFER_LENGTH);

        if (!serial_read_line(msg_in)) {
            // Quick & dirty input parse.  Presume input length > 0.
            if (msg_in[0] == '/') {
                if ( parse_command( msg_in ) )
                    print_usage( );                    
            } else if (msg_in[0] == '@' && strlen(msg_in) > 6 && !strncmp(msg_in, "@0x", 3) && msg_in[5] == ' ') {
                // Probably a chat 'tell' command.
                char address[3] = {msg_in[3], msg_in[4], 0};
                uint8_t target = (uint8_t)(hex_to_dec(address));
                // Sanitize message for printable characters, replace every nonprintable with '?'
                for (int i = 6; i < strlen(msg_in); ++i) {
                    if (!util_printable(msg_in[i])) {
                        msg_in[i] = '?';
                    }
                }
                chat_tell(msg_in + 6, target);
            } else {
                // Default, chat broadcast message.
                // Sanitize message for printable characters, replace every nonprintable with '?'
                for (int i = 0; i < strlen(msg_in); ++i) {
                    if (!util_printable(msg_in[i])) {
                        msg_in[i] = '?';
                    }
                }
                chat_shout(msg_in);
            }
        }
    }
}
