

#ifndef UTIL_BCD_H
#define UTIL_BCD_H


static inline uint dec2bcd(uint x) { return (x / 10) * 16 + (x % 10); }
static inline uint bcd2dec(uint x) { return (x / 16) * 10 + (x % 16); }

static inline uint dec2bcd16(uint x) { return dec2bcd(x / 100) * 256 + dec2bcd(x % 100); }
static inline uint bcd2dec16(uint x) { return bcd2dec(x / 256) * 100 + bcd2dec(x % 256); }


#endif

