/*
 * Copyright (c) 2006 cycad <cycad@zetasquad.com>
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

/* FIXME: move cmd_loadops to this module */

#ifndef _OPS_HPP
#define _OPS_HPP

#include "libopencore.hpp"

#define OP_FILE "ops.conf"

int	load_op_file();
void	cmd_loadops(CORE_DATA *cd);
void	cmd_listops(CORE_DATA *cd);

#endif

