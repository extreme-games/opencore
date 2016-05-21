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
#include <Python.h>

#include "lib.hpp"

#include "opencore.hpp"

#include "cmd.hpp"
#include "log.hpp"
#include "msg.hpp"
#include "player.hpp"
#include "util.hpp"

#include "opencore_swig.hpp"

typedef struct timer_entry TIMER_ENTRY;
struct timer_entry 
{
	LIST_ENTRY(timer_entry) entry;

	uintptr_t	data1;		/* user data 1 */
	uintptr_t	data2;		/* user data 2 */

	ticks_ms_t	base;		/* when the timer was set */
	ticks_ms_t	duration;	/* the duration */

	long		id;		/* the id of the timer */
};

LIST_HEAD(timer_head, timer_entry);

#define TYPE_UNKNOWN 0
#define TYPE_NATIVE 1
#define TYPE_PYTHON 2

struct lib_entry
{
	LIST_ENTRY(lib_entry) entry;

	int       registered;		/* true once the library successfully calls RegisterPlugin() */

	char	  libname[64];		/* library name */

					/* these are filled in by register_library() */
	char	  intname[16];		/* internal name */
	char	  author[16];		/* authors name */
	char	  version[8];		/* version number */
	char	  date[24];		/* build date */
	char	  time[24];		/* build time */
	char	  description[64];	/* library description */

	size_t	  pinfo_size;		/* size of each individuals player info */
	void	**pinfo_base;		/* array of player-specific entries */

	size_t    userdata_size;	/* size of user_data pointer */
	void	 *user_data;		/* pointer to module-specific data */

	timer_head	timer_list_head;	/* list of library-owned timers */
	int		timer_id;		/* the next timer id to be used */

	int	  type;			/* TYPE_NATIVE, TYPE_PYTHON */
	union {
		struct {
			GameEvent_cb	 GameEvent;		/* entry point to the dll */
			void	 	*handle;		/* handle to the library from dlopen(), may be NULL if in the core */
		} native_data[1];

		struct {
			PyObject *pModule;
			PyObject *pGameEvent;	/* python callback */
		} python_data[1];
	};
}; 

LIST_HEAD(lib_head, lib_entry);

typedef struct user_call USER_CALL;
struct user_call {
	SIMPLEQ_ENTRY(user_call) entry;

	char libname[64];
	char functionname[32];
	char *arg;
};

SIMPLEQ_HEAD(user_call_head, user_call);

typedef struct user_event USER_EVENT;
struct user_event {
	SIMPLEQ_ENTRY(user_event) entry;

	char libname[64];
	char eventname[32];
	char *arg;
};

SIMPLEQ_HEAD(user_event_head, user_event);


typedef
struct MOD_TL_DATA_
{
	lib_head	lib_list_head;	/* head of the list of loaded libraries */

	int	 	pinfo_nslots;	/* number of total pinfo slots allocated */

	user_call_head usercall_list_head;
	user_event_head userevent_list_head;

	CORE_DATA	cd;		/* core_data exported to modules */
} MOD_TL_DATA;

static int	unload_library(THREAD_DATA *td, char *libname);
static void	expire_timers_lib(THREAD_DATA *td, LIB_ENTRY *le, int expire_all);
static void libman_export_userevents(THREAD_DATA *td);
static void libman_export_usercalls(THREAD_DATA *td);
static void libman_export_usereventsandcalls(THREAD_DATA *td);

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
	// nothing for now, since all input to the core is coming through functions rather than direct sets	
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
	cd->killer_pinfo = NULL;
	cd->killed_pinfo = NULL;
	if (cd && cd->pinfo_base && cd->parray && le->pinfo_size > 0) {
		if (cd->p1) cd->p1_pinfo = GetPlayerInfo(cd, cd->p1);
		if (cd->p2) cd->p2_pinfo = GetPlayerInfo(cd, cd->p2);
		if (cd->killer) cd->killer_pinfo = GetPlayerInfo(cd, cd->killer);
		if (cd->killed) cd->killed_pinfo = GetPlayerInfo(cd, cd->killed);
	}
}


