
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

#define CMD_BALANCE 1
#define CMD_SETBALANCE 2
#define CMD_TRANSFER 3
#define CMD_ADD 4
#define CMD_REMOVE 5


#define QUERY_BALANCE 1
#define QUERY_SETBALANCE 2
#define QUERY_TRANSFER 3
#define QUERY_ADD 4
#define QUERY_REMOVE 5

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
		RegisterPlugin(OPENCORE_VERSION, "egc", "cycad", "1.0", __DATE__, __TIME__, "Prizes", sizeof(USER_DATA), sizeof(PLAYER_INFO));

		RegisterCommand(CMD_BALANCE, "!balance", "egc", 0, CMD_PRIVATE | CMD_PUBLIC, NULL, "Check your balance", NULL);
		RegisterCommand(CMD_SETBALANCE, "!setbalance", "egc", 2, CMD_PRIVATE | CMD_REMOTE, "<player>:<balance>:<comment>", "Set someones account balance", NULL);
		RegisterCommand(CMD_TRANSFER, "!transfer", "egc", 0, CMD_PRIVATE | CMD_PUBLIC, NULL, "Transfer money between players", NULL);
		RegisterCommand(CMD_ADD, "!add", "egc", 2, CMD_PRIVATE, NULL, "Check your balance", NULL);
		RegisterCommand(CMD_REMOVE, "!remove", "egc", 2, CMD_PRIVATE, NULL, "Remove money from an account", NULL);
		break;
	case EVENT_ENTER:
		pi(cd, cd->p1)->last_command_tick = GetTicksMs() - COMMAND_INTERVAL;
		break;
	case EVENT_STOP:
		break;
	case EVENT_COMMAND:
		QueryEscape(sfcmd_name, sizeof(sfcmd_name), cd->cmd_name);
		switch (cd->cmd_id) {
		case CMD_BALANCE:
			if (cd->cmd_argc <= 1) {
				QueryFmt(QUERY_BALANCE, 0, cd->cmd_name, "CALL Egc_Balance('%s');", sfcmd_name);
			} else if (strlen(cd->cmd_argr[1]) < 24) {
				char sfname[48];
				QueryEscape(sfname, sizeof(sfname), cd->cmd_argr[1]);
				QueryFmt(QUERY_BALANCE, 0, cd->cmd_name, "CALL Egc_Balance('%s');", sfname);
			} else {
				Reply("Usage: !balance [player]");
			}
			break;
		case CMD_SETBALANCE:
			if (cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') >= 3) {
				char name[24];
				char sfname[48];
				char balance[32];
				char comment[200];
				char sfcomment[400];

				DelimArgs(name, sizeof(name), cd->cmd_argr[1], 0, ':', false);
				QueryEscape(sfname, sizeof(sfname), name);

				DelimArgs(balance, sizeof(balance), cd->cmd_argr[1], 1, ':', false);
				double balancef = atof(balance);

				DelimArgs(comment, sizeof(comment), cd->cmd_argr[1], 2, ':', true);
				QueryEscape(sfcomment, sizeof(sfcomment), comment);

				QueryFmt(QUERY_SETBALANCE, 0, cd->cmd_name, "CALL Egc_SetBalance('%s', %0.2f, '%s: %s');",
						sfname, balancef, sfcmd_name, strlen(sfcomment) > 0 ? sfcomment : "No comment");
				ReplyFmt("Setting balance for \"%s\" to $%0.2f...", name, balancef);
			} else {
				Reply("Usage: !setbalance <player>:<balance>:<comment>");
			}
			break;
		case CMD_TRANSFER:
			if (cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') == 2) {
				char dest[24];
				char sfdest[48];
				char amount[32];

				DelimArgs(amount, sizeof(amount), cd->cmd_argr[1], 0, ':', false);
				double amountf = atof(amount);

				DelimArgs(dest, sizeof(dest), cd->cmd_argr[1], 1, ':', true);
				QueryEscape(sfdest, sizeof(sfdest), dest);

				QueryFmt(QUERY_TRANSFER, 0, cd->cmd_name, "CALL Egc_Transfer('%s', '%s', %0.2f, NULL);",
						sfcmd_name, sfdest, amountf);
				ReplyFmt("Transferring $%0.2f from \"%s\" to \"%s\"...", amountf, cd->cmd_name, dest);
			} else if (cd->cmd_level >= 2 && cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') >= 4) {
				char source[24];
				char sfsource[48];
				char amount[32];
				char dest[24];
				char sfdest[48];
				char comment[200];
				char sfcomment[400];

				DelimArgs(source, sizeof(source), cd->cmd_argr[1], 0, ':', false);
				DelimArgs(amount, sizeof(amount), cd->cmd_argr[1], 1, ':', false);
				DelimArgs(dest, sizeof(dest), cd->cmd_argr[1], 2, ':', false);
				DelimArgs(comment, sizeof(comment), cd->cmd_argr[1], 3, ':', true);

				QueryEscape(sfsource, sizeof(sfsource), source);
				double amountf = atof(amount);
				QueryEscape(sfdest, sizeof(sfdest), dest);
				QueryEscape(sfcomment, sizeof(sfcomment), comment);

				QueryFmt(QUERY_TRANSFER, 0, cd->cmd_name, "CALL Egc_Transfer('%s', '%s', %0.2f, '%s: %s');",
						sfsource, amountf, sfdest, sfcmd_name, sfcomment);
				ReplyFmt("Transferring $%0.2f from \"%s\" to \"%s\"...", amountf, cd->cmd_name, dest);
			} else {
				Reply("Usage: !transfer <amount>:<destination>");
				if (cd->cmd_level >= 2) Reply("Usage: !transfer <source>:<amount>:<destination>:<comment>");
			}
			break;
		case CMD_ADD:
		case CMD_REMOVE:
			if (cd->cmd_argc > 1 && ArgCount(cd->cmd_argr[1], ':') >= 3) {
				char target[24];
				char sftarget[48];
				char amount[32];
				char comment[200];
				char sfcomment[400];

				DelimArgs(target, sizeof(target), cd->cmd_argr[1], 0, ':', false);
				DelimArgs(amount, sizeof(amount), cd->cmd_argr[1], 1, ':', false);
				double amountf = atof(amount);
				DelimArgs(comment, sizeof(comment), cd->cmd_argr[1], 2, ':', true);

				int query_type = QUERY_ADD;
				const char *spname = "Egc_Add";
				const char *verb = "Adding";
				if (cd->cmd_id == CMD_REMOVE) {
					query_type = QUERY_REMOVE;
					spname = "Egc_Remove";
					verb = "Removing";
				}
				QueryFmt(query_type, 0, cd->cmd_name, "CALL %s('%s', %0.2f, '%s: %s');",
						spname, sftarget, amountf, sfcmd_name, sfcomment);
				ReplyFmt("%s $%0.2f from \"%s\"...", verb, amountf, target);
			} else {
				ReplyFmt("Usage: %s <target>:<amount>:<comment>", cd->cmd_id == CMD_ADD ? "!add" : "!remove");
			}
			break;
		default:
			break;
		}
		break;
	case EVENT_QUERY_RESULT:
		if (!cd->query_success) return;
		switch (cd->query_type) {
			case QUERY_BALANCE:
				if (cd->query_nrows == 1 && cd->query_ncols == 1) {
					char *balance = cd->query_resultset[0][0];	
					double balancef = atof(balance ? balance : "0");
					RmtMessageFmt(cd->query_name, "Balance: $%0.2f", balancef);
				}
				break;
			case QUERY_SETBALANCE:
				printf("rows:%d cols:%d value:%s\n", cd->query_nrows, cd->query_ncols, cd->query_resultset[0][0]);
				if (cd->query_nrows == 1 && cd->query_ncols == 1 && cd->query_resultset[0][0] && strcasecmp(cd->query_resultset[0][0], "1") == 0) {
					RmtMessage(cd->query_name, "Balance set");
				} else {
					RmtMessage(cd->query_name, "Unable to set balance");
				}
				break;
			case QUERY_TRANSFER:
				if (cd->query_nrows == 1 && cd->query_ncols == 1 && cd->query_resultset[0][0] && strcasecmp(cd->query_resultset[0][0], "1") == 0) {
					RmtMessage(cd->query_name, "Transfer complete");
				} else {
					RmtMessage(cd->query_name, "Transfer failed");
				}
				break;
			case QUERY_ADD:
			case QUERY_REMOVE:
				if (cd->query_nrows == 1 && cd->query_ncols == 1 && cd->query_resultset[0][0] && strcasecmp(cd->query_resultset[0][0], "1") == 0) {
					RmtMessageFmt(cd->query_name, "%s complete", cd->query_type == QUERY_ADD ? "Addition" : "Removal");
				} else {
					RmtMessageFmt(cd->query_name, "%s failed", cd->query_type == QUERY_ADD ? "Addition" : "Removal");
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

