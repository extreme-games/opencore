
#include <assert.h>

#include "opencore.hpp"

#include "commands.hpp"
#include "util.hpp"
#include "globals.hpp"
#include "userdata.hpp"
#include "pinfo.hpp"
#include "handlers.hpp"
#include "duel.hpp"


//
// the beast
//
extern "C"
void
GameEvent(CORE_DATA *cd)
{
	try {
		switch (cd->event) {
		case EVENT_START:		handleStart(cd);	break;
		case EVENT_STOP:		handleStop(cd);	break;
		case EVENT_LOGIN:		handleLogin(cd);	break;
		case EVENT_COMMAND:
			switch(cd->cmd_id) {
				case CMD_LOCKDUEL:		cmd_LockDuel(cd);			break;
				case CMD_DUELINFO:		cmd_DuelInfo(cd);			break;
				case CMD_DUELHELP:		cmd_DuelHelp(cd);			break;
				case CMD_NODUEL:		cmd_NoDuel(cd);			break;
				case CMD_SCORERESET:	cmd_ScoreReset(cd);		break;
				case CMD_ELORESET:		cmd_EloReset(cd);			break;
				case CMD_SLOWCHALLENGE:	cmd_Challenge(cd, true, false);	break;
				case CMD_UCHALLENGE:	cmd_Challenge(cd, true, false);	break;
				case CMD_CHALLENGE:		cmd_Challenge(cd, false, true);	break;
				case CMD_ACCEPT:		cmd_Accept(cd);			break;
				case CMD_REJECT:		cmd_Reject(cd);			break;
				case CMD_RATING:		cmd_Rating(cd, false);	break;
				case CMD_RANK:			cmd_Rank(cd, false);		break;
				case CMD_TOP10:			cmd_Top10(cd, false);		break;
				case CMD_PUBRATING:		cmd_Rating(cd, true);		break;
				case CMD_PUBRANK:		cmd_Rank(cd, true);		break;
				case CMD_PUBTOP10:		cmd_Top10(cd, true);		break;
				case CMD_STATS:			cmd_Stats(cd);			break;
			}
			break;
		case EVENT_CONFIG_CHANGE:	handleIniChange(cd);	break;
		case EVENT_ATTACH:		handleAttach(cd);	break;
		case EVENT_ENTER:		handleEnter(cd);	break;
		case EVENT_UPDATE:		handleUpdate(cd);	break;
		case EVENT_CHANGE:		handleChange(cd); break;
		case EVENT_TIMER:		handleTimer(cd);	break;
		case EVENT_LEAVE:		handleLeave(cd);	break;
		case EVENT_KILL:		handleKill(cd);	break;
		//case EVENT_DAMAGE:		handleDamage(cd);	break;
		case EVENT_QUERY_RESULT:	handleQueryResult(cd);	break;
		case EVENT_DEADSLOT:	handleDeadSlot(cd);	break;
		}
	} catch (...) {
		LogFmt(OP_SMOD, "Unknown critical exception thrown in Event %d, shutting down.", cd->event);
		StopBot("Critical exception thrown");
	}
}
