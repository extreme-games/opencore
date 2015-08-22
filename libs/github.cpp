/*
 * A test plugin demonstrating and explaining the opencore's features.
 */
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opencore.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct user_data USER_DATA;
struct user_data
{
	ticks_ms_t last_poll_tick;
	char gitpath[256];

	bool state;
};

#define STATE_READY 0
#define STATE_FETCH 1
#define STATE_PULL 2
#define STATE_MERGE 3
#define STATE_UPLOAD 4

static inline
USER_DATA *
ud(CORE_DATA *cd)
{
	return (USER_DATA*)cd->user_data;
}

// git fetch
// git pull
// git merge origin/master

static
bool
spawn_proc(pid_t *result, const char *file, const char *arg1, const char *arg2, const char *arg3)
{
	pid_t pid = fork();
	if (pid == -1) {
		return false;
	} else if (pid == 0) { /* child */
		execl(file, arg1, arg2, arg3, NULL);
	} else { /* parent */

	}
	
	*result = pid;
	return true;
}


void
CmdPull
{
	if (ud(cd)->state != STATE_READY) {
		Reply("GitHub pull already in progress");
		return;
	}

	if (spawn_proc(&ud(cd)->pid, ud(cd)->gitpath, "fetch", NULL, NULL)) {
		ud(cd)->state = STATE_FETCH;
	} else {
		Reply("Unable to spawn GitHub command");
	}
}


void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		RegisterPlugin(CORE_VERSION, "github", "cycad", "0.1", __DATE__, __TIME__, "GitHub Integration", sizeof(PLAYER_INFO));
		cd->user_data = malloc(sizeof(USER_DATA));

		RegisterCommand("!pull", "github", 9,
		    CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "Pull changes from GitHub to local repository and upload to server",
		    NULL,
		    CmdPull
		    );
		
		GetConfigString("github.gitpath", ud(data)->gitpath, sizeof(ud(data)->gitpath), "git");
		break;
	case EVENT_TICK:
		if (GetTicksMs() - ud(cd)->last_poll_tick >= 1000) {
			// poll process state
			ud(cd)->last_poll_tick = GetTicksMs();
		}
	case EVENT_STOP:
		free(cd->user_data);
		break;
	case EVENT_TRANSFER:
		break;
	}
}

#ifdef __cplusplus
}
#endif

