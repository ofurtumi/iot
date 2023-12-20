#ifndef TICTACTOE_H
#define TICTACTOE_H

#define TICTACTOE_BOARD     30    // one side of the board
#define TICTACTOE_N     (30*30)   // number of squares
#define TICTACTOE_N2       225    // data size with 4 squares-per-byte
#define TICTACTOE_N3       180    // data size with 5 squares-per-byte (=30^2/5)

/*
 *  Internal representation, one square per byte
 *  0=empty, 1=cross, 2=circle
 */
typedef struct
{
    uint8_t board[ TICTACTOE_N + 100 ];
} tictactoe_t;

/*
 *  Compact representation for data packets
 *  - five squares per octet (base 3)
 */
typedef struct  __attribute__((__packed__))
{
    uint8_t bdata[ TICTACTOE_N3 ];
} tictactoe_payload_t;


int     tictac_encode( const tictactoe_t *b, tictactoe_payload_t *p );
int     tictac_decode( const tictactoe_payload_t *p, tictactoe_t *b );

int     tictac_game_over( const tictactoe_t *b );
int     tictac_auto(      const tictactoe_t *b, int *x, int *y, uint8_t s );
int     tictac_set(             tictactoe_t *b, int  i, int  j, uint8_t s );
uint8_t tictac_get(       const tictactoe_t *b, int  i, int  j );


#endif
