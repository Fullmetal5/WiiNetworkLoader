// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "loader.h"

size_t strlen(const char *s)
{
	size_t len;

	for (len = 0; s[len]; len++)
		;

	return len;
}

void *memset(void *b, int c, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		((unsigned char *)b)[i] = c;

	return b;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	if (n == 0 || dest == src)
		return dest;
	
	if (dest > src) {
		for (size_t i = n - 1; i > 0; i--) {
			((char*)dest)[i] = ((char*)src)[i];
		}
		((char*)dest)[0] = ((char*)src)[0];
	} else {
		for (size_t i = 0; i < n; i++) {
			((char*)dest)[i] = ((char*)src)[i];
		}
	}
	return dest;
}
