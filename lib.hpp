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

#ifndef _LIB_HPP
#define _LIB_HPP

#include "core.hpp"

/*
 * Instance init/shutdown functions
 */
void	libman_instance_init(THREAD_DATA *td);
void	libman_instance_shutdown(THREAD_DATA *td);

/*
 * Expire all timers held by td
 */
void	libman_expire_timers(THREAD_DATA *td);

/*
 * Get the core data pointer for the thread, usually so it can be
 * initialized prior to the exportation of an event.
 */
CORE_DATA* libman_get_core_data(THREAD_DATA *td);

/*
 * Export an event to all libraries. If 'cd' is null only a CODE_DATA
 * with basic initialization will be sent.  If 'le' is specified, the event
 * is only exported to the specified library.
 */
void	libman_export_event(THREAD_DATA *td, int event, CORE_DATA *cd, LIB_ENTRY *le=NULL);

/*
 * Load a library into the core.
 */
void	libman_load_library(THREAD_DATA *td, char *libname);

/*
 * These are the player module's interface to pinfo management.
 */
void	libman_realloc_pinfo_array(THREAD_DATA *td, int new_size);
void	libman_move_pinfo_entry(THREAD_DATA *td, int dest_index, int source_index);

/*
 * This zeroes a players pinfo for a player in all libraries.
 */
void	libman_zero_pinfo(THREAD_DATA *td, PLAYER *p);

/*
 * Command callbacks.
 */
void	cmd_about(CORE_DATA *cd);
void	cmd_inslib(CORE_DATA *cd);
void	cmd_rmlib(CORE_DATA *cd);

#endif

