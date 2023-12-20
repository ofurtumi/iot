
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
#include "gameserver.h"
#include "app_chat.h"

#define TAG "gameserver.c"

/*
 *  Single tasks runs these so no locking mechanisms needed!
 */

#define MAX_GAMES        32
#define MAX_STATE      1024

#define FIVE_SECONDS   6000   // little grace period ... but not guaranteed :)
#define TEN_SECONDS   10000

#define STATE_FREE        0
#define STATE_RUNNING     1
#define STATE_ONE_WON     2
#define STATE_TWO_WON     3
#define STATE_TIE         4

#define PRIORITY_GAMESERVER 2  // wohoo, we are important!

/*
 *  Internal data structure - one per ongoing game
 */
typedef struct
{
    uint64_t  last_move;  // internal clock of our ESP32
    uint8_t   state;      // 1=one won, 2=two won, 3=on going
    uint8_t   game;       // what game is this
    uint16_t  seq;        // game id number
    uint16_t  round;      // current round
    uint8_t   turn;       // whose turn, 1 or 2; or 0 if game over
    uint8_t   node_1;
    uint8_t   node_2;
    char      board[MAX_STATE];
} game_t;

#define ANNOUNCEMENT_LEN  80

typedef struct {
    char msg[ ANNOUNCEMENT_LEN ];
} announcement_t;


/********************************************************************************/

static QueueHandle_t      game_queue;
static QueueHandle_t      msg_queue;
static SemaphoreHandle_t  gamelukko;
static uint8_t            waiting_node = 0;

static uint32_t      next_game_seq = 1;
static game_t        games[ MAX_GAMES ];  // init to all zero

static int           gameserver_ok = 0;
static int           active_games  = 0;

/********************************************************************************/
/*
 * just push it to queue and process later
 */
int announce( const char *buf )
{
    announcement_t A;
    
    if ( !buf || strlen(buf) >= ANNOUNCEMENT_LEN )
        return -1;
    memset( A.msg, 0, sizeof(announcement_t) );
    strcpy( A.msg, buf );
    return xQueueSend( msg_queue, &A, 0 ) == pdTRUE ? 0 : -2;
}

/*
 *  non-blocking, called regularly, process at most one assignment
 */
int one_announcement( void )
{
    announcement_t A;    
    if ( xQueueReceive( msg_queue, &A, 0 ) == pdTRUE )
    {
        chat_shout( A.msg );
        return 1;
    }
    return 0;
}    

/********************************************************************************/

uint64_t game_time(void) {
    return esp_timer_get_time() / 1000; // in ms
}


game_t * find_game( uint32_t seq )
{
    for( int i=0; i<MAX_GAMES; i++)
    {
        if ( games[i].seq == seq && games[i].state != STATE_FREE )
            return &games[i];
    }
    return 0;
}


/*
 *  Inform both parties
 */
int send_status( game_t *g )
{
    lownet_frame_t pkt;
    pkt.source      = lownet_get_device_id();
    pkt.protocol    = LOWNET_PROTOCOL_GAME;
    pkt.length      = LOWNET_PAYLOAD_SIZE;  // the payload length is game specific... push it all!
    game_status_t *gs = (game_status_t *)pkt.payload;

    switch( g->state )
    {
        case STATE_ONE_WON: gs->type = GAME_PACKET_WINNER_1;  break;
        case STATE_TWO_WON: gs->type = GAME_PACKET_WINNER_2;  break;
        case STATE_TIE:     gs->type = GAME_PACKET_TIE;       break;
        default:            gs->type = GAME_PACKET_STATUS;    break;
    }
    
    gs->game   = g->game;
    gs->seq    = g->seq;
    gs->round  = g->round;
    gs->node_1 = g->node_1;
    gs->node_2 = g->node_2;
    //for(int i=0; i<GAME_STATE; i++)
    //    gs->state[GAME_STATUS_HEADER+i] = 0;
    tictac_encode(  (const tictactoe_t *)&g->board, (tictactoe_payload_t *)(pkt.payload+sizeof(game_status_t)) );

    pkt.destination = g->node_1;
    lownet_send( &pkt );
    
    if ( g->node_1 != g->node_2 )  // autoplay mode
    {
        pkt.destination = g->node_2;
        lownet_send( &pkt );
    }
    return 0;
}

