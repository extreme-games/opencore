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

#include <sys/utsname.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsd/tree.h"

#include "botman.hpp"
#include "cmd.hpp"
#include "lib.hpp"
#include "log.hpp"
#include "msg.hpp"
#include "ops.hpp"

/*
 * Contains information about a command callback. These are used by
 * RegisterCommand() to keep track of command-specific data such as
 * arguments, descriptions, function pointers, and so forth.
 */
typedef
struct CMD_CALLBACK_DATA_
{
	SPLAY_ENTRY(CMD_CALLBACK_DATA_)	entry;	/* splay tree entry */

	int 		 id;			/* command id */
	char		 cmd_name[16];		/* name of the command */
	int		 cmd_level;		/* required op access level to use this command */
	CMD_TYPE	 cmd_type;		/* which type of messages this bot responds to (CMD_*) */
	char		 cmd_class[8];		/* class of the library */
	char		 cmd_args[32];		/* argument grammar */
	char		 cmd_desc[128];		/* one-line description of the command */
	char		 cmd_ldesc[256];	/* long description of the command */
	LIB_ENTRY	*lib_entry;		/* library id of the owning callback */
} CMD_CALLBACK_DATA;
int ccd_cmp(struct CMD_CALLBACK_DATA_ *lhs, struct CMD_CALLBACK_DATA_ *rhs);

SPLAY_HEAD(cmd_tree, CMD_CALLBACK_DATA_);

typedef struct MOD_TL_DATA_ MOD_TL_DATA;
struct MOD_TL_DATA_
{
	cmd_tree	tree_head;	/* head of the command tree */
};

static
inline
MOD_TL_DATA*
get_mod_tld(THREAD_DATA *td)
{
	return (MOD_TL_DATA*)td->mod_data->cmd;
}

SPLAY_PROTOTYPE(cmd_tree, CMD_CALLBACK_DATA_, entry, ccd_cmp);
SPLAY_GENERATE(cmd_tree, CMD_CALLBACK_DATA_, entry, ccd_cmp);

static CMD_CALLBACK_DATA*	allocate_ccd(int id, char *cmd_text, char *cmd_class, int cmd_level,
				    uint8_t cmd_type, char *cmd_args, char *cmd_desc, char *cmd_ldesc, LIB_ENTRY *lib_ptr);
static void			free_ccd(CMD_CALLBACK_DATA *ccd);

void
unregister_commands(THREAD_DATA *td, void *lib_entry)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)td->mod_data->cmd;

	CMD_CALLBACK_DATA *ccd, *next;
	for (ccd = SPLAY_MIN(cmd_tree, &mod_tld->tree_head); ccd != NULL; ccd = next) {
		next = SPLAY_NEXT(cmd_tree, &mod_tld->tree_head, ccd);
		if (ccd->lib_entry == lib_entry) {
			SPLAY_REMOVE(cmd_tree, &mod_tld->tree_head, ccd);
			free(ccd);
		}
	}
}

int
ccd_cmp(struct CMD_CALLBACK_DATA_ *lhs, struct CMD_CALLBACK_DATA_ *rhs)
{
	return strcasecmp(lhs->cmd_name, rhs->cmd_name);
}

void
cmd_instance_init(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = (MOD_TL_DATA*)malloc(sizeof(MOD_TL_DATA));
	bzero(mod_tld, sizeof(MOD_TL_DATA));

	td->mod_data->cmd = mod_tld;

	SPLAY_INIT(&mod_tld->tree_head);

}

void
cmd_instance_shutdown(THREAD_DATA *td)
{
	MOD_TL_DATA *mod_tld = get_mod_tld(td);
	
	CMD_CALLBACK_DATA *ccd, *next;
	for (ccd = SPLAY_MIN(cmd_tree, &mod_tld->tree_head); ccd != NULL; ccd = next) {
		next = SPLAY_NEXT(cmd_tree, &mod_tld->tree_head, ccd);
		SPLAY_REMOVE(cmd_tree, &mod_tld->tree_head, ccd);
		free(ccd);
	}
	
	free(mod_tld);
}

void 
RegisterCommand(int id, char *cmd_text, char *cmd_class, int cmd_level,
		uint8_t cmd_type, char *cmd_args, char *cmd_desc, char *cmd_ldesc) 
{
	THREAD_DATA *td = get_thread_data();
	MOD_TL_DATA *mod_tld = get_mod_tld(td);

	CMD_CALLBACK_DATA *ccd = allocate_ccd(id, cmd_text, cmd_class, cmd_level, cmd_type,
			cmd_args, cmd_desc, cmd_ldesc, td->lib_entry);

	/* insert ccd into the tree if it doesnt exist */
	struct CMD_CALLBACK_DATA_ *retval = SPLAY_INSERT(cmd_tree, &mod_tld->tree_head, ccd);
	if (retval != NULL) {
		LogFmt(OP_SMOD, "Ignoring duplicate command registration: %s", cmd_text);
		free_ccd(ccd);
	}
}


