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

#include <assert.h>
#include <ctype.h>
#include <ftw.h>
#include <libgen.h>
#include <stdarg.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>

#include "bsd/queue.h"

#include "botman.hpp"

#include "opencore.hpp"

#include "core.hpp"
#include "config.hpp"
#include "util.hpp"

#define FILE_EXTENSION ".conf"
#define TYPE_DIR "./types"

typedef struct type_entry TYPE_ENTRY;
struct type_entry
{
	LIST_ENTRY(type_entry) entry;

	char	type[16];		/* bot type */
	char	configfile[256];	/* config file */
	char	description[256];	/* description */
	int	nbots;			/* number of bots alive of this type */
	uint32_t used_mask;		/* bit mask of bots used */
}; 

LIST_HEAD(type_head, type_entry);

typedef struct bot_entry BOT_ENTRY;
struct bot_entry
{
	LIST_ENTRY(bot_entry) entry;

	char		type[16];	/* the bots type */
	char		bot_name[24];	/* bot name */
	char		owner[24];	/* bot owner */
	pthread_t	pid;		/* bots process id */
	int		bot_suffix;	/* bot's suffix */
	char		initial_arena[16];	/* initial arena the bot was started in */

	struct {
		pthread_mutex_t mtx;
		ticks_ms_t	last_checkin;
		int		should_exit;	/* set to 1 if the bot should exit */
	} shared_data[1];
}; 

LIST_HEAD(bot_head, bot_entry);

static bot_head		bot_list_head;
static pthread_mutex_t	bot_list_mtx = PTHREAD_MUTEX_INITIALIZER;
static type_head	type_list_head;
static pthread_mutex_t	type_list_mtx = PTHREAD_MUTEX_INITIALIZER;

static TYPE_ENTRY* find_type_entry_nr(char *type);
static void load_thread_config(THREAD_DATA *td, char *configfile);

static
bool
is_valid_typename(char *type)
{
	if (*type == '\0')
		return false;

	if (strlen(type) >= 16)
		return false;

	for (int i = 0; type[i] != '\0'; ++i) {
		if (isalpha(type[i]) == 0) {
			return false;
		}
	}

	return true;
}

static
int
get_next_free_num(int used_mask)
{
	int next = 1;

	int n = 1;
	while (next & used_mask) {
		next <<= 1;
		++n;
	}

	/* more than 32 bots exist */
	if (next == 0) {
		return 0;
	}

	return n;
}

int
botman_bot_shouldstop(void *bh)
{
	BOT_ENTRY *be = (BOT_ENTRY*)bh;
	pthread_mutex_lock(&be->shared_data->mtx);
	int ret = be->shared_data->should_exit;
	pthread_mutex_unlock(&be->shared_data->mtx);
	return ret;
}