game_t *start_newgame( uint8_t game, uint8_t node_1, uint8_t node_2 )
{
    if ( !gameserver_ok )
        return 0;

    if ( xSemaphoreTake( gamelukko, 1000 / portTICK_PERIOD_MS ) != pdTRUE )
        return 0;
    
    for(int i=0; i<MAX_GAMES; i++)
    {
        game_t *g = games + i;
        
        if ( g->state == STATE_FREE )
        {
            g->last_move = game_time();
            g->state     = STATE_RUNNING;
            g->game      = game;
            g->seq       = next_game_seq++;
            g->round     = 1;       // player 1 starts!
            g->turn      = node_1;
            g->node_1    = node_1;
            g->node_2    = node_2;
            memset( g->board, 0, MAX_STATE );
            active_games++;
            xSemaphoreGive( gamelukko );
            {
                char buf[80];
                sprintf( buf, "Game %lu between 0x%02x and 0x%02x has started!",
                         (unsigned long)g->seq, (unsigned int)node_1, (unsigned int)node_2 );
                announce( buf );
            }
            send_status( g );
            return g;
        }
    }
    xSemaphoreGive( gamelukko );
    return 0;
}


void announce_winner( game_t *g ) 
{
    uint8_t n1,n2;
    switch ( g->state ) 
    {
        case STATE_ONE_WON:
        case STATE_TIE:
            n1 = g->node_1;
            n2 = g->node_2;
            break;
        case STATE_TWO_WON:
            n1 = g->node_2;
            n2 = g->node_1;
            break;
        default:
            ESP_LOGW(TAG, "Game %lu: undefined result %d",
                     (unsigned long)g->seq, (int)g->state );
            return;
    }

    {
        char buf[80];
        
        if ( g->state==STATE_TIE )
            sprintf( buf, "Game %lu: tie between 0x%02x and 0x%02x",
                     (unsigned long)g->seq, (unsigned int)n1, (unsigned int)n2 );
        else
            sprintf( buf, "Game %lu: 0x%02x wins 0x%02x",
                     (unsigned long)g->seq, (unsigned int)n1, (unsigned int)n2 );
        announce( buf );
    }
    /*  Inform players  */
    send_status( g );
}


/********************************************************************************/


void process_game_action( game_t *g, game_action_t *ga ) 
{    
    switch ( g->game )
    {
        case GAME_TICTACTOE:
            lownet_frame_t pkt;
            uint8_t x = ga->move_x;
            uint8_t y = ga->move_y;
            uint8_t s = 2 - (g->round & 1);
            
            if ( ( s==1 && ga->node != g->node_1 ) ||
                 ( s==2 && ga->node != g->node_2 ) )
            {
                ESP_LOGE(TAG, "action from %02x when it's not its turn!", (unsigned)ga->node );
                s = 0;
            }

            tictactoe_t *b = (tictactoe_t *)g->board;

            //ESP_LOGI(TAG, "action from %02x: (%d,%d) with %d", (unsigned)ga->node, (int)x, (int)y, (int)s );
            
            /* prepare the response */
            pkt.source      = lownet_get_device_id();
            pkt.destination = ga->node;
            pkt.protocol    = LOWNET_PROTOCOL_GAME;
            pkt.length      = sizeof(game_action_t );
            game_action_t *ga2 = (game_action_t *)pkt.payload;
            *ga2 = *ga;
            
            if ( x >= TICTACTOE_BOARD ||
                 y >= TICTACTOE_BOARD ||
                 !s || s>2 ||
                 tictac_set(b,x,y,s) )    // failure -- square already marked?
            {
                ga2->flags = GAME_NACK;
                lownet_send( &pkt );
            }
            else
            {
                ga2->flags = GAME_ACK;
                lownet_send( &pkt );

                g->round++;
                g->turn = (g->round & 1) ? g->node_1 : g->node_2;
                g->last_move = game_time();

                //tictac_display_board( (tictactoe_t *)g->board );  // debug
                
                /* check the game outcome -- inform the opponent */
                int st = tictac_game_over( b );
                if ( st )
                {
                    while ( xSemaphoreTake( gamelukko, 1000 / portTICK_PERIOD_MS ) != pdTRUE )
                        printf( "gamelukko problems...\n" );
                    g->state = st==1 ? STATE_ONE_WON : STATE_TWO_WON;
                    active_games--;
                    xSemaphoreGive( gamelukko );
                    announce_winner( g );
                }
                else
                    send_status( g );  /*  Just inform players  */
            }
            break;
            
        default:
            ESP_LOGW(TAG, "action for unknown game type" );
            return;
    }
}

