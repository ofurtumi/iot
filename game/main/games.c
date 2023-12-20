#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>
#include "esp_timer.h"

#include "games.h"
#include "tictactoe.h"
#include "serial_io.h"

#include "gameserver.h"  // only our nodes can do both!

#define TAG "games.c"

#define PRIORITY_GAME 5

/*
 *  Different stages of the game
 */
#define GAME_UNDEFINED   0
#define GAME_REGISTERING 1
#define GAME_WAITING     2  /* registration completed */
#define GAME_ACTIVE      3  /* a game is ongoing      */
#define GAME_OVER        4  /* and then ended         */

/*
 * the current game
 */
static game_status_t current;       // latest game status
static tictactoe_t   tictactoe;     // easily accessible representation of the board
static uint8_t       server   = 0;  // node id of the server
static uint8_t       status   = 0;  // current state, GAME_xyz

static SemaphoreHandle_t  game_turn; // half-broken synchronization method, fix!

/*
 *  Display the tictactoe board
 *  - some VT100 codes to keep the board at the top-left corner
 *
 */
void tictac_display_board( const tictactoe_t *ttt ) 
{
    static char buf[TICTACTOE_BOARD + 1];
    
    serial_write_line( "\e7\e[;H" );  // save pos and to upper left
    for (int y=TICTACTOE_BOARD-1; y; y-- )
    {
        for (int x=0; x<TICTACTOE_BOARD; x++ )
        {
            uint8_t s = tictac_get( ttt, x, y );
            char c;
            switch ( s )
            {
                case 1:  c = 'x'; break;
                case 2:  c = 'o'; break;
                default: c = '_'; break;
            }
            buf[x] = c;
        }
        buf[ TICTACTOE_BOARD ] = '\0';
        serial_write_line( buf );
    }
    // restore position and one up as newline follows
    serial_write_line( "\e8\e[A" );  
}

/******************************************************************************/

void game_register( uint8_t snode )
{
    lownet_frame_t pkt;
    game_register_t *reg = (game_register_t *)pkt.payload;

    if ( status!=GAME_UNDEFINED && status!= GAME_OVER )
        ESP_LOGW(TAG, "registering while already registered!" );
    
    //ESP_LOGI(TAG, "registering to game server 0x%02x", (unsigned int)snode );
    server   = snode;
    status   = GAME_REGISTERING;
    
    pkt.source      = lownet_get_device_id();
    pkt.destination = snode;
    pkt.protocol    = LOWNET_PROTOCOL_GAME;
    pkt.length      = sizeof(game_register_t);
        
    reg->type   = GAME_PACKET_REGISTER;
    reg->game   = GAME_TICTACTOE;
    reg->flags  = 0;
    reg->online = 0;
    lownet_send( &pkt );
}

int send_move( int x, int y ) 
{
    lownet_frame_t pkt;
    game_action_t *ga = (game_action_t *)pkt.payload;
    uint8_t        me = lownet_get_device_id();

    pkt.source      = me;
    pkt.destination = server;
    pkt.protocol    = LOWNET_PROTOCOL_GAME;
    pkt.length      = sizeof(game_action_t);
    
    ga->type        = GAME_PACKET_ACTION;
    ga->game        = current.game;
    ga->seq         = current.seq;
    ga->round       = current.round;
    ga->node        = me;
    ga->move_x      = x;
    ga->move_y      = y;
    ga->flags       = 0;
    ga->checksum    = tictac_checksum( &tictactoe );  /* the move already on board! */

    //ESP_LOGW(TAG, "sending the move" );
    lownet_send( &pkt );
    return 0;
}

/*
 *  This task is always ready to make one more move!
 */
