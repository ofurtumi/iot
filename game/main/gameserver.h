
#ifndef GAMESERVER_H
#define GAMESERVER_H


#include "games.h"

void gameserver_receive( const lownet_frame_t *frame );
int  gameserver_init( void );

int  gameserver_active( void );  // returns the number of active games, or -1 if disabled




#endif
