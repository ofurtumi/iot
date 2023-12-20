#include <ctype.h>
#include <string.h>

#include "utility.h"

int util_printable(char c) {
	return ((c >= ' ' && c < 127)
		?	1
		:	0
	);
}

/*
 * Copies string of length len from src to dst,
 * omitting the non-printable characters
 */
int chat_strcpy( char *dst, int max, const char *src, int len) 
{
    int cnt = 0;
    for(int i=0; i<len && cnt<max; i++)
    {
        if ( util_printable(src[i]) )
            dst[cnt++] = src[i];
    }
    return cnt;
}

const char *hex2dec(const char* hex_str, uint32_t *res ) 
{
    uint32_t x=0;
    const char *cp = hex_str;
    while( 1 )
    {
        if      ( '0' <= *cp && *cp <= '9' )  x = 16*x +    (*cp-'0');
        else if ( 'a' <= *cp && *cp <= 'f' )  x = 16*x + 10+(*cp-'a');
        else if ( 'A' <= *cp && *cp <= 'F' )  x = 16*x + 10+(*cp-'A');
        else
        {
            if ( cp == hex_str )
                return 0;
            *res = x;
            return cp;
        }
        cp++;
    }
}
    
    

uint32_t hex_to_dec(const char* hex_digits) {
	const char* map = "0123456789abcdef";
	uint32_t acc = 0x00000000;

	for (int i = 0; i < strlen(hex_digits); ++i) {
		uint32_t addend = 0x10; // Too large for single digit.
		for (int j = 0; j < 16; ++j) {
			if (tolower(hex_digits[i]) == map[j]) {
				addend = j;
				break;
			}
		}
		if (addend >= 16) {
			// Invalid digit.
			return 0;
		}
		acc = (acc << 4) + addend;
	}
	return acc;
}