/*
 * Allocate a command callback entry.
 */
static
CMD_CALLBACK_DATA*
allocate_ccd(int id, char *cmd_name, char *cmd_class, int cmd_level,
    uint8_t cmd_type, char *cmd_args, char *cmd_desc,
    char *cmd_ldesc, LIB_ENTRY *lib_entry)
{
	CMD_CALLBACK_DATA *ccd = (CMD_CALLBACK_DATA*)malloc(sizeof(CMD_CALLBACK_DATA));

	ccd->id = id;
	strlcpy(ccd->cmd_name, cmd_name, 16);
	strlcpy(ccd->cmd_class, cmd_class, 8);
	ccd->cmd_level = cmd_level;
	ccd->cmd_type = cmd_type;
	strlcpy(ccd->cmd_args, cmd_args ? cmd_args : "(No Arguments)", 32);
	strlcpy(ccd->cmd_desc, cmd_desc ? cmd_desc : "", 128);
	strlcpy(ccd->cmd_ldesc, cmd_ldesc ? cmd_ldesc : "None", 256);
	ccd->lib_entry = lib_entry;

	return ccd;
}

/*
 * Free a command callback entry.
 */
static
void
free_ccd(CMD_CALLBACK_DATA *ccd)
{
	free(ccd);
}

static
CMD_CALLBACK_DATA*
auto_complete_helper(THREAD_DATA *td, char *cmd_name, int op_level,
    char *matchlist, int ml_len, int *pnmatches, int *exact)
{
	MOD_TL_DATA *mod_tld = get_mod_tld(td);

	CMD_CALLBACK_DATA *res;
	CMD_CALLBACK_DATA ccd, *n;

	strlcpy(ccd.cmd_name, cmd_name, 16);
	res = SPLAY_FIND(cmd_tree, &mod_tld->tree_head, &ccd);

	strlcpy(matchlist, "", ml_len);
	*pnmatches = 0;
	*exact = 0;

	if (res) {
		*exact = 1;
		*pnmatches = 1;
		strlcpy(matchlist, res->cmd_name, ml_len);
	} else {
		SPLAY_INSERT(cmd_tree, &mod_tld->tree_head, &ccd);

		int nmatches = 0;

		n = &ccd;
		while ((n = SPLAY_NEXT(cmd_tree, &mod_tld->tree_head, n)) != NULL &&
		    strncasecmp(n->cmd_name, cmd_name, strlen(cmd_name)) == 0 ) {
			if (op_level >= n->cmd_level) {
				if (nmatches != 0)
					strlcat(matchlist, ", ", ml_len);

				strlcat(matchlist, n->cmd_name, ml_len);
				res = n;

				++nmatches;
			}
		}

		*pnmatches = nmatches;
		if (nmatches >= 2) {
			res = NULL;	/* dont return a pointer if multiple cmds match */
		}

		SPLAY_REMOVE(cmd_tree, &mod_tld->tree_head, &ccd);
	}

	return res;
}

/*
 * Take a message string, parse its arguments, and pass it off
 * to the correct command handler if one exists.
 */
void
handle_command(THREAD_DATA *td, PLAYER *p,
    char *op_name, uint8_t cmd_type, char *command)
{
	char cmd_args[256];
	char *cmd_p = cmd_args;
	strlcpy(cmd_args, command, 256);

	/* take arguments and put them into argv/argr form */
	char **ap, *argv[17], **ap2, *argr[17];
	for (int i = 0; i < 16; ++i) {
		argv[i] = "";
		argr[i] = "";
	}
	argv[16] = NULL;
	argr[16] = NULL;

	int argc = 0;
	ap = argv;
	ap2 = argr;
	while (argc < 13 && (*ap = strsep(&cmd_p, " ")) != NULL) {
		if (**ap != '\0') {
			*ap2 = &command[*ap - cmd_args];
			++ap;
			++ap2;
			++argc;
		}
	}
	*ap = NULL;
	*ap2 = NULL;

	if (argc < 1) {
		return;
	}

	int op_level = GetOpLevel(op_name);

	char matches[256];
	int nmatches, exact;
	CMD_CALLBACK_DATA *ccd = auto_complete_helper(td, argv[0], op_level,
	    matches, sizeof(matches), &nmatches, &exact);

	int cmd_unknown = 0;
	if (ccd && op_level >= ccd->cmd_level) {
		if (exact == 0) {
			RmtMessageFmt(op_name, "Executing: %s %s", ccd->cmd_name, argr[1] ? argr[1] : "");
		}

		if (ccd->cmd_type & cmd_type) {
			char cmdstr[256];
			snprintf(cmdstr, 256, "%s %s", ccd->cmd_name, argr[1] ? argr[1] : "");
			log_logcmd(td->bot_name, (*td->arena->name) ? td->arena->name : (char*)"No Arena", op_name, op_level, cmdstr);

			CORE_DATA *cd = libman_get_core_data(td);
			cd->cmd_id = ccd->id;
			cd->cmd_name = op_name;
			cd->cmd_type = cmd_type;
			cd->cmd_argc = argc;
			cd->cmd_argv = argv;
			cd->cmd_level = op_level;
			cd->cmd_argr = argr;
			cd->p1 = p;

			libman_export_event(td, EVENT_COMMAND, cd, ccd->lib_entry);
		} else {
			char *type = "";
			if (cmd_type == CMD_REMOTE) type = "remote";
			else if (cmd_type == CMD_PUBLIC) type = "public";
			else if (cmd_type == CMD_PRIVATE) type = "private";
			RmtMessageFmt(op_name, "This command is not available through %s messages.", type);
		}
	} else if (nmatches >= 2) {
		RmtMessageFmt(op_name, "Matches: %s", matches);
	} else {
		cmd_unknown = 1;
	}
	
	if (cmd_unknown == 1 && (cmd_type == CMD_REMOTE || cmd_type == CMD_PRIVATE)) {
		RmtMessageFmt(op_name, "Unknown command, see \"!help\" for a command listing");
	}
}

