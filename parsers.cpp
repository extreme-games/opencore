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

#include <string.h>

#include "parsers.hpp"

#include "util.hpp"

bool
parse_rmt_message(char *str, rmt_msg_t *rmt_msg) 
{
	if (str[0] != '(')
		return false;

	/* This works because ")>" is not allowed in names */
	char *delim = strstr(str, ")>");
	if (delim == NULL)
		return false;

	int name_len = delim - &str[1];
	
	if (name_len <= 0 || name_len >= 20)
		return false;

	strncpy(rmt_msg->name, &str[1], name_len);
	rmt_msg->name[name_len]='\0';

	strlcpy(rmt_msg->msg, &delim[2], 256);

	return true;
}


bool
parse_info(char *str, int line, info_t *info)
{
//IP:69.140.87.2  TimeZoneBias:0  Freq:9999  TypedName:Bot-EG-OpenCore  Demo:0  MachineId:349912338
//Ping:0ms  LowPing:0ms  HighPing:0ms  AvePing:0ms
//LOSS: S2C:0.0%  C2S:0.0%  S2CWeapons:0.0%  S2C_RelOut:0(0)
//S2C:18936-->2  C2S:2-->43922
//C2S CURRENT: Slow:0 Fast:86 0.0%   TOTAL: Slow:0 Fast:42420 0.0%
//S2C CURRENT: Slow:0 Fast:0 0.0%   TOTAL: Slow:0 Fast:0 0.0%
//TIME: Session:   12:00:00  Total:  161:34:00  Created: 1-6-2007 19:29:39
//Bytes/Sec:9  LowBandwidth:0  MessageLogging:0  ConnectType:Unknown	
	int res = 0;
	char name[64];
	char ip[32];
	switch (line) {
	case 1:
		res = sscanf(str, "IP:%31s TimeZoneBias:%d Freq: %hu TypedName:%63[^:]: %d MachineId:%u ",
		    ip, &info->timezonebias, &info->freq, name, &info->demo, &info->mid);
		if (res == 6) {
			char *sep = strstr(name, "  ");
			if (sep) {
				*sep = '\0';
				strlcpy(info->name, name, 24);

				uint32_t a, b, c, d;
				if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
					uint8_t *p = (uint8_t*)&info->ip;
					p[0] = (uint8_t)d;
					p[1] = (uint8_t)c;
					p[2] = (uint8_t)b;
					p[3] = (uint8_t)a;

					return true;
				}
			}
		}		
		break;
	case 2:
		res = sscanf(str, "Ping:%ums LowPing:%ums HighPing:%ums AvePing:%ums ",
		    &info->ping->current, &info->ping->low, &info->ping->high, &info->ping->average);
		if (res == 4) {
			return true;
		}		
		break;	
	case 3:
		res = sscanf(str, "LOSS: S2C:%f%% C2S:%f%% S2CWeapons:%f%% ",
		    &info->ploss->s2c, &info->ploss->c2s, &info->ploss->s2c_wep);
		if (res == 3) {
			return true;
		}		
		break;
	case 4:
		res = sscanf(str, "S2C:%u-->%u C2S:%u-->%u ",
		    &info->stats->f1, &info->stats->f2, &info->stats->f3, &info->stats->f4);
		if (res == 4) {
			return true;
		}		
		break;
	case 5:
		res = sscanf(str, "C2S CURRENT: Slow:%u Fast:%u %f%% TOTAL: Slow:%u Fast:%u %f%% ",
		    &info->stats->c2s->slow, &info->stats->c2s->fast, &info->stats->c2s->pct,
		    &info->stats->c2s->total_slow, &info->stats->c2s->total_fast, &info->stats->c2s->total_pct);
		if (res == 6) {
			return true;
		}		
		break;
	case 6:
		res = sscanf(str, "S2C CURRENT: Slow:%u Fast:%u %f%% TOTAL: Slow:%u Fast:%u %f%% ",
		    &info->stats->s2c->slow, &info->stats->s2c->fast, &info->stats->s2c->pct,
		    &info->stats->s2c->total_slow, &info->stats->s2c->total_fast, &info->stats->s2c->total_pct);
		if (res == 6) {
			return true;
		}		
		break;
	case 7:
		res = sscanf(str, "TIME: Session: %u:%u:%u Total: %u:%u:%u Created: %u-%u-%u %u:%u:%u",
		    &info->usage->session->days, &info->usage->session->hours, &info->usage->session->minutes,
		    &info->usage->total->days, &info->usage->total->hours, &info->usage->total->minutes,
		    &info->usage->created->month, &info->usage->created->day, &info->usage->created->year,
		    &info->usage->created->hours, &info->usage->created->minutes, &info->usage->created->seconds);

		if (res == 12) {
			return true;
		}		
		break;
	case 8:
		res = sscanf(str, "Bytes/Sec:%u LowBandwidth:%u MessageLogging:%u ConnectType:%31s",
		    &info->bytes_per_second, &info->low_bandwidth, &info->message_logging, info->connect_type);

		if (res == 4) {
			return true;
		}		
		break;
	default:
		break;
	}

	return false;
}


