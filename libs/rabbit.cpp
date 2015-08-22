/*
 * Copyright (c) 2007-2015 cycad <cycad@greencams.net>
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

/*
 * This is a bot that runs the rabbit game.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opencore.hpp"

#define GAME_LENGTH_MS		((ticks_ms_t)(2 * 60 * 1000))
#define SPAM_INTERVAL_MS	((ticks_ms_t)(15 * 1000))
#define SAFE_GRACE_MS		((ticks_ms_t)(15 * 1000))

typedef struct user_data_ USER_DATA;
struct user_data_ {
	PLAYER_ID	rabbit_pid;		/*
						 * set to rabbits pid if the game is running
						 * to check if a game is going on use a 
						 * (user_data.rabbit_pid != PID_NONE) comparison
						 */
	int		expecting_where;	/* set if the bot is expecting *where response */
	long		where_timer;		/* timer id of location timer */
	long		gameover_timer;		/* timer id of game over timer */
};

inline
USER_DATA*
ud(CORE_DATA *cd)
{
	return (USER_DATA*)cd->user_data;
}


static void start_rabbit_game(CORE_DATA *cd, PLAYER *p, int bty, int special);

extern "C"
void
CmdRabbit(CORE_DATA *cd)
{
}


static
void
start_rabbit_game(CORE_DATA *cd, PLAYER *p, int bty, int special)
{
	/* store rabbit's pid in user data */
	ud(cd)->rabbit_pid = p->pid;

	/* announce the game to the arena */
	ArenaMessageFmt("%s is now the rabbit!", p->name);
	ArenaMessageFmt("%s is now the rabbit!", p->name);
	ArenaMessageFmt("%s is now the rabbit!", p->name);

	/* announce the game to the player */
	PrivMessageFmt(p, "You are now the rabbit! The game will end if you enter safe!");

	/* prize the bounty/special */
	PrivMessageFmt(p, "*prize %d", bty);
	if (special)
		PrivMessageFmt(p, "*prize #%d", special);

	/*
	 * send *where to get rabbits location and set the bot to parse messages looking
	 * for the response
	 */
	PrivMessage(p, "*where");
	ud(cd)->expecting_where = 1;

	/* set the timer for the next *where to be sent */
	ud(cd)->where_timer = SetTimer(SPAM_INTERVAL_MS, 0, 0);

	/* set the timer that signifies the game's end */
	ud(cd)->gameover_timer = SetTimer(GAME_LENGTH_MS, 0, 0);
}


#define COMMAND_RABBIT 1

