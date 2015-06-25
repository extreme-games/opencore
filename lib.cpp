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

/* FIXME: make it so td2cd() is unnecessary; keep module data in core_data
   instead of duplicating it */

#include <assert.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "bsd/queue.h"

#include "lib.hpp"

#include "opencore.hpp"

#include "cmd.hpp"
#include "log.hpp"
#include "msg.hpp"
#include "player.hpp"
#include "util.hpp"


typedef struct timer_entry TIMER_ENTRY;
struct timer_entry 
{
	LIST_ENTRY(timer_entry) entry;

	void		*data1;		/* user data 1 */
	void		*data2;		/* user data 2 */

	ticks_ms_t	 base;		/* when the timer was set */
	ticks_ms_t	 duration;	/* the duration */

	long		 id;		/* the id of the timer */
};

LIST_HEAD(timer_head, timer_entry);

struct lib_entry
{
	LIST_ENTRY(lib_entry) entry;

	void	 *handle;		/* handle to the library from dlopen() */
	char	  libname[64];		/* library name */

					/* these are filled in by register_library() */
	char	  intname[16];		/* internal name */
	char	  author[16];		/* authors name */
	char	  version[8];		/* version number */
	char	  date[24];		/* build date */
	char	  time[24];		/* build time */
	char	  description[64];	/* library description */

	int	  pinfo_size;		/* size of each individuals player info */
	void	**pinfo_base;		/* array of player-specific entries */

	void	 *user_data;		/* pointer to module-specific data */

	timer_head	timer_list_head;	/* list of library-owned timers */
	int		timer_id;		/* the next timer id to be used */

	GameEvent_cb GameEvent;		/* entry point to the dll */
}; 

LIST_HEAD(lib_head, lib_entry);

typedef
struct MOD_TL_DATA_
{
	lib_head	lib_list_head;	/* head of the list of loaded libraries */

	int	 	pinfo_nslots;	/* number of total pinfo slots allocated */

	CORE_DATA	cd;		/* core_data exported to modules */
} MOD_TL_DATA;

static LIB_ENTRY* find_lib_entry(MOD_TL_DATA *mod_tld, char *libname);
static int	unload_library(THREAD_DATA *td, char *libname);
static void	expire_timers_lib(THREAD_DATA *td, LIB_ENTRY *le, int expire_all);

CORE_DATA*
libman_get_core_data(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	return &mod_tld->cd;
}

static inline
void
cd2le(LIB_ENTRY *le, CORE_DATA *cd)
{
	le->user_data = cd->user_data;
}

static inline
void
le2cd(CORE_DATA *cd, LIB_ENTRY *le)
{
	cd->pinfo_base = le->pinfo_base;
	cd->user_data = le->user_data;
}

static inline
void
td2cd(CORE_DATA *cd, THREAD_DATA *td)
{
	cd->parray = player_get_parray(td);
	cd->phere = player_get_phere(td);

	cd->bot_pid = td->bot_pid;
	cd->bot_ship = td->bot_ship;
	cd->bot_freq = td->bot_freq;
	cd->bot_name = td->bot_name;
	cd->bot_arena = td->arena->name;

	cd->here_first = td->arena->here_first;
	cd->here_last = td->arena->here_last;
	cd->here_count = td->arena->here_count;

	cd->ship_first = td->arena->ship_first;
	cd->ship_last = td->arena->ship_last;
	cd->ship_count = td->arena->ship_count;

	cd->spec_first = td->arena->spec_first;
	cd->spec_last = td->arena->spec_last;
	cd->spec_count = td->arena->spec_count;
}

static
inline
void
try_set_pinfo(CORE_DATA *cd, LIB_ENTRY *le) {
	cd->p1_pinfo = NULL;
	cd->p2_pinfo = NULL;
	if (cd && cd->pinfo_base && cd->parray) {
		cd->p1_pinfo = GetPlayerInfo(cd, cd->p1);
		cd->p2_pinfo = GetPlayerInfo(cd, cd->p2);
	}
}