void
RegisterPlugin(char *oc_version, char *plugin_name, char *author, char *version, char *date, char *time, char *description, size_t userdata_size, size_t pinfo_size)
{
	THREAD_DATA *td = get_thread_data();
	LIB_ENTRY *le = td->lib_entry;

	if (le->registered) StopBot("Bot attempted to register twice");
	if (strcmp(OPENCORE_VERSION, oc_version) != 0) {
		LogFmt(OP_SMOD, "%s: Core version mismatch, core is %s, library is %s", le->libname, OPENCORE_VERSION, version);
		return;
	}

	strlcpy(le->intname, plugin_name, 16);
	strlcpy(le->author, author, 16);
	strlcpy(le->version, version, 8);
	strlcpy(le->date, date, 24);
	strlcpy(le->time, time, 24);
	strlcpy(le->description, description, 24);

	/* set user_data and pinfo sizes as specified unless the bot's type is python */
	le->pinfo_size = le->type == TYPE_PYTHON ? sizeof(void*) : pinfo_size;
	le->userdata_size = le->type == TYPE_PYTHON ? sizeof(void*) : userdata_size;

	/* setup user_data here so its valid after the RegisterPlugin() call */
	le->user_data = xcalloc(1, le->userdata_size);
	CORE_DATA *cd = libman_get_core_data(td);
	cd->user_data = le->user_data;

	le->registered = 1;
}


static
void
export_event_lib(THREAD_DATA *td, int event, CORE_DATA *cd, LIB_ENTRY *le)
{
	// NOTE: this is special cased to only export start events to unregistered cores
	if (!le->registered && event != EVENT_START) return;

	if (cd == NULL) cd = libman_get_core_data(td);

	le2cd(cd, le);
	try_set_pinfo(cd, le);

	cd->event = event;

	td->lib_entry = le;

	if (le->type == TYPE_NATIVE) {
		le->native_data->GameEvent(cd);
	} else if (le->type == TYPE_PYTHON) {
		PyObject *pArgs = PyTuple_New(1);
		if (pArgs) {
			PyObject *pArg = wrap_old_core_data(cd);
			if (pArg) {
				PyTuple_SetItem(pArgs, 0, pArg);
				PyObject *pResult = PyObject_CallObject(le->python_data->pGameEvent, pArgs);
				if (pResult) Py_DECREF(pResult);
			} else { // couldnt make argument
				LogFmt(OP_MOD, "%s: Unable to make argument for event call", le->libname);
				PyErr_Print();
			}
			Py_DECREF(pArgs);
		} else { // couldnt create tuple for arguments
			LogFmt(OP_MOD, "%s: Unable to make tuple for argument", le->libname);
			PyErr_Print();
		}
	} else {
		assert(0);
	}
	td->lib_entry = NULL;

	cd2le(le, cd);
}


void
libman_export_event(THREAD_DATA *td, int event, CORE_DATA *cd, LIB_ENTRY *le)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	if (cd == NULL)
		cd = libman_get_core_data(td);

	/* set thread-specific data into core_data once */
	td2cd(cd, td);

	if (le) {
		export_event_lib(td, event, cd, le);
	} else {
		LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
			export_event_lib(td, event, cd, le);
		}
	}

	libman_export_usereventsandcalls(td);
}


void
libman_export_usereventsandcalls(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	int n = 0;
	static const int limit = 100;
	while (!SIMPLEQ_EMPTY(&mod_tld->usercall_list_head) && !SIMPLEQ_EMPTY(&mod_tld->userevent_list_head)) {
		libman_export_usercalls(td);
		libman_export_userevents(td);

		if (++n >= limit) {
			LogFmt(OP_SMOD, "Recursive userevent/usercall cycle detected, breaking at %d", limit);
			break;
		}
	}
}

void
libman_export_usercalls(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;
	CORE_DATA *cd = libman_get_core_data(td);

	USER_CALL *uc;
	while ((uc = SIMPLEQ_FIRST(&mod_tld->usercall_list_head)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&mod_tld->usercall_list_head, entry);
			LIB_ENTRY *le = libman_find_lib(uc->libname);
			if (le) {
				cd->usercall_functionname = uc->functionname;
				cd->usercall_arg = uc->arg;
				libman_export_event(td, EVENT_USER_CALL, cd, le);
			} else {
				LogFmt(OP_SMOD, "Unable to perform usercall %s.%s(\"%s\")", uc->libname, uc->functionname, uc->arg ? uc->arg : "NULL");
			}

			free(uc->arg);
			free(uc);
	}
}


void
libman_export_userevents(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;
	CORE_DATA *cd = libman_get_core_data(td);

	USER_EVENT *ue;
	while ((ue = SIMPLEQ_FIRST(&mod_tld->userevent_list_head)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&mod_tld->userevent_list_head, entry);

			cd->userevent_libname = (char*)ue->libname;
			cd->userevent_eventname = (char*)ue->eventname;
			cd->userevent_arg = ue->arg;
			libman_export_event(td, EVENT_USER_EVENT, cd, NULL);

			free(ue->arg);
			free(ue);
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
		if (le->pinfo_size > 0) {
			pinfo = le->pinfo_base[p - parray];
			memset(pinfo, 0, le->pinfo_size);
		}
	}   
}

