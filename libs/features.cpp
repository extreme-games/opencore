/*
 * A test plugin demonstrating and explaining the opencore's features.
 */
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

#include "libopencore.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A player info struct is used to hold per-player data. The structs
 * are maintained internally by the core. EVENT_FIRSTSEEN is the 
 * event when a player is seen for the first time and his pinfo struct
 * has just been allocated and zeroed.
 *
 * EVENT_DEADSLOT is when a player's struct is no longer used and is
 * freed by the core.  This occurs 1 hour after a player was last seen.
 *
 * LIB_PINFO_SIZE must be set to the size of this structure.
 */
typedef struct player_info PLAYER_INFO;
struct player_info
{
	int	 kills;
	void	*ptr;
};

/*
 * Since modules are reentrant, global variables are accessible to
 * any thread.  user_data is a thread-specific storage area for
 * values that are global to a thread.
 *
 * This should be allocated in EVENT_START and freed in EVENT_STOP.
 */
typedef struct user_data USER_DATA;
struct user_data
{
	void	*example;
};

/*
 * Each of these must be set by the library. The names are self-
 * explanatory.
 */
char	LIB_NAME[]		= "Test";
char	LIB_AUTHOR[]		= "cycad";
char	LIB_VERSION[]		= "1.0";
char	LIB_DATE[]		= __DATE__;
char	LIB_TIME[]		= __TIME__;
char	LIB_DESCRIPTION[]	= "A test library.";
char	LIB_TARGET[]		= CORE_VERSION;
int	LIB_PINFO_SIZE		= sizeof(PLAYER_INFO);

/*
 * The callback for !hello which simply replies with "Hello."
 */
void
CmdHello(CORE_DATA *cd)
{
	/*
	 * RmtMessage() is used for a reply. It automatically converts between
	 * remote and private messages.
	 */
	RmtMessage(cd->cmd_name, "Hello.");
}

/*
 * The callback for !kills which displays the number of kills a player has
 * had since the bot entered the arena.
 */
void
CmdHereList(CORE_DATA *cd)
{
	for (PLAYER *p = cd->here_first; p != NULL; p = p->here_next) {
		RmtMessage(cd->cmd_name, p->name);
	}
}

/*
 * The callback for !kills which displays the number of kills a player has
 * had since the bot entered the arena.
 */
void
CmdKills(CORE_DATA *cd)
{
	PLAYER *p;
	if (cd->cmd_argc <= 1) {
		RmtMessageFmt(cd->cmd_name, "Usage: %s <player>", cd->cmd_argv[0]);
	} else if ((p = FindPlayerName(cd->cmd_argr[1], MATCH_HERE | MATCH_PREFIX)) == NULL) {
		/* argr is the argument and everything after it */
		RmtMessageFmt(cd->cmd_name, "Player not found");
	} else {
		RmtMessageFmt(cd->cmd_name, "Kills: %d",
		    ((PLAYER_INFO*)GetPlayerInfo(cd, p))->kills);
	}
}

void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		/* set the bot's thread-specific user_data */
		cd->user_data = malloc(sizeof(USER_DATA));

		/* register !hello */
		RegisterCommand("!hello", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "A command from a library.",
		    NULL,
		    CmdHello
		    );

		RegisterCommand("!kills", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    "<player>",
		    "See how many kills a player has",
		    NULL,
		    CmdKills
		    );

		RegisterCommand("!herelist", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "Display players in the arena",
		    NULL,
		    CmdHereList
		    );

		printf("event_start\n");

		/* Set some example timers */
		SetTimer(5000,  (void*)1 , (void*)2);
		SetTimer(10000,  (void*)3 , (void*)4);
		SetTimer(15000,  (void*)5 , (void*)6);
		break;
	case EVENT_CHANGE:
		{
			/* player p1 changed ship */
			PLAYER *p = cd->p1;
			ArenaMessageFmt("%s: freq %hu -> %hu\n", p->name, cd->old_freq, p->freq);
			ArenaMessageFmt("%s: ship %hu -> %hu\n", p->name, cd->old_ship, p->ship);
		}
		break;
	case EVENT_ALERT:
			printf("event_alert: %s %s %s %s\n", cd->alert_type,
			    cd->alert_name, cd->alert_arena, cd->alert_msg);
		break;
	case EVENT_MESSAGE:
		/* 
		 * Be careful not to end up in an infinite loop by sending messages
		 * from EVENT_MESSAGE, as this will generate another EVENT_MESSAGE.
		 */
		if (cd->msg_type == MSG_PUBLIC) 
			printf("%s> %s\n", cd->msg_name, cd->msg);
		else if (cd->msg_type == MSG_PRIVATE) 
			printf("%s> %s\n", cd->msg_name, cd->msg);
		else if (cd->msg_type == MSG_REMOTE) 
			printf("(%s)> %s\n", cd->msg_name, cd->msg);
		else if (cd->msg_type == MSG_ARENA) 
			printf("%s\n", cd->msg);
		break;
	case EVENT_TIMER:
		/* this happens when a timer set by SetTimer() expires */
		ArenaMessageFmt("event_timer: %ld      d1: %lu  d2: %lu \n", cd->timer_id,
		    (long)cd->timer_data1, (long)cd->timer_data2);
		break;
	case EVENT_KILL:
		ArenaMessageFmt("event_kill: %s killed by %s\n", cd->p1->name, cd->p2->name);
		((PLAYER_INFO*)GetPlayerInfo(cd, cd->p2))->kills++;
		break;
	case EVENT_UPDATE:
		ArenaMessageFmt("event_update: %s: %hu,%hu Status: %d%d%d%d%d%d%d%d\n", cd->p1->name, cd->p1->pos->x, cd->p1->pos->y,
		    (cd->p1->status & 0x80) ? 1 : 0,
		    (cd->p1->status & 0x40) ? 1 : 0,
		    (cd->p1->status & 0x20) ? 1 : 0,
		    (cd->p1->status & 0x10) ? 1 : 0,
		    (cd->p1->status & 0x08) ? 1 : 0,
		    (cd->p1->status & 0x04) ? 1 : 0,
		    (cd->p1->status & 0x02) ? 1 : 0,
		    (cd->p1->status & 0x01) ? 1 : 0
		    );
		break;
	case EVENT_STOP:
		free(cd->user_data);
		ArenaMessageFmt("event_stop\n");
		break;
	case EVENT_FIRSTSEEN:
		((PLAYER_INFO*)GetPlayerInfo(cd, cd->p1))->ptr = malloc(4);
		ArenaMessageFmt("event_firstseen: %s\n", cd->p1->name);
		break;
	case EVENT_DEADSLOT:
		free(((PLAYER_INFO*)GetPlayerInfo(cd, cd->p1))->ptr);
		ArenaMessageFmt("event_deadslot: %s\n", cd->p1->name);
		break;
	case EVENT_ENTER:
		ArenaMessageFmt("event_enter: %s\n", cd->p1->name);
		break;
	case EVENT_LEAVE:
		ArenaMessageFmt("event_leave: %s\n", cd->p1->name);
		break;
	case EVENT_TICK:
		/* commented out for loudness */
		/* ArenaMessageFmt("event_ticks: %X", GetTicksMs()); */
		break;
	}
}

#ifdef __cplusplus
}
#endif