void
libman_export_command(THREAD_DATA *td, LIB_ENTRY *le, CORE_DATA *cd, Command_cb func)
{
	td2cd(cd, td);

	/* the core has commands so le is not always valid, this should be changed */
	if (le) {
		le2cd(cd, le);
		try_set_pinfo(cd, le);
	}

	cd->event = EVENT_COMMAND;

	td->lib_entry = le;
	func(cd);
	td->lib_entry = NULL;

	if (le)
		cd2le(le, cd);
}

void
libman_export_event_lib(THREAD_DATA *td, int event, CORE_DATA *cd, LIB_ENTRY *le)
{
	if (cd == NULL)
		cd = libman_get_core_data(td);

	td2cd(cd, td);
	le2cd(cd, le);
	try_set_pinfo(cd, le);

	cd->event = event;

	td->lib_entry = le;
	le->GameEvent(cd);
	td->lib_entry = NULL;

	cd2le(le, cd);
}

/*
 * This does not call export_event_lib to prevent unnecessary calls
 * to td2cd.
 */
void
libman_export_event(THREAD_DATA *td, int event, CORE_DATA *cd)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	if (cd == NULL)
		cd = libman_get_core_data(td);

	/* set thread-specific data into core_data */
	td2cd(cd, td);

	cd->event = event;

	LIB_ENTRY *le;
	LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		/* set lib-specific data into core_data */
		le2cd(cd, le);
		try_set_pinfo(cd, le);

		td->lib_entry = le;
		le->GameEvent(cd);
		td->lib_entry = NULL;

		cd2le(le, cd);
	}
}

void
libman_zero_pinfo(THREAD_DATA *td, PLAYER *p)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	PLAYER *parray = player_get_parray(td);

	/* zero players pinfo entry in each library */
        LIB_ENTRY *le;
	void *pinfo;
        LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		pinfo = le->pinfo_base[p - parray];
		memset(pinfo, 0, le->pinfo_size);
	}   
}

void
libman_realloc_pinfo_array(THREAD_DATA *td, int new_count)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	assert(mod_tld->pinfo_nslots <= new_count);

        LIB_ENTRY *le;
        LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		le->pinfo_base = (void**)realloc(le->pinfo_base, new_count * sizeof(void*));

		for (int i = mod_tld->pinfo_nslots; i < new_count; ++i)
			le->pinfo_base[i] = malloc(le->pinfo_size);
	}   

	mod_tld->pinfo_nslots = new_count;
}

void
libman_move_pinfo_entry(THREAD_DATA *td, int dest_index, int source_index)
{
        assert(dest_index <= source_index);

	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

        LIB_ENTRY *le;
        LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
                memcpy(le->pinfo_base + (dest_index * sizeof(void*)),
                    le->pinfo_base + (source_index * sizeof(void*)),
                    le->pinfo_size);
        }
}

