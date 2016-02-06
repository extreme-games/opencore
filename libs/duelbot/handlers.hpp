
#pragma once

#include "opencore.hpp"

//
// event handlers
//
void handleCommand(CORE_DATA *cd);
void handleTick(CORE_DATA *cd);
void handleEnter(CORE_DATA *cd);
void handleLeave(CORE_DATA *cd);
void handleChange(CORE_DATA *cd);
void handleQueryResult(CORE_DATA *cd);
void handleTimer(CORE_DATA *cd);
void handleKill(CORE_DATA *cd);
void handleUpdate(CORE_DATA *cd);
void handleDamage(CORE_DATA *cd);
void handleStart(CORE_DATA *cd);
void handleStop(CORE_DATA *cd);
void handleLogin(CORE_DATA *cd);
void handleAttach(CORE_DATA *cd);
void handleIniChange(CORE_DATA *cd);
void handleDeadSlot(CORE_DATA *cd);
