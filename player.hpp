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

#ifndef _PLAYER_HPP
#define _PLAYER_HPP

#include "opencore.hpp"

#include "libopencore.hpp"

void	player_instance_init(THREAD_DATA *td);
void	player_instance_shutdown(THREAD_DATA *td);

/*
 * Remove players who have been absent for 'gone_ticks' milliseconds
 * or longer and generate an EVENT_DEADSLOT for each player removed.
 */
void	player_free_absent_players(THREAD_DATA *td, ticks_ms_t gone_ticks);

/*
 * Simulate EVENT_LEAVE for all present players. This only affects the core.
 * The event is not exported to librarys but all players are marked as
 * absent.  This is used when bot reconnects to clear out the present
 * players.
 */
void	player_simulate_player_leaves(THREAD_DATA *td);

/*
 * Get the total number of players in the player array.
 */
int	player_get_phere(THREAD_DATA *td);

/*
 * Get the player array's base pointer.
 */
PLAYER*	player_get_parray(THREAD_DATA *td);


/*
 * This is called the an enter packet is encountered. If the player
 * is new an EVENT_FIRSTSEEN is generated. If the player was not
 * already in the arena, an EVENT_ENTER is generated, otherwise
 * the event is ignored.
 *
 * If the player is newly seen, his player pointer with basic
 * initialized data is returned.
 */
PLAYER*	player_player_entered(THREAD_DATA *td, char *name, char *squad,
	    PLAYER_ID pid, FREQ freq, SHIP ship);

/*
 * Called when a leave packet is encountered. The player is removed
 * from the pid tree and an EVENT_LEAVE is generated.
 */
void	player_player_left(THREAD_DATA *td, PLAYER_ID pid);

/*
 * Called when a ship or freq change event occurs.
 */
void	player_player_change(THREAD_DATA *td, PLAYER *p, FREQ new_freq, SHIP new_ship);

/*
 * Internal versions of FindPlayerName and FindPlayerPid. These
 * don't do TLS lookups.
 */
PLAYER*	player_find_by_name(THREAD_DATA *td, const char *name, MATCH_TYPE match_type);
PLAYER*	player_find_by_pid(THREAD_DATA *td, PLAYER_ID pid, MATCH_TYPE match_type);

/*
 * Player linked list operations.
 */
#define GEN_PLAYER_LIST_PROTOTYPE(type)					\
inline									\
void									\
player_ll_##type##_add(PLAYER **head, PLAYER **tail, PLAYER *p)		\
{									\
	p->type##_prev = NULL;						\
	p->type##_next = *head;						\
	if (*head)							\
		(*head)->type##_prev = p;				\
	*head = p;							\
									\
	if (*tail == NULL)						\
		*tail = p;						\
}									\
									\
inline									\
void									\
player_ll_##type##_remove(PLAYER **head, PLAYER **tail, PLAYER *p)	\
{									\
	if (p->type##_prev)						\
		p->type##_prev->type##_next = p->type##_next;		\
	else								\
		*head = p->type##_next;					\
									\
	if (p->type##_next)						\
		p->type##_next->type##_prev = p->type##_prev;		\
	else								\
		*tail = p->type##_prev;					\
}

/* FIXME: this causes a lot of code duplication, need to make proper interface for this */
GEN_PLAYER_LIST_PROTOTYPE(here);
GEN_PLAYER_LIST_PROTOTYPE(ship);
GEN_PLAYER_LIST_PROTOTYPE(spec);


#endif

