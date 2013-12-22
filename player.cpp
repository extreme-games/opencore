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

#include <sys/param.h>

#include <assert.h>
#include <string.h>

#include "bsd/tree.h"

#include "player.hpp"

#include "lib.hpp"
#include "opencore.hpp"
#include "util.hpp"

/*
 * The player module maintains two red-black trees: One for player
 * IDs of players currently in the arena. The second player tree
 * is of players that the bot has seen since it was loaded but who
 * have not gone through EVENT_DEADSLOT.
 *
 * As such searching for a player by name may return a player who
 * isn't in the arena. PLAYER.in_arena will be set based on his
 * presence.
 */

/*
 * The player array is incremented by this number each time it needs
 * to be expanded.
 */
#define PA_INCREMENT 128

struct
player_node
{
	RB_ENTRY(player_node) entry;

	PLAYER *p;
};
int name_cmp(struct player_node *lhs, struct player_node *rhs);
int pid_cmp(struct player_node *lhs, struct player_node *rhs);

RB_HEAD(pid_tree, player_node);
RB_HEAD(name_tree, player_node);

typedef
struct MOD_TL_DATA_
{
	pid_tree	 pid_tree_head;		/* pid tree */
	name_tree	 name_tree_head;	/* head of the name tree */

	struct {			/* cache for player lookups */
		PLAYER *last_pid_lookup;
		PLAYER *last_name_lookup;
	} cache[1];

	PLAYER		*parray;	/* player array */
	int 		 phere;		/* player slots used */
	int		 ptotal;	/* total slots allocated */
} MOD_TL_DATA;

RB_PROTOTYPE(pid_tree, player_node, entry, pid_cmp);
RB_GENERATE(pid_tree, player_node, entry, pid_cmp);
RB_PROTOTYPE(name_tree, player_node, entry, name_cmp);
RB_GENERATE(name_tree, player_node, entry, name_cmp);

static void	player_pointers_changed(THREAD_DATA *td);
static void	rebuild_player_lists(THREAD_DATA *td);

int
name_cmp(struct player_node *lhs, struct player_node *rhs)
{
	return strcasecmp(lhs->p->name, rhs->p->name);
}

int
pid_cmp(struct player_node *lhs, struct player_node *rhs)
{
	return lhs->p->pid - rhs->p->pid;
}

int
player_get_phere(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	return mod_tld->phere;
}


PLAYER*
player_get_parray(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	return mod_tld->parray;
}


static
void
build_trees(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	PLAYER *parray = player_get_parray(td);
	for (int i = 0; i < player_get_phere(td); ++i) {
		PLAYER *p = &parray[i];

		struct player_node *n = (struct player_node*)malloc(sizeof(struct player_node));
		n->p = p;
		RB_INSERT(name_tree, &mod_tld->name_tree_head, n);

		if (p->in_arena) {
			n = (struct player_node*)malloc(sizeof(struct player_node));
			n->p = p;
			RB_INSERT(pid_tree, &mod_tld->pid_tree_head, n);
		}
	}
}

static
void
empty_trees(MOD_TL_DATA *mod_tld)
{
	struct player_node *n, *nxt;
	for (n = RB_MIN(pid_tree, &mod_tld->pid_tree_head); n != NULL; n = nxt) {
		nxt = RB_NEXT(pid_tree, &mod_tld->pid_tree_head, n);
		RB_REMOVE(pid_tree, &mod_tld->pid_tree_head, n);
		free(n);
	}

	for (n = RB_MIN(name_tree, &mod_tld->name_tree_head); n != NULL; n = nxt) {
		nxt = RB_NEXT(name_tree, &mod_tld->name_tree_head, n);
		RB_REMOVE(name_tree, &mod_tld->name_tree_head, n);
		free(n);
	}
}


/*
 * Remove absent players from the player array. Both the pid and name
 * tree are freed and then restructured. Player pointers could change
 * after this.
 */
