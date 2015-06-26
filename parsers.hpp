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
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#ifndef _PARSERS_HPP
#define _PARSERS_HPP

#include <stdint.h>

#include "opencore.hpp"

typedef struct rmt_msg_ rmt_msg_t;
struct rmt_msg_
{
	char name[20];
	char msg[256];
};

/*
 * Parse a remote message 'source' and extract its fields
 * into 'rmt_msg'.
 *
 * Returns true on success and false on failure.
 */
bool parse_rmt_message(char *source, rmt_msg_t *rmt_msg);

typedef struct alert_ alert_t;
struct alert_
{
	char type[16];
	char name[24];
	char arena[16]; char msg[256]; }; /*
 * Parse an ?alert of 'source' into 'alert'. 
 *
 * Returns true on success and false on failure.
 */
bool parse_rmt_alert(char *source, alert_t *alert);


bool parse_info(char *str, int line, info_t *info);
bool parse_einfo(char *str, einfo_t *info);

#endif

/* EOF */