void
libman_realloc_pinfo_array(THREAD_DATA *td, int new_count)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	assert(mod_tld->pinfo_nslots <= new_count);

        LIB_ENTRY *le;
        LIST_FOREACH(le, &mod_tld->lib_list_head, entry) {
		if (le->pinfo_size > 0) {
			le->pinfo_base = (void**)realloc(le->pinfo_base, new_count * sizeof(void*));

			for (int i = mod_tld->pinfo_nslots; i < new_count; ++i)
				le->pinfo_base[i] = malloc(le->pinfo_size);
		}
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
		if (le->pinfo_size > 0) {
			memcpy(le->pinfo_base + (dest_index * sizeof(void*)),
			    le->pinfo_base + (source_index * sizeof(void*)),
			    le->pinfo_size);
		}
        }
}


void
libman_load_library(THREAD_DATA *td, char *libname)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	LIB_ENTRY *le;
	bool core_load = libname == NULL;
	if (!libname) libname = CORE_NAME;

	le = (LIB_ENTRY*)xzmalloc(sizeof(LIB_ENTRY));
	strlcpy(le->libname, libname, 64);
	LIST_INIT(&le->timer_list_head);
	le->type = TYPE_UNKNOWN;

	// try to load as native
	void *handle = NULL;
	if (!core_load) {
		char filename[64] = { 0 };
		snprintf(filename, sizeof(filename)-1, "%s.so", libname);
		handle = dlopen(filename, RTLD_NOW);
	}
	if (handle || core_load) {
		GameEvent_cb sym = core_load ? GameEvent : (GameEvent_cb)dlsym(handle, "GameEvent");
		if (sym) {
			le->type = TYPE_NATIVE;
			le->native_data->GameEvent = sym;
			le->native_data->handle = handle;
		} else {
			if (handle) dlclose(handle);
		}
	}

	if (core_load && le->type != TYPE_NATIVE) {
		// the core can only be native, fail
		Log(OP_MOD, "Unable to load core");
		free(le);
		return;
	}

	if (le->type == TYPE_UNKNOWN) { // still unloaded, try to load as python
		PyObject *pName = PyString_FromString(libname);
		PyObject *pModule = PyImport_Import(pName);
		Py_DECREF(pName);

		if (pModule) {
			PyObject *pGameEvent = PyObject_GetAttrString(pModule, "GameEvent");

			if (pGameEvent) {
				if (PyCallable_Check(pGameEvent)) {
					le->type = TYPE_PYTHON;
					le->python_data->pGameEvent = pGameEvent;
					le->python_data->pModule = pModule;
				} else {
					Py_XDECREF(pGameEvent);
					LogFmt(OP_MOD, "%s: GameEvent() is not callable", libname);
				}
			} else {
				// function find failure
				PyErr_Print();
				
				Log(OP_MOD, "Couldn't find python function");
			}

			// loading failed, dont keep reference to module
			if (le->type != TYPE_PYTHON) {
				Py_DECREF(pModule);
			}
		} else {
			// error loading module
			PyErr_Print();
			LogFmt(OP_MOD, "Unable to load module: %s", libname);
		}
	}

	/* module failed to load, axe it */
	if (le->type == TYPE_UNKNOWN) {
		LogFmt(OP_MOD, "%s: Failed to load", libname);
		free(le);
		return;
	}

	/* event_start */
	bool unload = false;
	libman_export_event(td, EVENT_START, NULL, le);
	if (le->registered) {
		LogFmt(OP_MOD, "%s: Loaded successfully (%s)", le->libname, le->type == TYPE_NATIVE ? "Native" : le->type == TYPE_PYTHON ? "Python" : "Unknown");
	} else {
		/* plugin failed to register */
		LogFmt(OP_MOD, "%s: Failed to register", le->libname);
		unload = true;
	}

	/* setup pinfo */
	int nslots = mod_tld->pinfo_nslots;
	le->pinfo_base = (void**)xcalloc(nslots, sizeof(void*));
	for (int i = 0; i < nslots; ++i) {
		le->pinfo_base[i] = xmalloc(le->pinfo_size);
	}

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
		libman_export_event(td, EVENT_FIRSTSEEN, cd, le);
		libman_export_event(td, EVENT_ENTER, cd, le);
	}

	LIST_INSERT_HEAD(&mod_tld->lib_list_head, le, entry);

	if (unload) unload_library(td, libname);
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
SetTimer(ticks_ms_t duration, uintptr_t data1, uintptr_t data2)
{
	THREAD_DATA *td = get_thread_data();
	LIB_ENTRY *le = td->lib_entry;

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

			libman_export_event(td, EVENT_TIMER, cd, le);

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
	SIMPLEQ_INIT(&mod_tld->usercall_list_head);
	SIMPLEQ_INIT(&mod_tld->userevent_list_head);

	CORE_DATA *cd = &mod_tld->cd;

	cd->bot_pid = PID_NONE;
}