void
player_free_absent_players(THREAD_DATA *td, ticks_ms_t gone_ticks)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	PLAYER *p, *newp;

	ticks_ms_t ticks = get_ticks_ms();

	/*
	 * do this here instead of below so the player trees are in a valid
	 * state during the event
	 */
	for (int i = 0; i < mod_tld->phere; ++i) {
		p = &mod_tld->parray[i];
		
		if (p->in_arena == 0 && ticks - p->leave_tick >= gone_ticks) {
			/* event_deadslot */
			CORE_DATA *cd = libman_get_core_data(td);
			cd->p1 = p;
			libman_export_event(td, EVENT_DEADSLOT, cd);
		}
	}

	/* free unused player entries, and shift them all to the left */
	/* also shift pinfo */
	int used_offset = 0;
	for (int i = 0; i < mod_tld->phere; ++i) {
		p = &mod_tld->parray[i];
		
		if (p->in_arena == 0 && ticks - p->leave_tick >= gone_ticks) {
			/* player is being removed, do not copy his data */
		} else {
			/* copy pinfo and player pointer over to new location */
			newp = &mod_tld->parray[used_offset];
			libman_move_pinfo_entry(td, used_offset, i);
			memcpy(newp, p, sizeof(PLAYER));
			++used_offset;
		}
	}
	mod_tld->phere = used_offset;

	/* re-fill the trees with new parray information */
	player_pointers_changed(td);
}


/*
 * This must be called whenever player pointers change so trees and
 * internally stored player pointers can be invalidated!
 */
static
void
player_pointers_changed(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	empty_trees(mod_tld);
	build_trees(td);
	rebuild_player_lists(td);
}

static
void
rebuild_player_lists(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	td->arena->here_first = NULL;
	td->arena->here_last = NULL;
	td->arena->ship_first = NULL;
	td->arena->ship_last = NULL;
	td->arena->spec_first = NULL;
	td->arena->spec_last = NULL;
	
	for (int i = 0; i < mod_tld->phere; ++i) {
		td->arena->here_first = NULL;
		td->arena->here_last = NULL;
		td->arena->spec_first = NULL;
		td->arena->spec_last = NULL;
		td->arena->ship_first = NULL;
		td->arena->ship_last = NULL;
	}

	for (int i = 0; i < mod_tld->phere; ++i) {
		PLAYER *p = &mod_tld->parray[i];

		if (p->in_arena) {
			player_ll_here_add(&td->arena->here_first, &td->arena->here_last, p);

			if (p->ship == SHIP_NONE) {
				player_ll_spec_add(&td->arena->spec_first, &td->arena->spec_last, p);
			} else {
				player_ll_ship_add(&td->arena->ship_first, &td->arena->ship_last, p);
			}
		}
	}
}


/*
 * Expand player array by PA_INCREMENT elements.
 */
void
player_expand_parray(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	/* expand parray since it is at capacity */
	mod_tld->ptotal += PA_INCREMENT;

	libman_realloc_pinfo_array(td, mod_tld->ptotal);

	PLAYER *old_parray = mod_tld->parray;
	mod_tld->parray = (PLAYER*)realloc(mod_tld->parray,
	    mod_tld->ptotal * sizeof(PLAYER));

	if (old_parray != mod_tld->parray) {
		/* fix up altered player pointers */
		player_pointers_changed(td);
	}
}

/*
 * Returns NULL if the player exists in the arena already. Otherwise
 * returns a pointer to the newly added player.
 */