/* this could probably be made prettier */
void
libman_load_library(THREAD_DATA *td, char *libname)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	LIB_ENTRY *le;
	char filename[64];
	strlcpy(filename, libname, 64);
	strlcat(filename, ".so", 64);

	void *handle = dlopen(filename, RTLD_NOW);
	if (handle) {
		LIB_DATA *ld = (LIB_DATA*)dlsym(handle, "REGISTRATION_INFO");

		if (ld == NULL) {
			LogFmt(OP_SMOD, "%s: Registration info not found", libname);
			dlclose(handle);
		} else if (strcmp(CORE_VERSION, ld->oc_version) != 0) {
			LogFmt(OP_SMOD, "%s: Core version mismatch, library is %s", libname, ld->oc_version);
			dlclose(handle);
		} else {
			le = (LIB_ENTRY*)xzmalloc(sizeof(LIB_ENTRY));

			le->handle = handle;

			le->GameEvent = ld->cb;

			strlcpy(le->libname, libname, 64);

			le->pinfo_size = ld->pinfo_size;

			strlcpy(le->intname, ld->name, 16);
			strlcpy(le->author, ld->author, 16);
			strlcpy(le->version, ld->version, 8);
			strlcpy(le->date, ld->date, 24);
			strlcpy(le->time, ld->time, 24);
			strlcpy(le->description, ld->description, 64);

			LIST_INIT(&le->timer_list_head);

			/* event_start */
			libman_export_event_lib(td, EVENT_START, NULL, le);

			/* setup pinfo */
			int nslots = mod_tld->pinfo_nslots;
			le->pinfo_base = (void**)calloc(nslots, sizeof(void*));
			for (int i = 0; i < nslots; ++i)
				le->pinfo_base[i] = malloc(le->pinfo_size);

			/* setup parray */
			PLAYER *parray = player_get_parray(td);
			int phere = player_get_phere(td);
			PLAYER *p;
			/* if players are here (module was loaded while core
			   was connected, export events for them */
			for (int i = 0; i < phere; ++i) {
				p = &parray[i];
				CORE_DATA *cd = libman_get_core_data(td);

				cd->p1 = p;
				libman_export_event_lib(td, EVENT_FIRSTSEEN, cd, le);
				libman_export_event_lib(td, EVENT_ENTER, cd, le);
			}

			LogFmt(OP_MOD, "%s: Loaded successfully", libname);
			LIST_INSERT_HEAD(&mod_tld->lib_list_head, le, entry);
		}
	} else {
		LogFmt(OP_MOD, "%s: Error loading file: %s", libname, dlerror());
	}
}

void
KillTimer(long timer_id)
{
	THREAD_DATA *td = get_thread_data();
	LIB_ENTRY *le = (LIB_ENTRY*)td->lib_entry;

	TIMER_ENTRY *te, *tmp;
        LIST_FOREACH_SAFE(te, &le->timer_list_head, entry, tmp) {
		if (te->id == timer_id) {
			LIST_REMOVE(te, entry);
			free(te);
			return;
		}
        }
}

long
SetTimer(ticks_ms_t duration, void *data1, void *data2)
{
	THREAD_DATA *td = get_thread_data();
	LIB_ENTRY *le = (LIB_ENTRY*)td->lib_entry;

	TIMER_ENTRY *te = (TIMER_ENTRY*)malloc(sizeof(TIMER_ENTRY));

	ticks_ms_t ticks = get_ticks_ms();

	te->data1 = data1;
	te->data2 = data2;
	te->duration = duration;
	te->base = ticks;
	te->id = ++le->timer_id;

	LIST_INSERT_HEAD(&le->timer_list_head, te, entry);

	return te->id;
}

void
libman_expire_timers(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

        LIB_ENTRY *le;
        LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		expire_timers_lib(td, le, 0);
        }
	
}

static
void
expire_timers_lib(THREAD_DATA *td, LIB_ENTRY *le, int expire_all)
{
	ticks_ms_t ticks = get_ticks_ms();

	TIMER_ENTRY *te, *nxt;
	for (te = LIST_FIRST(&le->timer_list_head); te != NULL; te = nxt) {
		nxt = LIST_NEXT(te, entry);
		if (ticks - te->base >= te->duration || expire_all) {
			LIST_REMOVE(te, entry);

			CORE_DATA *cd = libman_get_core_data(td);

			cd->timer_data1 = te->data1;
			cd->timer_data2 = te->data2;
			cd->timer_id = te->id;

			libman_export_event_lib(td, EVENT_TIMER, cd, le);

			free(te);
		}
	}
}

void
libman_instance_init(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)xzmalloc(sizeof(MOD_TL_DATA));

	td->mod_data->lib = mod_tld;

	LIST_INIT(&mod_tld->lib_list_head);


	CORE_DATA *cd = &mod_tld->cd;

	cd->bot_pid = PID_NONE;
}

