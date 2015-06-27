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

#include <pthread.h>

#include <stdarg.h>
#include <time.h>

#include <list>

#include "log.hpp"

#define LOG_LINES 100
#define CMD_LOG_LINES 100

typedef struct log_entry LOG_ENTRY;
struct log_entry {
	OP_LEVEL lvl;
	char text[256];
};

typedef struct cmdlog_entry CMDLOG_ENTRY;
struct cmdlog_entry {
	OP_LEVEL lvl;
	char text[256];
};

static pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;
static LOG_ENTRY *log[LOG_LINES];
static int logi = 0;

static pthread_mutex_t cmdlog_mtx = PTHREAD_MUTEX_INITIALIZER;
static CMDLOG_ENTRY *cmdlog[LOG_LINES];
static int cmdlogi = 0;

static void log_string(THREAD_DATA *td, OP_LEVEL lvl, char *str);

static FILE *g_logfile = NULL;

void
log_init()
{
	for (int i = 0; i < LOG_LINES; ++i) {
		log[i] = (LOG_ENTRY*)xzmalloc(sizeof(LOG_ENTRY));
	}

	for (int i = 0; i < CMD_LOG_LINES; ++i) {
		cmdlog[i] = (CMDLOG_ENTRY*)xzmalloc(sizeof(CMDLOG_ENTRY));
	}
}

void
log_shutdown()
{
	for (int i = 0; i < LOG_LINES; ++i) {
		free(log[i]);
	}

	for (int i = 0; i < CMD_LOG_LINES; ++i) {
		free(cmdlog[i]);
	}

	if (g_logfile) {
		fclose(g_logfile);
	}

	pthread_mutex_destroy(&log_mtx);
}

void
Log(OP_LEVEL lvl, char *str)
{
	THREAD_DATA *td = get_thread_data();

	log_string(td, lvl, str);
}

void
LogFmt(OP_LEVEL lvl, char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(buf, 256, fmt, ap);
	Log(lvl, buf); 
	va_end(ap);
}

void
log_logcmd(char *botname, char *arena, char *player, OP_LEVEL level, char *str)
{
	pthread_mutex_lock(&cmdlog_mtx);
	CMDLOG_ENTRY *ce = cmdlog[cmdlogi];
	ce->lvl = level;
	snprintf(ce->text, 256, "\"%s\" to \"%s\" in %s: %s", player, botname, arena, str);
	Log(ce->lvl, ce->text);
	cmdlogi = (cmdlogi + 1) % CMD_LOG_LINES;
	pthread_mutex_unlock(&cmdlog_mtx);
}

void
log_string(THREAD_DATA *td, OP_LEVEL lvl, char *str)
{

	time_t t = time(NULL);

	struct tm ts;
	memset(&ts, 0, sizeof(struct tm));
	localtime_r(&t, &ts);

	pthread_mutex_lock(&log_mtx);
	log[logi]->lvl = lvl;
	/* originally LOG_ENTRY contained a *tm struct, but since log prints
	 * the line anyway it would still need to process the format string.
	 * better to do it only once here than at both cmd_log() and here.
	 */
	if (td) {
		snprintf(log[logi]->text, 256, "[%d/%d %2d:%02d:%02d] %s\\%s: %s",
		    ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec,
		    td->bot_name, (*td->arena->name) ? td->arena->name : "None", str);
	} else {
		/* td can be null because it can be called by the core */
		snprintf(log[logi]->text, 256, "[%d/%d %2d:%02d:%02d] Core: %s",
		    ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec,
		    str);
	}
	//printf("%s\n", log[logi]->text);
	if (g_logfile == NULL) {
		g_logfile = fopen("opencore.log", "a+");
	}
	if (g_logfile) {
		fprintf(g_logfile, "%s\n", log[logi]->text);
		fflush(g_logfile);
	}
	logi = (logi + 1) % LOG_LINES;
	pthread_mutex_unlock(&log_mtx);
}

void
cmd_log(CORE_DATA *cd)
{
	int nlines = 0;
	pthread_mutex_lock(&log_mtx);
	for (int i = 0; i < LOG_LINES; ++i) {
		LOG_ENTRY *le = log[(logi + i) % LOG_LINES];

		if (le->text[0] != '\0' && cd->cmd_level >= le->lvl) {
			RmtMessageFmt(cd->cmd_name, "%s", le->text);
			nlines++;
		}
	}
	pthread_mutex_unlock(&log_mtx);

	if (nlines == 0) {
		RmtMessage(cd->cmd_name, "No entries in log");
	}
}

void
cmd_cmdlog(CORE_DATA *cd)
{
	int nlines = 0;
	pthread_mutex_lock(&cmdlog_mtx);
	for (int i = 0; i < LOG_LINES; ++i) {
		CMDLOG_ENTRY *ce = cmdlog[(cmdlogi + i) % CMD_LOG_LINES];

		if (ce->text[0] != '\0' && (cd->cmd_level == OP_OWNER || cd->cmd_level > ce->lvl)) {
			RmtMessageFmt(cd->cmd_name, "%s", ce->text);
			nlines++;
		}
	}
	pthread_mutex_unlock(&cmdlog_mtx);

	if (nlines == 0) {
		RmtMessage(cd->cmd_name, "No command entries in log");
	}
}

