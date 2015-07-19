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

typedef struct player_info PLAYER_INFO;
struct player_info
{
	int	unused;
};

typedef struct user_data USER_DATA;
struct user_data
{
	int unused;
};

void GameEvent(CORE_DATA *cd);


#define CMD_NONE 0
#define CMD_QUERY 1

void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		RegisterPlugin(CORE_VERSION, "query", "cycad", "1.0", __DATE__, __TIME__, "Submit a query to database", sizeof(PLAYER_INFO));
		/* set the bot's thread-specific user_data */
		cd->user_data = malloc(sizeof(USER_DATA));

		RegisterCommand(CMD_QUERY, "!query", "query", 9, CMD_PRIVATE | CMD_REMOTE, NULL, "Submit a query to the database", NULL);
		break;
	case EVENT_STOP:
		free(cd->user_data);
		break;
	case EVENT_COMMAND:
		switch (cd->cmd_id) {
		case CMD_QUERY:
			if (cd->cmd_argc > 1) {
				Query(0, NULL, cd->cmd_name, cd->cmd_argr[1]);
				RmtMessage(cd->cmd_name, "Ok");
			} else {
				RmtMessage(cd->cmd_name, "Error");
			}
			break;
		default:
			break;
		}
		break;
	case EVENT_QUERY_RESULT:
		RmtMessageFmt(cd->query_name, "Success:%s Rows:%u Cols:%u", cd->query_success ? "Yes" : "No", cd->query_nrows, cd->query_ncols);
		for (size_t row = 0; row < cd->query_nrows; ++row) {
			char result[256];
			memset(result, 0, sizeof(result));
			for (size_t col = 0; col < cd->query_ncols; ++col) {
				if (strlen(result) > 0) strncat(result, ",", sizeof(result));
				strncat(result, cd->query_resultset[row][col], sizeof(result));
			}
			RmtMessage(cd->query_name, result);
		}
		break;
	}
}

#ifdef __cplusplus
}
#endif