PLAYER*
player_player_entered(THREAD_DATA *td, char *name, char *squad, PLAYER_ID pid, FREQ freq, SHIP ship)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	bool seen_before = true;
	PLAYER *p = player_find_by_name(td, name, MATCH_HERE | MATCH_GONE);
	if (p == NULL) {
		if (mod_tld->phere == mod_tld->ptotal) {
			player_expand_parray(td);
		}

		p = &mod_tld->parray[mod_tld->phere];
		bzero(p, sizeof(PLAYER));

		++mod_tld->phere;
		seen_before = false;
	} else if (p->in_arena) {
		return NULL;
	}

	/* add player to here list */
	player_ll_here_add(&td->arena->here_first, &td->arena->here_last, p);
	td->arena->here_count += 1;
	
	/* setup player's data */
	strlcpy(p->name, name, 20);
	strlcpy(p->squad, squad, 20);
	p->freq = freq;
	p->ship = ship;
	p->pid = pid;

	p->in_arena = 1;
	p->enter_tick = get_ticks_ms();

	if (p->ship == SHIP_NONE) {
		player_ll_spec_add(&td->arena->spec_first, &td->arena->spec_last, p);
		td->arena->spec_count += 1;
	} else {
		player_ll_ship_add(&td->arena->ship_first, &td->arena->ship_last, p);
		td->arena->ship_count += 1;
	}
	
	/*
	 * einfo/info entries are invalidated on enter (if a player is out of
	 * an arena you can tell the entry might be old because of his
	 * absense).
	 */
	p->info->valid = 0;
	p->einfo->valid = 0;
		
	/* initialize thread data if the bot is entering */	
	if (strcasecmp(p->name, td->bot_name) == 0) {
		td->in_arena = 1;
		strlcpy(td->arena->name, td->login->arenaname, 16);

		td->bot_pid = p->pid;
		td->bot_ship = p->ship;
		strlcpy(td->bot_name, p->name, 20);
		td->bot_freq = p->freq;
	}

	/* insert p & index into trees and export events */
	struct player_node *n = (struct player_node*)malloc(sizeof(struct player_node));
	n->p = p;
	RB_INSERT(pid_tree, &mod_tld->pid_tree_head, n);

	CORE_DATA *cd = libman_get_core_data(td);
	cd->p1 = p;
	if (seen_before == false) {
		/* insert into name tree */
		n = (struct player_node*)malloc(sizeof(struct player_node));
		n->p = p;
		RB_INSERT(name_tree, &mod_tld->name_tree_head, n);

		/* initialize pinfo */
		libman_zero_pinfo(td, p);

		/* event_firstseen */
		libman_export_event(td, EVENT_FIRSTSEEN, cd);
	}

	/* event_enter */
	libman_export_event(td, EVENT_ENTER, cd);

	return p;
}

void
player_player_change(THREAD_DATA *td, PLAYER *p, FREQ new_freq, SHIP new_ship)
{
	if (p->pid == td->bot_pid) {
		td->bot_freq = new_freq;
		td->bot_ship = new_ship;
	}

	if (p->ship == SHIP_NONE && new_ship != SHIP_NONE) {
		/* player entered ship */
		player_ll_ship_add(&td->arena->ship_first, &td->arena->ship_last, p);
		td->arena->ship_count++;

		player_ll_spec_remove(&td->arena->spec_first, &td->arena->spec_last, p);
		td->arena->spec_count--;
	} else if (p->ship != SHIP_NONE && new_ship == SHIP_NONE) {
		/* player specced */
		player_ll_spec_add(&td->arena->spec_first, &td->arena->spec_last, p);
		td->arena->spec_count++;

		player_ll_ship_remove(&td->arena->ship_first, &td->arena->ship_last, p);
		td->arena->ship_count--;
	}

	/* event_change */
	CORE_DATA *cd = libman_get_core_data(td);

	cd->p1 = p;
	cd->old_freq = p->freq;
	cd->old_ship = p->ship;

	p->freq = new_freq;
	p->ship = new_ship;

	libman_export_event(td, EVENT_CHANGE, cd);
}

void
player_player_left(THREAD_DATA *td, PLAYER_ID pid)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	struct player_node n, *res;
	PLAYER p;
	p.pid = pid;
	n.p = &p;

	/* 
	 * take player out of the pid tree since he could have left
	 * the zone, invalidating the pid
	 */
	res = RB_FIND(pid_tree, &mod_tld->pid_tree_head, &n);
        if (res) {
		PLAYER *p = res->p;

		RB_REMOVE(pid_tree, &mod_tld->pid_tree_head, res);
		free(res);

		player_ll_here_remove(&td->arena->here_first, &td->arena->here_last, p);
		td->arena->here_count -= 1;

		if (p->ship == SHIP_NONE) {
			player_ll_spec_remove(&td->arena->spec_first, &td->arena->spec_last, p);
			td->arena->spec_count -= 1;
		} else {
			player_ll_ship_remove(&td->arena->ship_first, &td->arena->ship_last, p);
			td->arena->ship_count -= 1;
		}

		/* event_leave */
		CORE_DATA *cd = libman_get_core_data(td);
		cd->old_freq = p->freq;
		cd->old_ship = p->ship;
		cd->p1 = p;

		p->in_arena = 0;
		p->leave_tick = get_ticks_ms();
		p->pid = PID_NONE;
		p->freq = FREQ_NONE;
		p->ship = SHIP_NONE;

		if (p->pid == td->bot_pid) {
			td->bot_pid = PID_NONE;
			td->in_arena = 0;
		}

		libman_export_event(td, EVENT_LEAVE, cd);
	}
}

