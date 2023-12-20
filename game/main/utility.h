
#ifndef GUARD_UTILITY_H
#define GUARD_UTILITY_H

int util_printable(char c);
int chat_strcpy( char *dst, int max, const char *src, int len);


uint32_t    hex_to_dec(const char* hex_digits);
const char *hex2dec(const char* hex_str, uint32_t *res );

#endif