bool
parse_einfo(char *str, einfo_t *info)
{

	//Bot-EG-OpenCore: UserId: 3506004  Res: 4096x4096  Client: VIE 1.34  Proxy: Undetermined  Idle: 6 s  Timer drift: 0

	/* in the future, parse these */
	char client[64];
	char proxy[64];

	int res; 
	res = sscanf(str, "%23[^:]: UserId: %u Res: %hux%hu Client: %63[^:]: %63[^:]: %u s Timer drift: %u ",
		info->name, &info->userid, &info->res->x, &info->res->y, client, proxy, &info->idle_seconds, &info->timer_drift
	      );

	if (res == 8) {
		return true;
	}
	
	return false;
}

/* 
 * FIXME: this is sloppy as hell
 */
bool
parse_rmt_alert(char *source, alert_t *alert) 
{
	const char *base = source;
	int endoffset = 0;

	int i = 0;
	while (source[i] && (source[i] != ':')) ++i;

	if (i == 0)
		return false;

	if (i > 15)
		return false;
	else if(source[i] != ':')
		return false;

	strncpy(alert->type, source, i);
	alert->type[i] = '\0';

	if (source[i + 1] != ' ')
		return false;
	else if (source[i + 2] != '(')
		return false;
	else if (!source[i + 3])
		return false;

	base = source + i + 3;

	i = 0;
	while(base[i] && (base[i] != ':')) ++i;

	if (base[i] != ':')
		return false;
	else if (base[i - 1] != ')')
		return false;
	else if (base[i + 1] != ' ')
		return false;
	else if (base[i + 2] == '\0')
		return false;

	strncpy(alert->msg, base + i + 2, 255);
	alert->msg[255] = '\0';

	endoffset = i - 1;

	char *separator = strstr((char*)source, ") (");

	if (!separator)
		return false;
	else if (separator > (base + endoffset))
		return false;

	char *newseparator;

	while ((newseparator = strstr(separator + 3, ") (")) != NULL) {
		if (newseparator > (base + endoffset))
			break;

		separator = newseparator;
	}
	
	if ((separator + 4)[0] == '\0' || (separator + 4)[0] == ')')
		return false;

	
	memset(alert->arena, 0, 16);
	strncpy(alert->arena, separator + 3, 16);

	if (alert->arena[0] == '\0')
		return false;

	int found = 0;

	for (i = 0; (!found) && (i < 16); ++i) {
		if (alert->arena[i] == ')') {
			alert->arena[i] = '\0';
			found = 1;
		}
	}

	if (!found)
		return false;

	if (separator - base == 0)
		return false;
	else if (separator - base > 24)
		return false;

	strncpy(alert->name, base, separator - base);
	alert->name[separator - base] = '\0';

	return true;
}

