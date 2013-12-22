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

#ifndef _CMD_HPP
#define _CMD_HPP

#include "libopencore.hpp"

#include "opencore.hpp"

/*
 * Called for each command.
 */
void	handle_command(THREAD_DATA *td,
	    PLAYER *p,
	    char *op_name,
	    uint8_t cmd_type,
	    char *command);

/*
 * Called for each command.
 */
void	cmd_instance_init(THREAD_DATA *td);
void	cmd_instance_shutdown(THREAD_DATA *td);

void	unregister_commands(THREAD_DATA *td, void *lib_entry);

void	cmd_about(CORE_DATA *cd);
void	cmd_die(CORE_DATA *cd);
void	cmd_help(CORE_DATA *cd);
void	cmd_stopbot(CORE_DATA *cd);
void	cmd_sysinfo(CORE_DATA *cd);

#endif

