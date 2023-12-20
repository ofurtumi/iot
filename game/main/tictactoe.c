#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "tictactoe.h"

/*
 *  Sample code for the tictactoe game with the aim of 5-in-a-row.
 *
 *  Compile with -DSTANDALONE to produce a test binary for stdin/out:
 *
 *    gcc -Wall -DSTANDALONE -o tictactoe tictactoe.c
 *
 *
 *  Oct 29, 2023 -- esa@hi.is
 */

/*
 *  Encode the sparse memory representation to packet payload
 *  using 1 byte per 5 squares.
 */
int tictac_encode( const tictactoe_t *b, tictactoe_payload_t *p )
{
    uint8_t B = 1; // B = 1, 3, 9, 27, 81, 1, ...
    uint8_t v = 0;
    int     j = 0;
    
    for( int i=0; i<TICTACTOE_N; i++)
    {
        v  += b->board[i]*B;
        if ( B >= 81 ) 
        {
            p->bdata[ j++ ] = v;
            B = 1;
            v = 0;
        }
        else
            B *= 3;
    }
    if ( j != TICTACTOE_N3 || B!=1 )
    {
        printf( "Oops, board encoding failed\n" );
        return -1;
    }
    return 0;
}

/*
 * Decode the dense packet representation back to the
 * internal representation in memory that is fast to work with
 */
int tictac_decode( const tictactoe_payload_t *p, tictactoe_t *b )
{
    uint8_t B,v;
    int     k = 0;
    int     err = 0;
    
    for( int i=0; i<TICTACTOE_N3; i++)
    {
        B = p->bdata[i];
        for( int j=0; j<5; j++)
        {
            v = B % 3;
            B = B / 3;
            b->board[k++] = v;
        }
        if ( B )
            err++;
    }
    if ( k != TICTACTOE_N || err )
    {
        printf( "Oops, board decoding failed\n" );
        return -1;
    }
    return 0;
}


/*
 *  Subroutines with no range checks!
 *
 *  tictac_get() returns the piece at (i,j), if any (0,1,2)
 *  set_board() sets piece s to (i,j), return zero on success.
 *
 */
uint8_t tictac_get( const tictactoe_t *b, int i, int j )
{
    return b->board[ i + TICTACTOE_BOARD*j ];
}


int tictac_set( tictactoe_t *b, int i, int j, uint8_t s )
{
    int pos = i + TICTACTOE_BOARD*j;
    if ( b->board[pos] )  // already taken
        return -1;
    b->board[pos] = s;
    return 0;
}

void empty_board( tictactoe_t *b )
{
    memset( b->board, 0, TICTACTOE_N );
}

/*
 *  Returns the winner, or zero if no winner
 */
int tictac_game_over( const tictactoe_t *b )
{
    int evaluate( int i, int j, int dx, int dy )
    {
        int i1 = i + 4*dx;
        int j1 = j + 4*dx;
        if ( i < 0 || i >= TICTACTOE_BOARD ||
             j < 0 || j >= TICTACTOE_BOARD )
        {
            printf( "Oops" );
            while(1)
                ;
        }
        
        if ( i1 < 0 || i1 >= TICTACTOE_BOARD ||
             j1 < 0 || j1 >= TICTACTOE_BOARD )
            return 0;
        uint8_t s = tictac_get(b,i,j);
        if ( !s )
            return 0;
        for( int k=0; k<4; k++ )  // four more and we are done!
        {
            i += dx;
            j += dy;
            if ( tictac_get(b,i,j) != s )
                return 0;
        }
        return s;  // winner found!  1 or 2
    }

    for( int i=0; i<TICTACTOE_BOARD; i++)
    {
        for( int j=0; j<TICTACTOE_BOARD; j++)
        {
            int r =
                evaluate( i, j, 1, 0 ) |
                evaluate( i, j, 0, 1 ) | 
                evaluate( i, j, 1, 1 ) |
                evaluate( i, j, 1,-1 );
            if ( r )
                return r;
        }
    }
    return 0;
}

/* some elementary 16-bit randomness for actions! */
int my_random(void)
{
    static uint32_t st = 1;
    return (st = (64733*st + 1) & 0xffff);
}

