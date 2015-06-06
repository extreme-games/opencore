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

#include "ops.hpp"

#include <pthread.h>

#include <stdio.h>

#include <map>

#include "cstr.hpp"
#include "util.hpp"

/*
 * Used by the map of bot operators. Contains security information about
 * an op.
 */
typedef
struct OP_ENTRY_
{
	int	level;	/* player's access level (0-9) */
} OP_ENTRY;

/*
 * Global op map and its mutex.
 */
typedef Cstr<20, strcasecmp>		op_name_t;
typedef std::map<op_name_t, OP_ENTRY>	op_map_t;
static op_map_t				g_op_map;
static pthread_mutex_t			g_op_map_mutex = PTHREAD_MUTEX_INITIALIZER; 

int
GetOpLevel(char *name)
{
	int level = 0;
	
	pthread_mutex_lock(&g_op_map_mutex);
	op_map_t::iterator iter = g_op_map.find(name);
	if (iter != g_op_map.end()) {
		OP_ENTRY *oe = &iter->second;
		level = oe->level;
	}
	pthread_mutex_unlock(&g_op_map_mutex);

	return level;
}

void
cmd_listops(CORE_DATA *cd)
{
	pthread_mutex_lock(&g_op_map_mutex);
	op_map_t::iterator iter = g_op_map.begin();

	while (iter != g_op_map.end()) {
		if (iter->second.level <= cd->cmd_level) {
			ReplyFmt("Level %d Op: %s", iter->second.level, iter->first.c_str());	
		}
		++iter;
	}

	pthread_mutex_unlock(&g_op_map_mutex);
}

void
cmd_loadops(CORE_DATA *cd)
{
	if (load_op_file() == 0) {
		RmtMessageFmt(cd->cmd_name, "Reloaded op file: %s", OP_FILE);
		LogFmt(OP_SMOD, "%s reloaded by %s", OP_FILE, cd->cmd_name);
	} else
		RmtMessageFmt(cd->cmd_name, "Reloading of %s failed", OP_FILE);
}


int
load_op_file()
{
	FILE *f = fopen(OP_FILE, "rb");
	if (f) {
		pthread_mutex_lock(&g_op_map_mutex);
		g_op_map.clear();
		
		char buf[256];
		while (get_line(f, buf, 256) != EOF) {
			if (buf[0] != '\0' && buf[0] != '#') {
				if (DelimCount(buf, ':') == 1) {
					char name[20]; 
					DelimArgs(name, 20, buf, 0, ':', false);
					op_name_t entry_name = name;
					OP_ENTRY entry_value;
					entry_value.level = AtoiArg(buf, 1, ':');
					if (entry_value.level >= 1 &&
							entry_value.level <= 9) {
						g_op_map.insert(std::make_pair(entry_name, entry_value));
					} else
						LogFmt(OP_SMOD, "Ignoring invalid op level for %s", name);
				}
			}
		}

		fclose(f);
		pthread_mutex_unlock(&g_op_map_mutex);
	} else
		return -1;

	return 0;
}

