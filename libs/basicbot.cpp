/*
 * A simple bot that welcomes you to an arena.
 */

#include "libopencore.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void GameEvent(CORE_DATA *cd);

LIB_DATA REGISTRATION_INFO = {
	"basicbot",
	"cycad",
	"1.0",
	__DATE__,
	__TIME__,
	"A basic demonstration bot.",
	CORE_VERSION,
	0,
	GameEvent
};

void
CmdHello(CORE_DATA *cd)
{
	/*
	 * RmtMessage() is used for a reply. It automatically converts between
	 * remote and private messages.
	 */
	RmtMessage(cd->cmd_name, "Hello.");
	PLAYER *p;
	for (p = cd->here_first; p; p = p->here_next) {
		RmtMessageFmt(cd->cmd_name, "Here: %-20s (Ship: %d) (Freq: %hu)", p->name, p->ship, p->freq);
	}
	RmtMessageFmt(cd->cmd_name, "Here Count: %d", cd->here_count);

	for (p = cd->spec_first; p; p = p->spec_next) {
		RmtMessageFmt(cd->cmd_name, "Spec: %-20s (Ship: %d) (Freq: %hu)", p->name, p->ship, p->freq);
	}
	RmtMessageFmt(cd->cmd_name, "Spec Count: %d", cd->spec_count);

	for (p = cd->ship_first; p; p = p->ship_next) {
		RmtMessageFmt(cd->cmd_name, "Ship: %-20s (Ship: %d) (Freq: %hu)", p->name, p->ship, p->freq);
	}
	RmtMessageFmt(cd->cmd_name, "Ship Count: %d", cd->ship_count);
}

void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		RegisterCommand("!helloa", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "A command from a library.",
		    NULL,
		    CmdHello
		    );
		RegisterCommand("!hellob", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "A command from a library.",
		    NULL,
		    CmdHello
		    );
		RegisterCommand("!hello", "Test", 0,
		    CMD_PUBLIC | CMD_PRIVATE | CMD_REMOTE,
		    NULL,
		    "A command from a library.",
		    NULL,
		    CmdHello
		    );
		break;
	case EVENT_ENTER:
		break;
	}
}

#ifdef __cplusplus
}
#endif

