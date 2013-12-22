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

typedef struct info_ info_t;
struct info_ {
	char		name[24];
	uint32_t	ip;
	int		timezonebias;
	uint16_t	freq;
	int		demo;
	uint32_t	mid;

	struct {
		uint32_t current;
		uint32_t low;
		uint32_t high;
		uint32_t average;
	} ping[1];

	struct {
		float	s2c;
		float	c2s;
		float	s2c_wep;
	} ploss[1];

	struct {
		uint32_t	f1;
		uint32_t	f2;
		uint32_t	f3;
		uint32_t	f4;

		struct {
			uint32_t	slow;
			uint32_t	fast;
			float		pct;
			uint32_t	total_slow;
			uint32_t	total_fast;
			float		total_pct;
		} c2s[1], s2c[1];
	} stats[1];

	struct {
		struct {
			uint32_t days;
			uint32_t hours;
			uint32_t minutes;
		} session[1], total[1];	

		struct {
			unsigned month;
			unsigned day;
			unsigned year;

			unsigned hours;
			unsigned minutes;
			unsigned seconds;
		} created[1];	
	} usage[1];

	uint32_t	bytes_per_second;
	uint32_t	low_bandwidth;
	uint32_t	message_logging;
	char		connect_type[32];
};
//Bytes/Sec:9  LowBandwidth:0  MessageLogging:0  ConnectType:Unknown	

bool parse_info(char *str, int line, info_t *info);

typedef struct einfo_ einfo_t;
struct einfo_ {
	char name[24];
	uint32_t userid;

	struct {
		uint16_t x;
		uint16_t y;
	} res[1];

	uint32_t idle_seconds;
	uint32_t timer_drift;
};

bool parse_einfo(char *str, einfo_t *info);

#endif

/* EOF */