char*
StartBot(char *type, char *arena, char *owner)
{
	assert(type != NULL && arena != NULL);

	if (is_valid_typename(type) == false) {
		return "Invalid type name";
	}

	char configfile[256];

	pthread_mutex_lock(&type_list_mtx);
	TYPE_ENTRY *te = find_type_entry_nr(type);
	if (te != NULL) {
		strncpy(configfile, te->configfile, 256);
		configfile[256 - 1] = '\0';
	}
	pthread_mutex_unlock(&type_list_mtx);

	if (te == NULL) {
		return "No such bot type";
	}

	int op_level = owner ? GetOpLevel(owner) : 9;

	/* load configurables */
	int reqlevelpub = get_config_int("bot.reqlevelpub", 2, configfile);
	int reqlevelsub = get_config_int("bot.reqlevelsub", 2, configfile);
	int reqlevelpriv = get_config_int("bot.reqlevelpriv", 2, configfile);
	int maxbots = get_config_int("login.maxbots", 0, configfile);

	/* check for the owners proper access levels */
	if (IsPub(arena) && reqlevelpub > op_level) {
		return "Not enough access to start bot in pub";
	} else if (IsPriv(arena) && reqlevelpriv > op_level) {
		return "Not enough access to start bot in priv";
	} else if (reqlevelsub > op_level) {
		return "Not enough access to start bot in subarena";
	}

	/* this is expensive, so do this allocation outside of mutex locking */
	THREAD_DATA *td = (THREAD_DATA*)malloc(sizeof(THREAD_DATA));
	memset(td, 0, sizeof(THREAD_DATA));
	load_thread_config(td, configfile);
	strlcpy(td->login->arenaname, arena, 16);
	strlcpy(td->bot_type, type, 16);

	/* check to make sure too many bots dont already exist */
	pthread_mutex_lock(&type_list_mtx);
	te = find_type_entry_nr(type);
	if (maxbots > 0 && te->nbots >= maxbots) {
		pthread_mutex_unlock(&type_list_mtx);
		free(td);
		return "Too many bots of that type are already running";
	} else if (maxbots == 0 && te->nbots >= 1) {
		pthread_mutex_unlock(&type_list_mtx);
		free(td);
		return "That bot is already active";
	}
	int bot_suffix = 0;	/* bot suffix */
	if (maxbots > 0) {
		/* generate the bots name */
		bot_suffix = get_next_free_num(te->used_mask);
		if (bot_suffix == 0) {
			pthread_mutex_unlock(&type_list_mtx);
			return "Max bot names reached";
		}
		te->used_mask |= (te->used_mask | (1 << (bot_suffix - 1)));
	} 
	te->nbots += 1;
	pthread_mutex_unlock(&type_list_mtx);
	if (maxbots > 0) {
		/* adjust bot login name if necessary */
		char botname[24];
		snprintf(botname, 24, "%s-%d", td->bot_name, bot_suffix);
		strlcpy(td->bot_name, botname, 24);
	}

	/* bot is ready to start */
	BOT_ENTRY *be = (BOT_ENTRY*)malloc(sizeof(BOT_ENTRY));
	memset(be, 0, sizeof(BOT_ENTRY));
	
	/* init bot entry fields */
	strlcpy(be->bot_name, td->bot_name, 24);
	strlcpy(be->type, type, 16);
	be->bot_suffix = bot_suffix;
	td->botman_handle = be;
	pthread_mutex_init(&be->shared_data->mtx, 0);
	be->shared_data->last_checkin = get_ticks_ms();
	strlcpy(be->initial_arena, arena, 16);
	strlcpy(be->owner, owner, 24);

	pthread_mutex_lock(&bot_list_mtx);
	if (pthread_create(&be->pid, NULL, BotEntryPoint, td) == 0) {
		LIST_INSERT_HEAD(&bot_list_head, be, entry);
	} else {
		pthread_mutex_unlock(&bot_list_mtx);
		free(td);
		pthread_mutex_destroy(&be->shared_data->mtx);
		free(be);
		return "Bot was unable to start correctly";
	}
	pthread_mutex_unlock(&bot_list_mtx);

	return NULL;
}

static
void
empty_types_list()
{
	pthread_mutex_lock(&type_list_mtx);
	TYPE_ENTRY *te;
	while (LIST_EMPTY(&type_list_head) == 0) {
		te = LIST_FIRST(&type_list_head);
		LIST_REMOVE(te, entry);
		free(te);
	}
	pthread_mutex_unlock(&type_list_mtx);
}


static
void
free_bot_slot(char *type, int suffix)
{
	pthread_mutex_lock(&type_list_mtx);
	TYPE_ENTRY *te = find_type_entry_nr(type);
	if (te != NULL) {
		if (suffix > 0) {
			te->used_mask &= ~(te->used_mask | (1 << (suffix - 1)));
		}
		te->nbots -= 1;	
	}
	pthread_mutex_unlock(&type_list_mtx);
}

void
botman_stop_all_bots()
{
	pthread_mutex_lock(&bot_list_mtx);
	BOT_ENTRY *be;
	LIST_FOREACH(be, &bot_list_head, entry) {
		pthread_mutex_lock(&be->shared_data->mtx);
		be->shared_data->should_exit = 1;
		pthread_mutex_unlock(&be->shared_data->mtx);
	}
	pthread_mutex_unlock(&bot_list_mtx);
}

void
botman_bot_exiting(void *bh)
{
	BOT_ENTRY *be = (BOT_ENTRY*)bh;

	pthread_mutex_lock(&bot_list_mtx);
	LIST_REMOVE(be, entry);
	pthread_mutex_unlock(&bot_list_mtx);

	free_bot_slot(be->type, be->bot_suffix);

	pthread_mutex_destroy(&be->shared_data->mtx);
	free(be);
}

void
botman_bot_checkin(void *bh)
{
	BOT_ENTRY *be = (BOT_ENTRY*)bh;

	ticks_ms_t ticks = get_ticks_ms();

	pthread_mutex_lock(&be->shared_data->mtx);
	be->shared_data->last_checkin = ticks;
	pthread_mutex_unlock(&be->shared_data->mtx);
}

