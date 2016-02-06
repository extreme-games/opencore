
#pragma once

#include <list>
#include <ctime>

#include "dueltypes.hpp"
#include "globals.hpp"


struct UserData {
	// arrays of duel circles and challenges
	DUEL_CIRCLE *duels;
	CHALLENGE *ch;

	// center safe upper left & lower right
	uint16_t CSX1;
	uint16_t CSY1;
	uint16_t CSX2;
	uint16_t CSY2;

	// center safe upper left & lower right for *warpto
	uint16_t CSWTX1;
	uint16_t CSWTY1;
	uint16_t CSWTX2;
	uint16_t CSWTY2;

	// graveyard coordinates
	uint16_t GYX1;
	uint16_t GYY1;
	uint16_t GYX2;
	uint16_t GYY2;
	
	// no-anti boundary coordinates
	uint16_t NABX1;
	uint16_t NABY1;
	uint16_t NABX2;
	uint16_t NABY2;

	// where the bot starts
	uint16_t BOTX;
	uint16_t BOTY;

	// is dueling currently locked?
	bool LOCKED;

	// values loaded by config
	char DBFILE[64];				// database config file
	bool USEDB;						// use db?
	char DBPREFIX[64];				// db name to use
	uint32_t ENTERDELAY;				// an adjustment to kill timers
	uint32_t KILLTIMER;				// the how long to wait after a respawn before checking if all players are dead
	uint32_t SMALLTEAMKILLTIMER;		// the same as above, but for 1v1 duels
	uint32_t GONETIME;					// how long a player has to be out of safe for to be counted as out of safe
	uint32_t SMALLTEAMGONETIME;		// the same as above, but for 1v1 duels
	bool SWAPPEDSIDESENDSDUEL;		// if a team hits the other teams spawn point does the duel end?
	bool SHIPRESETONGYWARP;			// when warping out of the graveyard, reset ships?
	bool SWAPCENTERANDGRAVEYARD;	// to prevent being warped back to graveyard after lag attaching,
									// the arena can be configured so players spawn in the graveyard
									// making the warp unnecessary. in the situation the bot warps
									// players out of the graveyard to the real center safe
	bool REMOVEANTI;				// remove antiwarp if its enabled in the center area?
	uint16_t SPAWNPOINTRADIUS;			// how big are the spawn points? any update within the radius of
									// the spawn point means their opponent got safed
	int MAXBYVAL;					// max win-by value
	bool WAITFORPOSPACKET;			// wait for a position update (on respawn) to send *warpto?
									// or just send it immediately on the kill event?
	int NUMCIRCLES;					// the total number of duel circles and challenges
	int MAXPERTEAM;					// the max # of players allowed on a single team
	int MAXROUNDS;					// the max # of rounds allowd in a duel
	bool SENDWATCHDAMAGE;			// send watchdamage to players who enter?
	int PLAYERSFORARENASPEW;		// how many players total in a duel for *arena stats?
	bool DEFAULT1V1FAST;			// do 1v1 duels fast by default?
	uint32_t ASSISTINTERVAL;			// how long after doing damage does it no longer count towards an assist?
	uint32_t NCINTERVAL;				// grace period to ignore a no-score duel result if a duel lasts less than this interval
};


inline
UserData*
ud(CORE_DATA *data) {
	return (UserData *)(data->user_data);
}


void ud_init(CORE_DATA *data);
void ud_delete(CORE_DATA *data);
void ud_loadconfig(CORE_DATA *data);
void ud_reloadconfig(CORE_DATA *data);
void ud_loadcommon(CORE_DATA *data);

void ud_warptograveyard(CORE_DATA *data, PLAYER *p);
void ud_warptocenterwithwarpto(CORE_DATA *data, PLAYER *p);

bool ud_iscentersafe(CORE_DATA *data, uint16_t x, uint16_t y);
bool ud_isduelcircle(CORE_DATA *data, uint16_t x, uint16_t y);
bool ud_isgraveyard(CORE_DATA *data, uint16_t x, uint16_t y);
bool ud_iscenterarea(CORE_DATA *data, uint16_t x, uint16_t y);
bool ud_isnoantiboundary(CORE_DATA *data, uint16_t x, uint16_t y);

DUEL* ud_getduelbypid(CORE_DATA *data, uint16_t pid);
DUEL* ud_getduelbyfreq(CORE_DATA *data, uint16_t freq);
DUEL* ud_getopenduelbyname(CORE_DATA *data, const char *name);
DUEL* ud_getopenduel(CORE_DATA *data);

CHALLENGE* ud_getopenchallenge(CORE_DATA *data);
CHALLENGE* ud_getchallengebyfreq(CORE_DATA *data, uint16_t freq);