/*
 * "Elo formula":
 *
 *  r' = r + k*(s-e)
 *
 *  k = "learning rate", 32
 *  s = outcome, (win=1, loss=0, tie=1/2)
 *  e = expected outcome = qa/(qa+qb), qi=10^(ri/c)
 *      where c=400
 *  r_init = 1500
 *  r_min  = 1000
 */

/*
 *  This is a manager and a janitor!
 */
void gameserver_loop( void *p )
{
    while( 1 )
    {
        game_action_t ga;
        int      twait = 500 / portTICK_PERIOD_MS;
        uint64_t tnow  = game_time();

        one_announcement();
        
        while ( xQueueReceive( game_queue, &ga, twait ) == pdTRUE )
        {
            game_t *g = find_game( ga.seq );
            int     plr;
            
            twait = 0;  // let's not prolong this
            if ( !g || g->round != ga.round || g->game != ga.game ||
                 g->state != STATE_RUNNING )
            {
                ESP_LOGW(TAG, "action by %02x for non-existing game (seq %lu)",
                         (unsigned int)ga.node, (unsigned long)ga.seq );
                continue;
            }
            if      ( ga.node == g->node_1 )  plr = 1;
            else if ( ga.node == g->node_2 )  plr = 2;
            else
            {
                ESP_LOGW( TAG, "game_loop: illegal move, ignored (2)" );
                continue;
            }
            if ( ga.node != g->turn )
            {
                ESP_LOGW(TAG, "not node's turn" );
                continue;
            }            

            /* we may have a valid action */
            if ( ga.type == GAME_PACKET_QUIT )
            {
                ESP_LOGW(TAG, "quit packet received -- untested feature!" );
                if ( xSemaphoreTake( gamelukko, portMAX_DELAY ) != pdTRUE )
                    printf( "oops" );
                g->state = plr==1 ? STATE_TWO_WON : STATE_ONE_WON;
                active_games--;
                xSemaphoreGive( gamelukko );
                announce_winner( g );
            }
            else if ( ga.type == GAME_PACKET_ACTION )
            {
                process_game_action( g, &ga );
            }
        }
        
        // Janitor duties
        tnow = game_time();
        for( int i=0; i<MAX_GAMES; i++ ) 
        {
            game_t *g = games + i;
            if ( g->state==STATE_FREE )
                continue;
            if ( g->state==STATE_RUNNING )
            {
                if ( g->last_move + FIVE_SECONDS < tnow )
                {
                    g->state = g->turn==g->node_1 ? STATE_TWO_WON : STATE_ONE_WON;
                    g->last_move = tnow;
                    announce_winner( g );
                }
            }
            else  /* game was finished earlier */
            {
                if ( g->last_move + TEN_SECONDS < tnow )
                    g->state = STATE_FREE;    
            }
        }
    }
}


void game_action( const lownet_frame_t *frame )
{
    const uint8_t        node = frame->source;
    const game_action_t *ga   = (const game_action_t *)frame->payload;

    if ( ga->node != node )
    {
		ESP_LOGW(TAG, "action by %02x for node %02x", (unsigned int)node, (unsigned int)ga->node );
        return;
    }
    if ( xQueueSend( game_queue, ga, 0 ) != pdTRUE )
		ESP_LOGE(TAG, "game queue full!" );
}


