/*
 * Copyright (c) 2006 cycad <cycad@zetasquad.com>
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
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

#ifndef _CONFIG_HPP
#define _CONFIG_HPP
/* 
 * Configuration file format:
 *
 * Configuration files need to be in the format:
 *	key=value
 */

/*
 * Get a configuration integer value from a file.
 *
 * If 'key' can't be found for any reason, 'default_value' is
 * returned.
 */
int	config_get_int(char *key, int default_value, char *filename);

/*
 * Get a string from a config file.
 *
 * 'key' is the key name to look up.
 * 'dest' is the destination buffer.
 * 'max_len' is the size of the destination buffer.
 * 'default_value' is a pointer to the string to be
 *	returned if 'key' cannot be found.
 * 'filename' is the file to look in.
 */
void	config_get_string(char *key, char *dest, int max_len, char *default_value, char *filename);


bool	config_key_exists(char *key, char *filename);

#endif