static
TYPE_ENTRY*
find_type_entry_nr(char *type)
{
	TYPE_ENTRY *te;
	LIST_FOREACH(te, &type_list_head, entry) {
		if (strcasecmp(te->type, type) == 0) {
			return te;
		}
	}

	return NULL;
}

static
char *last_str(char *haystack, char *needle)
{
	char *p = strstr(haystack, needle);
	if (p == NULL) return NULL;

	char *q;
	while ((q = strstr(&p[1], needle)) != NULL) {
		p = q;
	}

	return p;
}

static
void
load_thread_config(THREAD_DATA *td, char *configfile)
{
	struct THREAD_DATA::net_t *n = td->net;

	get_config_string("login.username", td->bot_name, 24, "*", configfile);
	get_config_string("login.password", td->login->password, 32, "", configfile);
	get_config_string("login.autorun", td->login->autorun, 256, "", configfile);
	get_config_string("login.chats", td->login->chats, 256, "", configfile);

	get_config_string("core.libraries", td->libstring, 512, "", configfile);

	get_config_string("net.servername", n->servername, 128, "*", configfile);
	get_config_string("net.serverport", n->serverport, 8, "65000", configfile);


	td->periodic->info = get_config_int("periodic.infoseconds", 0, configfile);
	if (td->periodic->info && td->periodic->info < 60) {
		td->periodic->info = 60;
	}
	td->periodic->info *= 1000;

	td->periodic->einfo = get_config_int("periodic.einfoseconds", 0, configfile);
	if (td->periodic->einfo && td->periodic->einfo < 60) {
		td->periodic->einfo = 60;
	}
	td->periodic->einfo *= 1000;


	td->debug->spew_packets =
	    get_config_int("debug.spewpackets", 0, configfile);
	td->debug->show_unhandled_packets = 
	    get_config_int("debug.showuhandledpackets", 0, configfile);

	td->enter->send_watch_damage =
	    (uint8_t)get_config_int("enter.sendwatchdamage", 0, configfile);
	td->enter->send_einfo =
	    (uint8_t)get_config_int("enter.sendeinfo", 0, configfile);
	td->enter->send_info =
	    (uint8_t)get_config_int("enter.sendinfo", 0, configfile);
}

/*
 * Called for each file in the configuration directory, this
 * tests to make sure the file has a valid name and then
 * adds it to the type list if it does.
 */
int
ftw_cb(const char *fpath, const struct stat *sb, int typeflag)
{
	if (typeflag != FTW_F) return 0;

	/* verify that file ends in the extension */
	char *ext = last_str((char*)fpath, FILE_EXTENSION);
	if (ext == NULL) return 0;
	if (ext[strlen(FILE_EXTENSION)] != '\0') return 0;


	TYPE_ENTRY *te = (TYPE_ENTRY*)malloc(sizeof(TYPE_ENTRY));
	memset(te, 0, sizeof(TYPE_ENTRY));
	
	/* init type entry fields */
	char *tmp = strdup(fpath);
	char *base = basename(tmp);
	ext = last_str(base, FILE_EXTENSION);
	char buf[16];
	memset(buf, 0, 16);
	int len = ext - base;
	if (len > 15) return 0;
	strncpy(buf, base, ext - base);
	snprintf(te->type, 16, "%s", buf);
	free(tmp);

	snprintf(te->configfile, 256, "%s", fpath);
	get_config_string("bot.description", te->description, 256, "No description", (char*)fpath);

	/* add entry to list */
	pthread_mutex_lock(&type_list_mtx);
	/* make sure the type doesnt already exist in the list */
	TYPE_ENTRY *entry_exists = find_type_entry_nr(te->type);
	if (entry_exists == NULL) {
		LIST_INSERT_HEAD(&type_list_head, te, entry);
	}
	pthread_mutex_unlock(&type_list_mtx);

	if (entry_exists) {
		LogFmt(OP_MOD, "Unable to update type: %s", te->type);
		free(te);
	} else {
		LogFmt(OP_MOD, "Added type: %s", te->type);
	}

	return 0;
}

void
sigint_handler(int signum)
{
	printf("\nExiting due to SIGINT...\n");
	_exit(0);
}

