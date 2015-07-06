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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "util.hpp" /* for get_line() */
#include "core.hpp"
#include "config.hpp"

/* FIXME: refactor config_Xxx functions to share the same parsing code */
/* FIXME: cache files, reopening a file each time is inefficient */
int
config_get_int(char *key, int default_value, char *filename) 
{
	/* this is the default value to return */
	int result = default_value;

	/* open the file */
	FILE *f = fopen(filename, "rb");
	if (f) {
		/* continually read lines until our the key is found */
		char line[512];
		while (get_line(f, line, 512) != EOF) {
			/* if the beginning of the line is the key */
			if (strncasecmp(line, key, strlen(key)) == 0) {
				int offset = strlen(key);
				/* skip any trailing whitespace in the key */
				while (isspace(line[offset+1]))
					++offset;

				/* if the line has an = char, then it is the key */ 
				if (line[offset] == '=') {
					/* set the result and break out of the loop */
					result = atoi(&line[offset+1]);
					break;
				}
			}
		}

		/* close the open file descriptor */
		fclose(f);
	}

	return result;
}

/* FIXME: cache files, reopening a file each time is inefficient */
void
config_get_string(char *key, char *dest,
		int max_len, char *default_value, char *filename)
{
	/* this is the default value to return */
	char *result = default_value;

	/* the line buffer */
	char line[512];
	bzero(line, 512);

	/* open the file */
	FILE *f = fopen(filename, "rb");
	if (f) {
		/* continually read lines until our the key is found */
		while (get_line(f, line, 512) != EOF) {
			/* if the beginning of the line is the key */
			if (strncasecmp(line, key, strlen(key)) == 0) {
				int offset = strlen(key);
				/* skip any trailing whitespace in the key */
				while (isspace(line[offset+1]))
					++offset;

				/* if the line has an = char, then it is the key */ 
				if (line[offset] == '=') {
					/* set the result and break out of the loop */
					result = &line[offset+1];
					break;
				}
			}
		}

		/* close the open file descriptor */
		fclose(f);
	}

	strlcpy(dest, result, max_len);
}


bool
config_key_exists(char *key, char *filename) 
{
	/* this is the default value to return */
	bool result = false;

	/* open the file */
	FILE *f = fopen(filename, "rb");
	if (f) {
		/* continually read lines until our the key is found */
		char line[512];
		while (get_line(f, line, 512) != EOF) {
			/* if the beginning of the line is the key */
			if (strncasecmp(line, key, strlen(key)) == 0) {
				int offset = strlen(key);
				/* skip any trailing whitespace in the key */
				while (isspace(line[offset+1]))
					++offset;

				/* if the line has an = char, then it is the key */ 
				if (line[offset] == '=') {
					result = true;
					break;
				}
			}
		}

		/* close the open file descriptor */
		fclose(f);
	}

	return result;
}

/* gets a line from the file, returns the last character read */
#if 0
static
int
get_config_line(FILE *f, char *buf, int size)
{ 
	/* clear the buffer */
	bzero(buf, size);

	int i = 0,	/* bytes read */
	    c;		/* character read */
	while ((c = fgetc(f)) != EOF && (c != '\n')) {
		if (i < size - 1) { /* if there is space left in the buffer, copy the char */
			buf[i] = c;
			++i;
		}
	}

	return c;
}
#endif


int
GetConfigInt(char *key, int default_value)
{
	THREAD_DATA *td = get_thread_data();

	return config_get_int(key, default_value, td->configfile);
}


void
GetConfigString(char *key, char *dest, int dst_sz, char *default_value)
{
	THREAD_DATA *td = get_thread_data();

	config_get_string(key, dest, dst_sz, default_value, td->configfile);
}


bool
ConfigKeyExists(char *key)
{
	THREAD_DATA *td = get_thread_data();

	return config_key_exists(key, td->configfile);
}


/* EOF */

