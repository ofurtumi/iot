#ifndef GAMES_H
#define GAMES_H

#include "lownet.h"
#include "tictactoe.h"


/*
 *  Game packets belong to three groups
 */

/*  Register packets  */
#define GAME_PACKET_REGISTER  0x01  // Register to game
/*  Status packets    */
#define GAME_PACKET_STATUS    0x02  // Game status report
#define GAME_PACKET_WINNER_1  0x03  // Game over
#define GAME_PACKET_WINNER_2  0x04  // Game over
#define GAME_PACKET_TIE       0x05  // and game over
/*  Action packets     */
#define GAME_PACKET_QUIT      0x06  // Player informs about quiting
#define GAME_PACKET_ACTION    0x07  // Player's action


/*
 *  Flags i.e. specic bits
 */
#define GAME_ACK              0x01
#define GAME_NACK             0x02


/*
 *  Games we support
 */
#define GAME_TICTACTOE        0x01

/*
 *  Other constants
 */
#define GAME_STATUS_HEADER      12  // type, game, seq, round, node, res
#define GAME_STATE            ((LOWNET_PAYLOAD_SIZE)-(GAME_STATUS_HEADER))

/******************************************************************************/

typedef struct __attribute__((__packed__))
{
    /*** the common part ***/
    uint8_t       type;     // what kind of packet        (1)
    uint8_t       game;     // what game is this          (1)
} game_msg_header_t;

/******************************************************************************/

typedef struct __attribute__((__packed__))
{
    /*** the common part ***/
    uint8_t       type;     // what kind of packet        (1)
    uint8_t       game;     // what game is this          (1)
    /*** server response uses the following ***/
    uint8_t       flags;    // server: ACK or NACK; node: zero
    uint8_t       online;   // how many players online
} game_register_t;

/******************************************************************************/

/*
 *  Game status messages
 */
typedef struct __attribute__((__packed__))
{
    /*** the common part ***/
    uint8_t       type;     // what kind of packet        (1)
    uint8_t       game;     // what game is this          (1)
    /*** game idenfiers ***/
    uint32_t      seq;      // game id number             (4)
    uint32_t      round;    // game round                 (4)
    /*** status ***/
    uint8_t       node_1;   // player #1                  (1)
    uint8_t       node_2;   // player #2                  (1)
    //uint8_t       state[GAME_STATE];
} game_status_t;

/******************************************************************************/

/*
 *  Game actions messages
 */
typedef struct __attribute__((__packed__))
{
    /*** the common part ***/
    uint8_t       type;     // what kind of packet        (1)
    uint8_t       game;     // what game is this          (1)
    /*** game idenfiers ***/
    uint32_t      seq;      // game id number             (4)
    uint32_t      round;    // game round for action      (4)
    /*** action ***/
    uint8_t       node;     // node whose action this is  (1)
    uint8_t       move_x;   // x-coordinate               (1)
    uint8_t       move_y;   // x-coordinate               (1)
    uint8_t       flags;    // ACK/NACK on response, zero the other way
    uint32_t      checksum; // CRC-24 of the 2-bit presentation
} game_action_t;

/******************************************************************************/

void game_register( uint8_t snode );                // register to game server
void game_receive( const lownet_frame_t *frame );   // handle the incoming packets here
void game_init( void );

/*
 *   Wrong place ...
 */
void tictac_display_board( const tictactoe_t *ttt );

/*
 *  These are implemented in tictac_node.c
 */
uint32_t tictac_checksum( const tictactoe_t *b );
int      tictac_move(     const tictactoe_t *b,
                          int *xp, int *yp,
                          uint8_t s, uint32_t time_ms );
#endif