void
cmd_inslib(CORE_DATA *cd)
{
	THREAD_DATA *td = (THREAD_DATA*)get_thread_data();
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	if (cd->cmd_argc != 2) {
		RmtMessageFmt(cd->cmd_name, "Usage: %s <lib name>",
		    cd->cmd_argv[0]);
		return;
	}

	char *libname = cd->cmd_argv[1];
	LIB_ENTRY *le = find_lib_entry(mod_tld, libname);

	if (le == NULL) {
		libman_load_library(td, libname);
		if (find_lib_entry(mod_tld, libname) != NULL)
			RmtMessageFmt(cd->cmd_name, "Library loaded");
		else
			RmtMessageFmt(cd->cmd_name, "Library failed to load");
	} else
		RmtMessageFmt(cd->cmd_name, "Library is already loaded");
}

void
cmd_rmlib(CORE_DATA *cd)
{
	THREAD_DATA *td = (THREAD_DATA*)get_thread_data();

	if (cd->cmd_argc == 2) {
		if (unload_library(td, cd->cmd_argv[1]) == 0)
			RmtMessage(cd->cmd_name, "Library unloaded");
		else
			RmtMessage(cd->cmd_name, "Library is not loaded");
	} else
		RmtMessageFmt(cd->cmd_name, "Usage: %s <lib name>", cd->cmd_argv[0]);
}

static
LIB_ENTRY*
find_lib_entry(MOD_TL_DATA *mod_tld, char *libname)
{
	LIB_ENTRY *le;
	LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		if (strcasecmp(le->libname, libname) == 0)
			return le;
	}

	return NULL;
}

void
cmd_about(CORE_DATA *cd)
{
	THREAD_DATA *td = (THREAD_DATA*)get_thread_data();
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	RmtMessageFmt(cd->cmd_name, "opencore %s (%s %s) Copyright 2006-2015 cycad <cycad@greencams.net>",
	    CORE_VERSION, __DATE__, __TIME__);

	RmtMessage(cd->cmd_name, "");

	if (LIST_EMPTY(&mod_tld->lib_list_head) != 0)
		RmtMessage(cd->cmd_name, "No libraries are loaded");

	LIB_ENTRY *node;
	LIST_FOREACH(node, &mod_tld->lib_list_head, entry) {
		RmtMessageFmt(cd->cmd_name, "%-16.16s %-8.8s %s %s by %-16.16s  %s",
		    node->libname,
		    node->version,
		    node->date,
		    node->time,
		    node->author,
		    node->description
		    );
	}
}

static
int
unload_library(THREAD_DATA *td, char *libname)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	LIB_ENTRY *le = find_lib_entry(mod_tld, libname);
	if (le == NULL)
		return -1;

	/* expire timers */
	expire_timers_lib(td, le, 1);

	/* generate deadslot event */
	PLAYER *parray = player_get_parray(td);
	int phere = player_get_phere(td);
	for (int i = 0; i < phere; ++i) {
		/* event_deadslot */
		CORE_DATA *cd = libman_get_core_data(td);
		cd->p1 = &parray[i];

		libman_export_event_lib(td, EVENT_DEADSLOT, cd, le);
	}

	/* event_stop */
	libman_export_event_lib(td, EVENT_STOP, NULL, le);

	/* free pinfo */
	for (int i = 0; i < mod_tld->pinfo_nslots; ++i)
		free(le->pinfo_base[i]);
	free(le->pinfo_base);
		
	dlclose(le->handle);

	unregister_commands(td, le);

	LIST_REMOVE(le, entry);
	free(le);

	return 0;
}

void
libman_instance_shutdown(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	LIB_ENTRY *le;
	while (LIST_EMPTY(&mod_tld->lib_list_head) == 0) {
		le = LIST_FIRST(&mod_tld->lib_list_head);
		unload_library(td, le->libname);
	}

	free(mod_tld);
}

