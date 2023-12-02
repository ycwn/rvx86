

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "core/types.h"

#include "util/hexdump.h"



void hexdump(uintptr_t addr, const void *buf, size_t len)
{

	char hex[64], str[32];

	while (len > 0) {

		int count = (len > 16)? 16: len;
		char *h = hex, *s = str;

		for (int n=0; n < count; n++) {

			u8 ch = ((u8*)buf)[n];

			if (n == 8) *h++ = *s++ = ' ';

			sprintf(h, "%02x ", ch);
			h += 3;

			*s++ = isprint(ch)? ch: '.';

		}

		*h = *s = 0;

		printf("%010x |  %-49s | %-17s |\n", addr, hex, str);

		addr += count;
		buf  += count;
		len  -= count;

	}

}

