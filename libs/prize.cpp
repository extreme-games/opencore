
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "opencore.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_INTERVAL (30 * 1000)

#define CMD_PRIZES 1
#define CMD_CREATE 2
#define CMD_DELETE 3


#define QUERY_PRIZES 1
#define QUERY_CREATE 2
#define QUERY_DELETE 3

typedef struct player_info PLAYER_INFO;
struct player_info {
	ticks_ms_t last_command_tick;
};
inline PLAYER_INFO* pi(CORE_DATA *cd, PLAYER *p) { return (PLAYER_INFO*)GetPlayerInfo(cd, p); }

typedef struct user_data USER_DATA;
struct user_data
{
	int unused;
};



void
GameEvent(CORE_DATA *cd)
{
	char sfcmd_name[64];

	switch (cd->event) {
	case EVENT_START:
		RegisterPlugin(OPENCORE_VERSION, "prize", "cycad", "1.0", __DATE__, __TIME__, "Prizes", sizeof(USER_DATA), sizeof(PLAYER_INFO));

		RegisterCommand(CMD_PRIZES, "!prizes", "prize", 0, CMD_PRIVATE | CMD_PUBLIC, NULL, "List prizes available in the arena", NULL);
		RegisterCommand(CMD_CREATE, "!createprize", "prize", 5, CMD_PRIVATE | CMD_REMOTE, "<arena>:<name>:<cost>:<description>:<data>", "Create a new prize", NULL);
		RegisterCommand(CMD_DELETE, "!deleteprize", "prize", 5, CMD_PRIVATE | CMD_REMOTE, "<arena>:<name>", "Delete a prize", NULL);
		break;
	case EVENT_ENTER:
		pi(cd, cd->p1)->last_command_tick = GetTicksMs() - COMMAND_INTERVAL;
		break;
	case EVENT_STOP:
		break;
	case EVENT_COMMAND:
		QueryEscape(sfcmd_name, sizeof(sfcmd_name), cd->cmd_name);
		switch (cd->cmd_id) {
		case CMD_PRIZES:
			{
				char sfarena[32];
				QueryEscape(sfarena, sizeof(sfarena), cd->bot_arena);
				QueryFmt(QUERY_PRIZES, 0, cd->cmd_name, "CALL Prize_GetPrizes('%s');", sfarena);
			}
			break;
		case CMD_CREATE:
			if (cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') >= 5) {
				char arena[16];
				char name[24];
				char cost[32];
				char description[200];
				char data[200];

				DelimArgs(arena, sizeof(arena), cd->cmd_argr[1], 0, ':', false);
				DelimArgs(name, sizeof(name), cd->cmd_argr[1], 1, ':', false);
				DelimArgs(cost, sizeof(cost), cd->cmd_argr[1], 2, ':', false);
				DelimArgs(description, sizeof(description), cd->cmd_argr[1], 3, ':', false);
				DelimArgs(data, sizeof(data), cd->cmd_argr[1], 4, ':', true);

				char sfarena[32];
				char sfname[48];
				double costf = atof(cost);
				char sfdescription[400];
				char sfdata[400];

				QueryEscape(sfarena, sizeof(sfarena), arena);
				QueryEscape(sfname, sizeof(sfname), name);
				QueryEscape(sfdescription, sizeof(sfdescription), description);
				QueryEscape(sfdata, sizeof(sfdata), data);

				QueryFmt(QUERY_CREATE, 0, cd->cmd_name, "CALL Prize_CreatePrize('%s', '%s', %0.2f, '%s', '%s');",
						sfarena, sfname, costf, sfdescription, sfdata);
			} else {
				Reply("Usage: !createprize <arena>:<name>:<cost>:<description>:<data>");
			}
			break;
		case CMD_DELETE:
			if (cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') >= 2) {
				char arena[16];
				char name[24];

				DelimArgs(arena, sizeof(arena), cd->cmd_argr[1], 0, ':', false);
				DelimArgs(name, sizeof(name), cd->cmd_argr[1], 1, ':', false);

				char sfarena[32];
				char sfname[48];

				QueryEscape(sfarena, sizeof(sfarena), arena);
				QueryEscape(sfname, sizeof(sfname), name);

				QueryFmt(QUERY_DELETE, 0, cd->cmd_name, "CALL Prize_DeletePrize('%s', '%s');", sfarena, sfname);
			} else {
				Reply("Usage: !deleteprize <arena>:<name>");
			}
			break;
		default:
			break;
		}
		break;
	case EVENT_QUERY_RESULT:
		if (!cd->query_success) return;
		switch (cd->query_type) {
			case QUERY_PRIZES:
				if (cd->query_nrows >= 1 && cd->query_ncols == 4) {
					RmtMessageFmt(cd->query_name, "+---Name-----+--Cost--+-Description---------------------------------------------------------------+");
					for (size_t i = 0; i < cd->query_nrows; ++i) {
						char *name = cd->query_resultset[i][0];	
						char *cost = cd->query_resultset[i][1];	
						char *desc = cd->query_resultset[i][2];	
						double costf = atof(cost);
						char buf[32] = { '\0' };
						snprintf(buf, sizeof(buf), "$%0.2f", costf);
						RmtMessageFmt(cd->query_name, "| %-10s | %6s | %-73s |", name, buf, desc);
					}
					RmtMessageFmt(cd->query_name, "+------------+--------+---------------------------------------------------------------------------+");
				} else {
					RmtMessageFmt(cd->query_name, "No prizes available in this arena");
				}
				break;
			case QUERY_CREATE:
				if (cd->query_nrows == 1 && cd->query_ncols == 1 && cd->query_resultset[0][0] && strcasecmp(cd->query_resultset[0][0], "1") == 0) {
					RmtMessage(cd->query_name, "Prize added");
				} else {
					RmtMessage(cd->query_name, "Unable to add prize");
				}
				break;
			case QUERY_DELETE:
				if (cd->query_nrows == 1 && cd->query_ncols == 1 && cd->query_resultset[0][0] && strcasecmp(cd->query_resultset[0][0], "1") == 0) {
					RmtMessage(cd->query_name, "Prize deleted");
				} else {
					RmtMessage(cd->query_name, "Unable to delete prize");
				}
				break;
			default:
				break;
		}
		break;
	}
}

#ifdef __cplusplus
}
#endif