/*
 *  Simple heuristic for our end.
 *  Returns zero on a successful choice
 */
int tictac_auto( const tictactoe_t *b, int *x, int *y, uint8_t s )
{
    int best_v  = -1;
    int best_i  =  0;
    int best_j  =  0;
    int m       = TICTACTOE_BOARD - 1;
    int markers = 0;  // count the number of symbols
    int equal   = 0;
    
    for(int i=0; i<TICTACTOE_BOARD; i++ )
    {
        for(int j=0; j<TICTACTOE_BOARD; j++ )
        {
            if ( tictac_get(b,i,j) )
            {
                markers++;
                continue;
            }
            int v = 0;  // value of this action
            if ( i>0        && tictac_get(b,i-1,j  ) )  v++;
            if ( j>0        && tictac_get(b,i,  j-1) )  v++;
            if ( i>0 && j>0 && tictac_get(b,i-1,j-1) )  v++;
            if ( i<m        && tictac_get(b,i+1,j  ) )  v++;
            if ( j<m        && tictac_get(b,i,  j+1) )  v++;
            if ( i<m && j<m && tictac_get(b,i+1,j+1) )  v++;

            if ( v < best_v )
                continue;
            
            if ( v==best_v )
            {
                equal++;
                if ( my_random() > 0xffff/equal )
                    continue;
            }
            else
                equal = 1;
            /* accept this as the best */
            best_v = v;
            best_i = i;
            best_j = j;
        }
    }
    //printf( "auto loops done, best %d, equal=%d\n", best_v, equal );
    
    if ( best_v<0 )
        return -1;
    if ( markers == 0 )
    {
        best_i = TICTACTOE_BOARD / 2;
        best_j = TICTACTOE_BOARD / 2;
    }
    //printf( "best: %d,%d\n", best_i, best_j );
    *x = best_i;
    *y = best_j;
    return 0;
}


/***********************************************************/

#ifdef STANDALONE

void print_board( const tictactoe_t *b )
{
    for(int i=0; i<TICTACTOE_BOARD; i++ )
    {
        printf( "%02d: ", i+1 );
        for(int j=0; j<TICTACTOE_BOARD; j++ )
        {
            uint8_t s = tictac_get(b,i,j);
            printf( "%c",  (s==0 ? '_' : (s==1 ? 'x' : 'o') ) );
        }
        printf( "\n" );
    }
}


void ask_move( tictactoe_t *b, uint8_t s )
{
    while( 1 )
    {
        int i, j;
        
        printf( "Give your move (e.g., 3,4)\n" );
        if ( scanf( "%d,%d", &i, &j ) == 2 &&
             i >= 1 && i <= TICTACTOE_BOARD  &&
             j >= 1 && j <= TICTACTOE_BOARD  )
        {
            i = i - 1;
            j = j - 1;
            if ( !tictac_set(b,i,j,s) )
                return;
            printf( "Square is already marked - try again\n" );
        }
        else
            printf( "Illegal input - try again\n" );
    }
}
        
int main( int argc, char **argv )
{
    tictactoe_payload_t payload;
    tictactoe_t b;
    int res, x, y;
    int round = 0;
    
    empty_board( &b );

    /*
     *  Test the game by playing up to 10 rounds
     */
    while ( !(res=tictac_game_over(&b)) && round < 10 )
    {
        /* check that encoding/decoding to packet format works */
        if ( tictac_encode( &b, &payload ) )
        {
            printf( "Board encoding failed!\n" );
            return -1;
        }        
        empty_board( &b );  // no cheating!
        if ( tictac_decode( &payload, &b ) )
        {
            printf( "Board decoding failed!\n" );
            return -1;
        }

        if ( !(round & 1) )
        {
            print_board( &b );
            ask_move( &b, 1 );
        }
        else if ( !tictac_auto(  &b, &x, &y, 2) &&
                  !tictac_set( &b,  x,  y, 2) )
        {
            // ok move!
        }
        else
        {
            printf( "Heuristic did not find a any move!\n" );
            return -1;
        }        
        round++;
    }
    printf( "Result: %d\n", res );
    return 0;
}

#endif
/***********************************************************/
