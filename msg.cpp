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

#include "msg.hpp"

#include "opencore.hpp"

#include "core.hpp"
#include "lib.hpp"
#include "log.hpp"
#include "psend.hpp"
#include "player.hpp"
#include "util.hpp"

static void send_message(MSG_TYPE m, PLAYER_ID pid, const char *msg);
static void rmt_message(THREAD_DATA *td, const char *name, const char *msg);

static
void
send_message(MSG_TYPE m, PLAYER_ID pid, const char *msg)
{
	THREAD_DATA *td = get_thread_data();
	if (td->net->state != NS_CONNECTED) {
		LogFmt(OP_SMOD, "Attempt to send message from invalid state ignored");
		return;
	}

	SOUND s = 0x00;

	pkt_send_message(td, m, s, pid, msg);
}

void
FreqMessage(PLAYER *p, const char *msg)
{
	send_message(MSG_FREQ, p->pid, msg);
}

void
FreqMessageFmt(PLAYER *p, const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);
	FreqMessage(p, buf);

	va_end(ap);
}

void
TeamMessage(const char *msg)
{
	send_message(MSG_TEAM, PID_NONE, msg);
}

void
TeamMessageFmt(const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);
	TeamMessage(buf);

	va_end(ap);
}

void
ChatMessage(const char *chat, const char *msg)
{
	THREAD_DATA *td = get_thread_data();

	char buf[256];
	for (int i = 0; i < MAX_CHATS; ++i) {
		if (strcasecmp(chat, td->chats->chat[i]) == 0) {
			snprintf(buf, 256, ";%d;%s", i+1, msg);
			send_message(MSG_CHAT, PID_NONE, buf);
			break;
		}
	}
}

void
ChatMessageFmt(const char *chat, const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);
	ChatMessage(chat, buf);

	va_end(ap);
}

char*
ChatName(int number)
{
	if (number < 1 || number > MAX_CHATS) {
		return NULL;
	}

	THREAD_DATA *td = get_thread_data();

	return td->chats->chat[number - 1];
}

void
PrivMessage(PLAYER *p, const char *msg)
{
	send_message(MSG_PRIVATE, p->pid, msg);
}

void
PrivMessageFmt(PLAYER *p, const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);
	PrivMessage(p, buf);

	va_end(ap);
}

void
RmtMessage(const char *name, const char *msg)
{
	THREAD_DATA *td = get_thread_data();
	PLAYER *p = player_find_by_name(td, name, MATCH_HERE | MATCH_GONE);
	if (p != NULL && p->in_arena) {
		PrivMessage(p, msg);
	} else {
		rmt_message(td, name, msg);
	}
}

static
void
rmt_message(THREAD_DATA *td, const char *name, const char *msg)
{
	char buf[256];
	snprintf(buf, 256, ":%s:%s", name, msg);

	send_message(MSG_REMOTE, PID_NONE, buf);
}


void
RmtMessageFmt(const char *name, const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);

	RmtMessage(name, buf);

	va_end(ap);
}

void
ArenaMessage(const char *msg)
{
	PubMessageFmt("*arena %s", msg);
}

void
ArenaMessageFmt(const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);

	ArenaMessage(buf);

	va_end(ap);
}

void
PubMessage(const char *msg)
{
	send_message(MSG_PUBLIC, PID_NONE, msg);
}

void
PubMessageFmt(const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);

	PubMessage(buf);

	va_end(ap);
}

void
Reply(const char *msg)
{
	THREAD_DATA *td = get_thread_data();
	CORE_DATA *cd = libman_get_core_data(td);

	if (cd->event != EVENT_COMMAND) {
		Log(OP_SMOD, "Attempt to Reply() from non-command event");
	} else {
		RmtMessage(cd->cmd_name, msg);
	}
}


void
ReplyFmt(const char *fmt, ...)
{
	va_list(ap);
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, 256, fmt, ap);
	Reply(buf);

	va_end(ap);
}