void
cmd_sysinfo(CORE_DATA *cd)
{
	struct utsname uts;
	bzero(&uts, sizeof(struct utsname));
	uname(&uts);

	/* hide full hostname */
	char *dot = strstr(uts.nodename, ".");
	if (dot != NULL)
		*dot = '\0';	

	RmtMessageFmt(cd->cmd_name, "%s %s %s %s %s",
	    uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);

	double load[3];
	if (getloadavg(load, 3) != -1)
		RmtMessageFmt(cd->cmd_name, "Load Averages: %.2f %.2f %.2f", load[0], load[1], load[2]);
}

void
cmd_help(CORE_DATA *cd)
{
	char *match = NULL;
	if (cd->cmd_argc >= 2)
		match = cd->cmd_argv[1];

	THREAD_DATA *td = get_thread_data();
	MOD_TL_DATA *mod_tld = get_mod_tld(td);

	/* show sections */
	CMD_CALLBACK_DATA *ccd;
	int displayed = 0;

	SPLAY_FOREACH(ccd, cmd_tree, &mod_tld->tree_head) {
		if (cd->cmd_level >= ccd->cmd_level) {

			if (match == NULL ||
			    strcasecmp(match, ccd->cmd_class) == 0 ||
			    strcasecmp(match, ccd->cmd_name) == 0) {
				char prefix[32];
				snprintf(prefix, 32, "[%s]", ccd->cmd_class);

				if (cd->cmd_level > 0) { /* only show command level/type information to staff */
					RmtMessageFmt(cd->cmd_name, "%-10s [%s%s%s:%d] %-16s %-24s %s",
					    prefix,
					    (ccd->cmd_type & CMD_PUBLIC)  ? "P" : "-",
					    (ccd->cmd_type & CMD_PRIVATE) ? "P" : "-",
					    (ccd->cmd_type & CMD_REMOTE)  ? "R" : "-",
					    ccd->cmd_level,
					    ccd->cmd_name,
					    ccd->cmd_args,
					    ccd->cmd_desc
					    );
				} else {
					RmtMessageFmt(cd->cmd_name, "%-10s %-16s %-24s %s",
					    prefix,
					    ccd->cmd_name,
					    ccd->cmd_args,
					    ccd->cmd_desc
					    );
				}
				
				/* if the match was for a specific command, show its long description */
				if (match && match[0] == CMD_CHAR) {
					RmtMessageFmt(cd->cmd_name, "Description: %s",
					    (ccd->cmd_ldesc[0] != '\0') ? ccd->cmd_ldesc : "None");
				}

				++displayed;
			}
		}
	}

	if (displayed == 0) {
		if (match[0] == CMD_CHAR)
			RmtMessageFmt(cd->cmd_name, "Command not found.");
		else
			RmtMessageFmt(cd->cmd_name, "Section not found. If you are searching for a "	 
			    "command, be sure to precede it with '%c'.", CMD_CHAR);
	}
}

void
cmd_stopbot(CORE_DATA *cd)
{
	RmtMessageFmt(cd->cmd_name, "Stopping bot...");	
	StopBotFmt("Stopbot issued by %s", cd->cmd_name);
}

void
cmd_die(CORE_DATA *cd)
{
	RmtMessageFmt(cd->cmd_name, "Shutting down...");	
	LogFmt(OP_MOD, "Die issued by %s", cd->cmd_name);
	botman_stop_all_bots();
}