void
cmd_inslib(CORE_DATA *cd)
{
	THREAD_DATA *td = (THREAD_DATA*)get_thread_data();

	if (cd->cmd_argc != 2) {
		RmtMessageFmt(cd->cmd_name, "Usage: %s <lib name>",
		    cd->cmd_argv[0]);
		return;
	}

	char *libname = cd->cmd_argv[1];
	LIB_ENTRY *le = libman_find_lib(libname);

	if (le == NULL) {
		libman_load_library(td, libname);
		if (libman_find_lib(libname) != NULL)
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
		char *libname = cd->cmd_argv[1];
		if (strcmp(libname, CORE_NAME) != 0) {
			if (unload_library(td, libname) == 0) {
				RmtMessage(cd->cmd_name, "Library unloaded");
			} else {
				RmtMessage(cd->cmd_name, "Library is not loaded");
			}
		} else {
			RmtMessage(cd->cmd_name, "core cannot be unloaded");
		}
	} else {
		RmtMessageFmt(cd->cmd_name, "Usage: %s <lib name>", cd->cmd_argv[0]);
	}
}

LIB_ENTRY*
libman_find_lib(char *libname)
{
	THREAD_DATA *td = get_thread_data();
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

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
	    OPENCORE_VERSION, __DATE__, __TIME__);

	RmtMessage(cd->cmd_name, "");

	if (LIST_EMPTY(&mod_tld->lib_list_head) != 0)
		RmtMessage(cd->cmd_name, "No libraries are loaded");

	LIB_ENTRY *node;
	LIST_FOREACH(node, &mod_tld->lib_list_head, entry) {
		RmtMessageFmt(cd->cmd_name, "%-16.16s %c %-8.8s %-11.11s %-8.8s by %-16.16s  %s",
		    node->libname,
		    node->type == TYPE_NATIVE ? 'N' : node->type == TYPE_PYTHON ? 'P' : 'U',	
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

	LIB_ENTRY *le = libman_find_lib(libname);
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

		libman_export_event(td, EVENT_DEADSLOT, cd, le);
	}

	/* event_stop */
	libman_export_event(td, EVENT_STOP, NULL, le);

	/* free pinfo */
	if (le->pinfo_size > 0) {
		for (int i = 0; i < mod_tld->pinfo_nslots; ++i) {
			free(le->pinfo_base[i]);
		}
	}
	free(le->pinfo_base);

	/* free user_data */
	free(le->user_data);
		
	if (le->type == TYPE_NATIVE) {
		if (le->native_data->handle) dlclose(le->native_data->handle);
	} else if (le->type == TYPE_PYTHON) {
		Py_XDECREF(le->python_data->pGameEvent);
		Py_DECREF(le->python_data->pModule);
	}

	unregister_commands(td, le);

	LIST_REMOVE(le, entry);
	free(le);

	return 0;
}


void
libman_get_current_libname(char *dst, size_t dst_sz)
{
	THREAD_DATA *td = get_thread_data();
	LIB_ENTRY *le = td->lib_entry;

	strlcpy(dst, le->libname, dst_sz);
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


bool
UserEvent(const char *eventname, const char *arg)
{
	if (strlen(eventname) >= 32) return false;

	THREAD_DATA *td = get_thread_data();
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;
	LIB_ENTRY *le = td->lib_entry;

	USER_EVENT *ue = (USER_EVENT*)malloc(sizeof(USER_EVENT));

	strlcpy(ue->libname, le->libname, 64);
	strlcpy(ue->eventname, eventname, 32);
	if (arg) {
		ue->arg = strdup(arg);
		if (!ue->arg) {
			free(ue);
			return false;
		}
	} else {
		ue->arg = NULL;
	}

	SIMPLEQ_INSERT_TAIL(&mod_tld->userevent_list_head, ue, entry);

	return true;
}


bool
UserCall(const char *libname, const char *eventname, const char *arg)
{
	if (strlen(libname) >= 64 || strlen(eventname) >= 32) return false;

	THREAD_DATA *td = get_thread_data();
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->lib;

	USER_CALL *uc = (USER_CALL*)malloc(sizeof(USER_CALL));

	strlcpy(uc->libname, libname, 64);
	strlcpy(uc->functionname, eventname, 32);
	if (arg) {
		uc->arg = strdup(arg);
		if (!uc->arg) {
			free(uc);
			return false;
		}
	} else {
		uc->arg = NULL;
	}

	SIMPLEQ_INSERT_TAIL(&mod_tld->usercall_list_head, uc, entry);

	return true;
}