void
player_instance_init(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)malloc(sizeof(MOD_TL_DATA));
	memset(mod_tld, 0, sizeof(MOD_TL_DATA));

	td->mod_data->player = mod_tld;

	RB_INIT(&mod_tld->pid_tree_head);
	RB_INIT(&mod_tld->name_tree_head);
}

/*
 * Generate fake player leave events for all existing players. Used
 * to empty the player tree when the bot gets disconnected and then
 * reconnects.
 */
void
player_simulate_player_leaves(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	PLAYER *p; 
	for (int i = 0; i < mod_tld->phere; ++i) {
		p = &mod_tld->parray[i];
		if (p->in_arena)
			player_player_left(td, p->pid);
	}   
}

/*
 * Frees all memory used by the player module.
 */
void
player_instance_shutdown(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;
	
	player_simulate_player_leaves(td);	
	player_free_absent_players(td, (ticks_ms_t)0);

	free(mod_tld->parray);

	/* free thread's data */
	free(mod_tld);
}

/*
 * Searches all players.
 */
PLAYER*
FindPlayerName(char *name, MATCH_TYPE match_type)
{
	return player_find_by_name(get_thread_data(), name, match_type);
}


PLAYER*
player_find_by_name(THREAD_DATA *td, const char *name, MATCH_TYPE match_type)
{
	assert (name != NULL);

	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	if (strlen(name) >= 20)
		return NULL;

	PLAYER *res = NULL;	/* final result */

	PLAYER **pcache = &mod_tld->cache->last_name_lookup; 
	if (*pcache && strcasecmp((*pcache)->name, name) == 0) {
		/* cache hit */
		res = *pcache;
	} else {
		/* exact name match lookup needed */

		struct player_node n, *elm;
		PLAYER p;
		n.p = &p;
		strlcpy(p.name, name, 20);

		elm = RB_FIND(name_tree, &mod_tld->name_tree_head, &n);
		if (elm) {
			res = elm->p;
		}

		/* lookup name by prefix if prior matches failed */
		if (elm == NULL && (match_type & MATCH_PREFIX)) {
			elm = RB_NFIND(name_tree, &mod_tld->name_tree_head, &n);
			if (elm) {
				if (strncasecmp(elm->p->name, name, MIN(strlen(name), 20)) == 0) {
					res = elm->p;
				}
			}
		}
	}

	if (res) {
		/* store lookup to cache */
		*pcache = res;

		/* apply flags */
		if ((match_type & MATCH_HERE) == 0 && res->in_arena) {
			res = NULL;
		}
		if ((match_type & MATCH_GONE) == 0 && res->in_arena == 0) {
			res = NULL;
		}
	}

	return res;
}


/*
 * Only searches players in the arena, since pids are not guaranteed to be valid
 * once a player leaves (The player could leave and reenter the zone).
 */
PLAYER*
FindPlayerPid(PLAYER_ID pid, MATCH_TYPE match_type)
{
	return player_find_by_pid(get_thread_data(), pid, match_type);
}

/* MATCH_PREFIX and MATCH_GONE are ignored */
PLAYER*
player_find_by_pid(THREAD_DATA *td, PLAYER_ID pid, MATCH_TYPE match_type)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->player;

	PLAYER *res = NULL;

	/* check cache */
	PLAYER **pcache = &mod_tld->cache->last_pid_lookup; 
	if (*pcache && (*pcache)->pid == pid) {
		res = *pcache;
	} else {
		struct player_node n, *elm;
		PLAYER p;
		n.p = &p;
		p.pid = pid;

		elm = RB_FIND(pid_tree, &mod_tld->pid_tree_head, &n);

		if (elm) {
			res = elm->p;
		}
	}

	if (res) {
		*pcache = res;
	}

	if ((match_type & MATCH_HERE) == 0) {
		Log(OP_SMOD, "PID search for player not in arena, this is probably a plugin error\n");
		res = NULL;
	}

	return res;
}