void*
thread_checker(void *)
{
	for (;;) {
		ticks_ms_t ticks = get_ticks_ms();

		pthread_mutex_lock(&bot_list_mtx);
		BOT_ENTRY *be, *tmp;
		LIST_FOREACH_SAFE(be, &bot_list_head, entry, tmp) {

			pthread_mutex_lock(&be->shared_data->mtx);
			ticks_ms_t last_checkin = be->shared_data->last_checkin;
			pthread_mutex_unlock(&be->shared_data->mtx);

			/* verify that the bot has checked in */
			if (ticks - last_checkin > BOTMAN_CHECKIN_INTERVAL * 3) {
				LIST_REMOVE(be, entry);

				LogFmt(OP_MOD, "Removing dead bot (No checkin for %d seconds): %s\n",
				    (ticks - last_checkin)/1000, be->bot_name);

				pthread_cancel(be->pid);
				free_bot_slot(be->type, be->bot_suffix);
				pthread_mutex_destroy(&be->shared_data->mtx);
				free(be);
			}
		}
		pthread_mutex_unlock(&bot_list_mtx);

		sleep(5);
	}

	return NULL;
}

void
botman_mainloop()
{
	signal(SIGINT, sigint_handler);

	pthread_t unused;
	pthread_create(&unused, NULL, thread_checker, NULL);

	bool running = true;
	do {
		BOT_ENTRY *be;
		pthread_t pid;
		pthread_mutex_lock(&bot_list_mtx);
		be = LIST_FIRST(&bot_list_head);
		if (be) {
			pid = be->pid;
		} else {
			running = false;
		}
		pthread_mutex_unlock(&bot_list_mtx);

		if (running) {
			pthread_join(pid, NULL);

			/* a bot exited, remove its thread entry */
			pthread_mutex_lock(&bot_list_mtx);
			BOT_ENTRY *tmp;
			LIST_FOREACH_SAFE(be, &bot_list_head, entry, tmp) {
				if (pid == be->pid) {
					LIST_REMOVE(be, entry);
					pthread_mutex_destroy(&be->shared_data->mtx);
					free(be);
				}
			}
			pthread_mutex_unlock(&bot_list_mtx);
		}
	} while (running);
}

static
void
load_bottypes()
{
	ftw(TYPE_DIR, ftw_cb, 1);
}

void
cmd_listbots(CORE_DATA *cd)
{
	pthread_mutex_lock(&bot_list_mtx);
	
	ticks_ms_t ticks = get_ticks_ms();

	BOT_ENTRY *be;
	LIST_FOREACH(be, &bot_list_head, entry) {
		pthread_mutex_lock(&be->shared_data->mtx);
		RmtMessageFmt(cd->cmd_name, "Type:%s LastCheckedIn:%ds Name:%s Arena:%s Owner:%s",
		    be->type, (ticks - be->shared_data->last_checkin) / 1000, be->bot_name,
		    be->initial_arena, be->owner ? be->owner : "(null)");
		pthread_mutex_unlock(&be->shared_data->mtx);
	}
	pthread_mutex_unlock(&bot_list_mtx);
}

void
botman_init()
{
	LIST_INIT(&type_list_head);
	LIST_INIT(&bot_list_head);

	load_bottypes();
}

void
botman_shutdown()
{
	empty_types_list();

	BOT_ENTRY *be;
	while (LIST_EMPTY(&bot_list_head) == 0) {
		be = LIST_FIRST(&bot_list_head);
		LIST_REMOVE(be, entry);

		pthread_cancel(be->pid);
		pthread_mutex_destroy(&be->shared_data->mtx);
		free(be);
	}

	pthread_mutex_destroy(&bot_list_mtx);
	pthread_mutex_destroy(&type_list_mtx);
}


void
cmd_loadtypes(CORE_DATA *cd)
{
	LogFmt(OP_SMOD, "Types list reloaded by %s", cd->cmd_name);

	pthread_mutex_lock(&type_list_mtx);

	/* erase unused bot entries */
	TYPE_ENTRY *te, *tmp;
	LIST_FOREACH_SAFE(te, &type_list_head, entry, tmp) {
		if (te->nbots == 0) {
			LIST_REMOVE(te, entry);
		} else {
			ReplyFmt("Couldn't reload %s type: %d bots still in use", te->type, te->nbots);
		}
	}

	pthread_mutex_unlock(&type_list_mtx);

	Reply("Types list reloaded");
}

void
cmd_types(CORE_DATA *cd)
{
	pthread_mutex_lock(&type_list_mtx);
	TYPE_ENTRY *te;
	int ntypes = 0;
	LIST_FOREACH(te, &type_list_head, entry) {
		if (ntypes == 0) {
			Reply("Startable bot types:");
		}
		ReplyFmt("%-12s %s", te->type, te->description);
		++ntypes;
	}
	pthread_mutex_unlock(&type_list_mtx);

	if (ntypes == 0) {
		Reply("No types");
	}
}

