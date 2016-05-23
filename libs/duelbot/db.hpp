
#pragma once

#include "opencore.hpp"

#define DUELQUERY_QUERYIGNORE 0
#define DUELQUERY_ELOCHECK 1
#define DUELQUERY_RETRIEVESTATS 2
#define DUELQUERY_TOP10 3
#define DUELQUERY_RANK 4
#define DUELQUERY_STATSCHECK 5
#define DUELQUERY_ELORESET 6

void db_addnewplayer(CORE_DATA *cd, PLAYER *p);
void db_storeplayersdata(CORE_DATA *cd, PLAYER *p);
void db_initdb(CORE_DATA *cd);
void db_selectplayersdata(CORE_DATA *cd, char *name);
void db_elocheck(CORE_DATA *cd, char *requester, char *target, bool pub);
void db_updatepubstats(CORE_DATA *cd, PLAYER *p);
void db_top10(CORE_DATA *cd, char *requester, bool pub);
void db_rank(CORE_DATA *cd, char *requester, bool pub, int elo);
void db_statscheck(CORE_DATA *cd, char *requester, char *target);
void db_eloreset(CORE_DATA *cd, char *requester, char *target);