void my_policy( void *p )
{
    while( 1 )
    {
        if ( xSemaphoreTake( game_turn, 5000 / portTICK_PERIOD_MS ) != pdTRUE )
            continue;

        if ( status != GAME_ACTIVE || !server )
        {
            ESP_LOGW(TAG, "policy w/ inactive game" );
            continue;
        }        
        if ( current.game != GAME_TICTACTOE )
        {
            ESP_LOGW(TAG, "unsupported game (2)" );
            continue;
        }

        switch( current.type )
        {
            case GAME_PACKET_STATUS:    // game active
            {
                uint8_t me = lownet_get_device_id();
                uint8_t s  = 2 - (current.round & 1);
                int x, y;

                if ( (s==1 && current.node_1 == me) ||
                     (s==2 && current.node_2 == me) )
                {
                    vTaskDelay( 200 / portTICK_PERIOD_MS);
                    if ( !tictac_move( &tictactoe, &x, &y, s, 3000) &&
                         x >= 0 && x < TICTACTOE_BOARD &&
                         y >= 0 && y < TICTACTOE_BOARD )
                    {
                        /* make the move on board and send it */
                        tictac_set( &tictactoe, x, y, s );
                        send_move( x, y );
                    }
                    else
                    {
                        ESP_LOGE( TAG, "move (%d,%d) failed for player %d", (int)x, (int)y, (int)s );
                    }
                }
            }
            break;
                
            case GAME_PACKET_WINNER_1:  // game over
            case GAME_PACKET_WINNER_2:  // game over
            case GAME_PACKET_TIE:       // and game over
                //serial_write_line( "Game over" );
                status = GAME_OVER;
                ESP_LOGE( TAG, "my policy, but game over!" );
                break;
        }
    }
}

/*
 *  This routines handles all incoming packets with GAME protocol
 */
void game_receive( const lownet_frame_t *frame ) 
{
    const game_msg_header_t *g = (const game_msg_header_t *)frame->payload;

    if ( gameserver_active()>=0 )
    {
        gameserver_receive( frame );
        return;
    }
    
    if ( g->game != GAME_TICTACTOE )  /*  Ignore silently games we have no idea about  */
    {
        ESP_LOGW(TAG, "unsupported game" );
        return;
    }

    //ESP_LOGW(TAG, "game protocol message from 0x%02x received (type %u)", frame->source, g->type );
    
    switch (g->type)
    {
        case GAME_PACKET_REGISTER:  // Register to game
        {
            const game_register_t *reg = (const game_register_t *)frame->payload;

            if ( status == GAME_REGISTERING &&
                 frame->source==server      &&
                 reg->flags == GAME_ACK     )
            {
                status   = GAME_WAITING;
                //ESP_LOGI(TAG, "ACK for game registration from 0x%02x", (unsigned int)frame->source );
            }
            break;
        }
        
        case GAME_PACKET_STATUS:    // game active
        case GAME_PACKET_WINNER_1:  // game over
        case GAME_PACKET_WINNER_2:  // game over
        case GAME_PACKET_TIE:       // and game over
        {
            const game_status_t *gs = (const game_status_t *)frame->payload;

            if ( status != GAME_WAITING &&
                 current.seq != gs->seq )
            {
                ESP_LOGW(TAG, "invalid game seq" );
                return;
            }
            
            /* store the state and decode the board */
            current = *gs;
            tictac_decode( (const tictactoe_payload_t *)&frame->payload[GAME_STATUS_HEADER], &tictactoe );
            tictac_display_board( &tictactoe );
            switch( g->type ) 
            {
                case GAME_PACKET_STATUS:
                {
                    status = GAME_ACTIVE;
                    uint8_t me = lownet_get_device_id();
                    uint8_t next = (gs->round & 1) ? gs->node_1 : gs->node_2;

                    //ESP_LOGW(TAG, "status packet: me=%02x next=%02x", (unsigned int)me, (unsigned int)next );

                    if ( next==me )
                        xSemaphoreGive( game_turn );
                    break;
                }
                case GAME_PACKET_WINNER_1:
                case GAME_PACKET_WINNER_2:
                case GAME_PACKET_TIE:
                    status = GAME_OVER;
                    // FIX/CHECK: without delay game_register sometimes fails?!
                    vTaskDelay( 100 / portTICK_PERIOD_MS); 
                    game_register( 0xf0 );  // stress test, remove later FIX FIX
                    break;
            }
            break;
        }
        case  GAME_PACKET_ACTION:  // ACK or NACK, not handled ...  FIX THIS
            break;
            
        default:
            ESP_LOGW(TAG, "invalid message received" );
            break;
    }
}


void game_init( void )
{
    game_turn = xSemaphoreCreateBinary( );

    xTaskCreate(
        my_policy,
        "game_policy",
        4092,
        NULL,
        PRIORITY_GAME,
        0 );
}
