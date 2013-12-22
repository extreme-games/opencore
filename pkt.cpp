/*
 * Copyright (c) 2007 cycad <cycad@zetasquad.com>
 *
 * This software is provided 'as-is', without any express or implied 
 * warranty. In no event will the author be held liable for any damages 
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you
 *     must not claim that you wrote the original software. If you use 
 *     this software in a product, an acknowledgment in the product 
 *     documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and
 *     must not be misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <stdarg.h>
#include <ctype.h>

#include "pkt.hpp"

/* 
 * Build a packet according to the string 'fmt' with the variable arguments
 * passed to the function.
 *
 * 'a' - int8_t
 * 'A' - uint8_t
 * 'b' - int16_t
 * 'B' - uint16_t
 * 'c' - int32_t
 * 'C' - uint32_t
 * 'z' - int8_t[], int len
 * 'Z' - uint8_t[], int len
 * 
 * If z/Z are used, the next argument after the source buffer must be
 * the number of bytes to copy from that buffer.
 *
 * Returns the number of bytes written.
 */
int
build_packet(uint8_t *buf, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int8_t a, *z;
	uint8_t A, *Z;
	int16_t b;
	uint16_t B;
	int32_t c;
	uint32_t C;
	int len;

	int i = 0;
	while (*fmt != '\0') {
		switch (*fmt++) {
		case 'a':
			a = va_arg(ap, int);
			buf[i++] = a;
			break;
		case 'A':
			A = va_arg(ap, unsigned int);
			buf[i++] = A;
			break;
		case 'b':
			b = va_arg(ap, int);
			*(int16_t*)&buf[i] = b;
			i += 2;
			break;
		case 'B':
			B = va_arg(ap, unsigned int);
			*(uint16_t*)&buf[i] = B;
			i += 2;
			break;
		case 'c':
			c = va_arg(ap, int);
			*(int32_t*)&buf[i] = c;
			i += 4;
			break;
		case 'C':
			C = va_arg(ap, unsigned int);
			*(uint32_t*)&buf[i] = C;
			i += 4;
			break;
		case 'z':
			z = va_arg(ap, int8_t*);
			len = va_arg(ap, int);	
			memcpy(&buf[i], z, len);
			i += len;
			break;
		case 'Z':
			Z = va_arg(ap, uint8_t*);
			len = va_arg(ap, int);	
			memcpy(&buf[i], Z, len);
			i += len;
			break;
		default:
			va_end(ap);
			return -1;
		}
	}

	va_end(ap);
	return i;
}

/*
 * Extract a buffer into the variables passed as variable arguments to
 * the function.
 *
 * 'a' - int8_t*
 * 'A' - uint8_t*
 * 'b' - int16_t*
 * 'B' - uint16_t*
 * 'c' - int32_t*
 * 'C' - uint32_t*
 * 'z' - int8_t[], int len
 * 'Z' - uint8_t[], int len
 */
int
extract_packet(uint8_t *buf, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int8_t *a, *z;
	uint8_t *A, *Z;
	int16_t *b;
	uint16_t *B;
	int32_t *c;
	uint32_t *C;
	int len;

	/* buffer offset */
	int i = 0;
	while (*fmt != '\0') {
		switch (*fmt++) {
		case 'a':
			a = va_arg(ap, int8_t*);
			if (a) *a = buf[i];
			++i;
			break;
		case 'A':
			A = va_arg(ap, uint8_t*);
			if (A) *A = buf[i];
			++i;
			break;
		case 'b':
			b = va_arg(ap, int16_t*);
			if (b) *b = *(int16_t*)&buf[i];
			i += 2;
			break;
		case 'B':
			B = va_arg(ap, uint16_t*);
			if (B) *B = *(uint16_t*)&buf[i];
			i += 2;
			break;
		case 'c':
			c = va_arg(ap, int32_t*);
			if (c) *c = *(int32_t*)&buf[i];
			i += 4;
			break;
		case 'C':
			C = va_arg(ap, uint32_t*);
			if (C) *C = *(uint32_t*)&buf[i];
			i += 4;
			break;
		case 'z':
			z = va_arg(ap, int8_t*);
			len = va_arg(ap, int);
			if (z) memcpy(z, &buf[i], len);
			i += len;
			break;
		case 'Z':
			Z = va_arg(ap, uint8_t*);
			len = va_arg(ap, int);
			if (Z) memcpy(Z, &buf[i], len);
			i += len;
			break;
		default:
			va_end(ap);
			return -1;
		}
	}

	va_end(ap);
	return i;
}


/*
 * Write the data at buffer to standard out in a formatted and
 * readable way.
 */
void
spew_packet(uint8_t *buf, int len, char *direction)
{
	char hex[32];
	char txt[12];
	char tuple[4];

	bzero(hex,32);
	bzero(txt,12);
	bzero(tuple,4);

	FILE *f = fopen("packet.log", "a+");
	int i;
	for (i = 0; i < len; ++i) {
		snprintf(tuple, 4, "%2.2X ", buf[i]);
		strlcat(hex, tuple, 32);

		txt[i%8]= isalnum(buf[i]) ? buf[i] : '.';

		if ((i+1)%8 == 0) {
			fprintf(f, "[%s] %-32s %s\n", direction, hex, txt);
			bzero(hex,32);
			bzero(txt,12);
		}
	}

	if (i % 8 != 0)
		fprintf(f, "[%s] %-32s %s\n", direction, hex, txt);

	fprintf(f, "\n");
	fclose(f);
}

