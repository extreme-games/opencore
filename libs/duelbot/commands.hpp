
#pragma once

#include "opencore.hpp"

//
// command handlers
//
void cmd_DuelInfo(CORE_DATA *cd);
void cmd_DuelHelp(CORE_DATA *cd);
void cmd_NoDuel(CORE_DATA *cd);
void cmd_ScoreReset(CORE_DATA *cd);
void cmd_Challenge(CORE_DATA *cd, bool slow, bool rated);
void cmd_Accept(CORE_DATA *cd);
void cmd_Reject(CORE_DATA *cd);
void cmd_LockDuel(CORE_DATA *cd);
void cmd_Rating(CORE_DATA *cd, bool pub);
void cmd_Top10(CORE_DATA *cd, bool pub);
void cmd_Rank(CORE_DATA *cd, bool pub);
void cmd_Stats(CORE_DATA *cd);
void cmd_EloReset(CORE_DATA *cd);