extern "C"
void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		RegisterPlugin(OPENCORE_VERSION, "rabbit", "cycad", "1.0", __DATE__, __TIME__, "A rabbit bot", sizeof(USER_DATA), 0);

		/* set rabbit pid to nobody */
		ud(cd)->rabbit_pid = PID_NONE;

		/* register rabbit command */
		RegisterCommand(COMMAND_RABBIT, "!rabbit", "Rabbit", 2, CMD_PRIVATE, "<name>:<bounty>[:<special>]", "Start the rabbit game", NULL); break;
	case EVENT_LOGIN:
		/* set the bot's location to center */
		SetPosition(16 * 512, 16 * 512, 0, 0);
		break;
	case EVENT_COMMAND:
		switch (cd->cmd_id) {
		case COMMAND_RABBIT: {
			int show_usage = 0;

			PLAYER_ID rpid = ud(cd)->rabbit_pid;

			if (rpid == PID_NONE) {	/* rabbit game is not runnig */
				if (cd->cmd_argc <= 1) {
					show_usage = 1;
				} else {
					char *cmd = cd->cmd_argr[1];
					int nargs = ArgCount(cmd, ':');

					if (nargs != 2 && nargs != 3) {
						show_usage = 1;
					} else {	/* args to command are valid */
						/* copy name out of command */
						char rname[20];
						DelimArgs(rname, sizeof(rname), cmd, 0, ':', 0);

						/* copy bounty out of command */
						int bty = AtoiArg(cmd, 1, ':');

						/* copy special out of command if it was specified */
						int special = 0;
						if (nargs == 3)
							special = AtoiArg(cmd, 2, ':');

						/* find the player specified on the command line */
						PLAYER *rabbit = FindPlayerName(rname, MATCH_HERE | MATCH_PREFIX);
						if (rabbit) {
							/* player found, start the game */
							start_rabbit_game(cd, rabbit, bty, special);
						} else {
							RmtMessageFmt(cd->cmd_name, "Player not found: %s", rname);
						}
					}
				}
			} else {
				/* rabbit game is already running */
				PLAYER *p = FindPlayerPid(rpid, MATCH_HERE);

				RmtMessageFmt(cd->cmd_name, "Rabbit is already running (%s is the rabbit)",
				    p ? p->name : "**UNKNOWN**");
			}

			/* display usage if necessary */
			if (show_usage) RmtMessageFmt(cd->cmd_name, "Usage: %s <name>:<bounty>[:<special>]", cd->cmd_argv[0]);
		}
		default: break;
		}
		break;
	case EVENT_UPDATE:
		if (ud(cd)->rabbit_pid == cd->p1->pid) {	/* if the update is for the rabbit */
			if (cd->p1->status & STATUS_SAFE) {	/* check if the rabbit is in safe */
				/* the rabbit entered safe, end the game */
				ArenaMessageFmt("%s has lost the rabbit game by entering safe!", cd->p1->name);
				ud(cd)->rabbit_pid = PID_NONE;
			}
		}
		break;
	case EVENT_TIMER:
		if (ud(cd)->rabbit_pid != PID_NONE) {	/* if the rabbit game is running */
			/* find the rabbit */
			PLAYER *p = FindPlayerPid(ud(cd)->rabbit_pid, MATCH_HERE);
			if (p) {
				if (ud(cd)->where_timer == cd->timer_id) {
					/* a *where timer expired, send the next one and look for response */
					PrivMessage(p, "*where");
					ud(cd)->expecting_where = 1;
				} else if (ud(cd)->gameover_timer == cd->timer_id) {
					/* the game over timer expired, the rabbit didnt die and he won */
					ArenaMessageFmt("%s wins the rabbit game by staying alive!", p->name);
					ud(cd)->rabbit_pid = PID_NONE;
				}
			} else {
				/* rabbit not found, this should never happen! */
			}
		}
	case EVENT_MESSAGE:
		/*
		 * Only look for a response of the game is running, the bot is expecting *where,
		 * and the message type is an arena message.
		 */
		if (ud(cd)->rabbit_pid != PID_NONE && ud(cd)->expecting_where && 
		    cd->msg_type == MSG_ARENA) {
			/* message is a possible *where response */

			/* find the rabbit */
			PLAYER *p = FindPlayerPid(ud(cd)->rabbit_pid, MATCH_HERE);
			if (p) {
				char where_prefix[32];
				snprintf(where_prefix, 32, "%s: ", p->name);
				where_prefix[32 - 1] = '\0';

				/*
				 * Verify that this is *where output by looking for "rabbit: " at the
				 * beginning of the string.
				 */
				if (strncasecmp(where_prefix, cd->msg, strlen(where_prefix)) == 0) {
					/* this must be a *where response, process it */
					char loc[4];
					snprintf(loc, 4, "%s", &cd->msg[strlen(where_prefix)]);
					loc[4 - 1] = '\0';

					/* announce the rabbits location */
					ArenaMessageFmt(">>> Rabbit %s is at %-3s <<<", p->name, loc);

					/* set the next *where timer */
					ud(cd)->where_timer = SetTimer(30 * 1000, 0, 0);

					/*
					 * The bot wont be looking for *where responses until the
					 * next time it sends the command, so turn parsing off
					 * for now.
					 */
					ud(cd)->expecting_where = 0;
				}
			}
		}
		break;
	case EVENT_KILL:
		if (cd->p1->pid == ud(cd)->rabbit_pid) {
			/* the rabbit died */
			ArenaMessageFmt("%s wins the rabbit game by killing Rabbit %s!", cd->p2->name, cd->p1->name);
			ud(cd)->rabbit_pid = PID_NONE;
		}
		break;
	case EVENT_LEAVE:
		if (ud(cd)->rabbit_pid == cd->p1->pid) {
			/* the rabbit left */
			ArenaMessageFmt("%s has lost the rabbit game by leaving the arena!", cd->p1->name);
			ud(cd)->rabbit_pid = PID_NONE;
		}
		break;
	case EVENT_CHANGE:
		if (ud(cd)->rabbit_pid == cd->p1->pid) {
			/* the rabbit changed ship */
			ArenaMessageFmt("%s has lost the rabbit game by changing ship/freq!", cd->p1->name);
			ud(cd)->rabbit_pid = PID_NONE;
		}
		break;
	}
}