void register_node( const lownet_frame_t *frame )
{
    uint8_t node = frame->source;
    const game_register_t *gr = (const game_register_t *)frame->payload;

    void respond( uint8_t n, uint8_t flag )
    {
        lownet_frame_t pkt;
        game_register_t *reg = (game_register_t *)pkt.payload;
        pkt.source      = lownet_get_device_id();
        pkt.destination = n;
        pkt.protocol    = LOWNET_PROTOCOL_GAME;
        pkt.length      = sizeof(game_register_t);
        
        reg->type   = GAME_PACKET_REGISTER;
        reg->game   = gr->game;
        reg->flags  = flag;
        reg->online = 0;
        lownet_send( &pkt );
		ESP_LOGW(TAG, "reg response %d to node %02x", (unsigned int)flag, (unsigned int)n );
    }

    /********************************************/
    
    if ( xSemaphoreTake( gamelukko, 1000 / portTICK_PERIOD_MS ) != pdTRUE )
    {
        respond( node, GAME_NACK );
        return;
    }

    if ( !waiting_node )
    {
		ESP_LOGW(TAG, "node %02x added to waiting list", (unsigned int)node );
        waiting_node = node;
        xSemaphoreGive( gamelukko );
        respond( node, GAME_ACK );
        return;
    }
    
    /* start the game! */
    uint8_t node2 = waiting_node;
    waiting_node = 0;
    xSemaphoreGive( gamelukko );
    
    if ( node2 == node && 0 )
    {
        respond( node, GAME_NACK );
        return;
    }

    respond( node, GAME_ACK );  // quickly before anything happens

    if ( !start_newgame( gr->game, node2, node ) )
    {
        respond( node,  GAME_NACK );  // too many games?!
        respond( node2, GAME_NACK );
		ESP_LOGW(TAG, "Game between %02x and %02x cancelled", (unsigned int)node, (unsigned int)node2 );
    }
}


void gameserver_receive( const lownet_frame_t *frame ) 
{
    const game_msg_header_t *g = (const game_msg_header_t *)frame->payload;

    if ( !gameserver_ok )                  /*  We are not setup as game server */
        return;
    if ( g->game != GAME_TICTACTOE )  /*  Ignore silently games we have no idea about  */
        return;
    
    switch (g->type)
    {
        case GAME_PACKET_REGISTER:  // Register to game
            register_node( frame );
            register_node( frame );  // debug mode, play against itself!
            break;

        case GAME_PACKET_STATUS:    // game active
        case GAME_PACKET_WINNER_1:  // game over
        case GAME_PACKET_WINNER_2:  // game over
        case GAME_PACKET_TIE     :  // and game over
            /* these should come from us - ignore! */
            break;
            
        case GAME_PACKET_ACTION  :  // Player's action
            game_action( frame );
            break;
        case GAME_PACKET_QUIT    :  // Player informs about quiting
            /* IMPLEMENT -- GENERAL QUIT? */
            break;
    }
}


/*
 *  Init something and check that configuration
 *  options make sense. If all is good, games_ok is set!
 */
int gameserver_init( void )
{
    if ( MAX_STATE < sizeof(tictactoe_t) )
    {
        ESP_LOGW(TAG,  "init_games failed: too small MAX_STATE" );
        return -1;
    }
    game_queue = xQueueCreate(20, sizeof(game_action_t));
    msg_queue  = xQueueCreate(10, sizeof(announcement_t));
    
    if ( !game_queue )
    {
        ESP_LOGW(TAG, "Error: game_queue could not be created!" );
        return -1;
    }

    gamelukko = xSemaphoreCreateBinary( );
    xSemaphoreGive( gamelukko );
    
    gameserver_ok = 1;

    xTaskCreate(
        gameserver_loop,
        "gameserver",
        4092,
        NULL,
        PRIORITY_GAMESERVER,
        0 );
    
    return 0;
}

int gameserver_active( void )
{
    return gameserver_ok ? active_games : -1;
}
