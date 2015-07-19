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

#ifndef _BOTMAN_HPP
#define _BOTMAN_HPP

#include "core.hpp"

#define BOTMAN_CHECKIN_INTERVAL		5000
#define BOTMAN_STOPCHECK_INTERVAL	250

/*
 * Instance init/shutdown functions
 */
void	botman_init();
void	botman_shutdown();

/*
 * The mainloop of the core.
 */
void	botman_mainloop();

void	botman_bot_exiting(void *bh);
void	botman_bot_checkin(void *bh);
int	botman_bot_shouldstop(void *bh);
void	botman_stop_all_bots();

void	cmd_types(CORE_DATA *cd);
void	cmd_loadtypes(CORE_DATA *cd);
void	cmd_listbots(CORE_DATA *cd);

#endif

