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

#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "util.hpp"

int Atoi(const char *text) {
	int result = 0;
	unsigned int i = 0;

	unsigned int start = i;
	while (text[i] && !isdigit(text[i]))
		++i;


	while (text[i] && isdigit(text[i])) {
		result = (10 * result) + (text[i] - '0');
		++i;
	}

	if (text[start] == '-')
		result *= -1;

	return result;
}

void
strlwr(char *str)
{
	while (*str) {
		*str = tolower(*str);
		++str;
	}
}


int
IsPub(char *arena)
{
	if (arena[0] == '\0')
		return 0;

	for (int i = 0; arena[i] != '\0'; ++i) {
		if (arena[i] < '0' || arena[i] > '9') 
			return 0;
	}
	
	return 1;
}

int
IsSubarena(char *arena)
{
	if (arena[0] == '\0')
		return 0;

	if (IsPub(arena) != 0)
		return 0;
	
	if (arena[0] == '#')
		return 0;

	return 1;
}

int
IsPriv(char *arena)
{
	if (arena[0] != '#')
		return 0;

	return 1;
}

void DelimArgs(char *dst, int len, char *src, int arg, char delim, bool getremain) {
    int delimcount = 0, destindex = 0;
    int sourceindex = 0;

    while ((src[sourceindex]) && ((delimcount <= arg) || getremain)) {
        if ((src[sourceindex] == delim) && (!getremain || (delimcount < arg))) {
            ++delimcount;
        }
        else if (delimcount == arg) {
            dst[destindex] = src[sourceindex];

            if (destindex + 1 >= len) 
                break;

            ++destindex;
        }

        ++sourceindex;
    }

    dst[destindex] = '\0';
}


int
ArgCount(char *str, char delim)
{
	int nargs = DelimCount(str, delim);

	if (strlen(str) != 0) 
		++nargs;

	return nargs;
}

int DelimCount(char *str, char delim) {
	int res = 0;

	for (int i = 0; str[i] != '\0'; ++i) {
		if (str[i] == delim) {
			++res;
		}
	}

	return res;
}


int AtoiArg(char *src, int arg, char delim) {
    int i = 0, delims = 0;

    while (src[i]) {
        if (src[i] == delim)
            delims++;
        else {
            if (delims == arg)
                return Atoi(src + i);
        }

        ++i;
    }

    return 0;
}

void
TicksToText(char *dst, int dstl, ticks_ms_t ticks)
{
	ticks = (ticks / 1000) / 60;	// ticks is actually seconds
	uint32_t d, h, m;
	m = ticks % 60;
	ticks -= m;
	h = (ticks / 60) % 24;
	ticks -= h;
	d = (ticks / (60 * 24)); 

	if (d) {
		snprintf(dst, dstl, "%ud, %uh, %um", d, h, m);
	} else if (h) {
		snprintf(dst, dstl, "%uh, %um", h, m);
	} else {
		snprintf(dst, dstl, "%um", m);
	}
}

/* gets a line from the file, returns the last character read */
int
get_line(FILE *f, char *buf, int size)
	{ 
	/* clear the buffer */
	bzero(buf, size);

	int i, c;
	i = 0;
	while ((c = fgetc(f)) != EOF &&
	    (c != '\n')) {
		if (i < size-1) { /* if there is space left in the buffer, copy the char */
			buf[i]=c;
			++i;
		}
	}

	return c;
}

/* get the current tick count in milliseconds */
ticks_ms_t 
get_ticks_ms()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
		
	return tv.tv_sec*1000 + tv.tv_usec/1000;
}

ticks_ms_t
GetTicksMs() 
{
	return get_ticks_ms();
}

/* get the current tick count in hundreths of a second */
ticks_hs_t
get_ticks_hs()
{
	return (ticks_hs_t)get_ticks_ms()/10;
}


void
strlcat(char *dst, const char *src, int dst_sz)
{
	size_t dst_len = strlen(dst);

	char *str = &dst[dst_len];

	assert(dst_sz >= (int)dst_len);

	size_t str_sz = dst_sz - dst_len;
	strlcpy(str, src, str_sz);
}


void
strlcpy(char *dst, const char *src, int dst_sz)
{
	if (dst_sz == 0) return;

	snprintf(dst, dst_sz, "%s", src);
	dst[dst_sz - 1] = '\0';	/* null termination is automatic in nix, but not in windows */
}

void*
xcalloc(size_t nmemb, size_t sz)
{
	if (sz == 0 || nmemb == 0) return NULL;

	void *res = calloc(nmemb, sz);
	if (res == NULL) {
		perror("No memory");
		exit(-1);
	}

	return res;
}

void*
xmalloc(size_t sz)
{
	if (sz == 0) return NULL;

	void *res = malloc(sz);
	if (res == NULL) {
		perror("No memory");
		exit(-1);
	}

	return res;
}

void
*xzmalloc(size_t sz)
{
	void *res = xmalloc(sz);
	memset(res, 0, sz);
	return res;
}
