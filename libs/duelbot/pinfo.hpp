
#pragma once

#include "opencore.hpp"

#define MAX_ASSISTS 10

struct PlayerInfo {
	bool pinfo_reused;		// TRUE if this pinfo is being reused (player left arena and reentered)

	// per-duel stats
	struct {
		uint16_t ckills;		// consecutive kills at the moment
		uint16_t mostckills;	// total consecutive kills (spree)
		uint16_t kills;			// total kills
		uint16_t deaths;		// total deaths
		uint32_t dt;			// damage taken
		uint32_t dg;			// damage given
		uint32_t assists;		// assists
	} stats[1];

	struct {
		uint16_t pid;
		uint32_t ticks;
	} last_damage[MAX_ASSISTS];

	struct {
		struct {
			uint32_t kills;		// total kills
			uint32_t deaths;		// total deaths

			uint32_t wins;			// total wins
			uint32_t losses;		// total losses
		} rated[1];

		struct {
			uint32_t kills;
			uint32_t deaths;

			uint32_t wins;
			uint32_t losses;
		} unrated[1];

		struct {
			uint32_t kills;
			uint32_t deaths;
		} pub[1];
	} all_stats[1];

	// general duel bot flags
	bool noduel_enabled;			// does this player have dueling disabled?
	bool allowed_to_change_freq;	// freq change is ok (player is being setfreq'ed away from a duel he isnt allowed in)

	// flags pertaining to the current duel
	bool gotsafed;	// has this player been safed in the current duel?
	bool nfdw;		// needs fast duel warp on respawn
	bool waskilled;	// set in duels to let the bot know a player is in center
					// because they were killed (as opposed to being warped or warping)

	// for purposes of limiting commands and *commands, both in the duel and otherwise
	uint32_t lastwarptick;			// last warp tick
	uint32_t lastchallengetick;	// last challenge tick
	uint32_t lastcenterwarptotick;	// last warp to center tick
	uint32_t lastantitakentick;	// last tick of anti being taken
	uint32_t lastcenterareaupdatetick;	// last update packet that was from a safezone tick

	int elo;		// elo rating
	int temp_elo;	// scratch space for computing elo (see duel_computestats())
	int max_elo;	// max elo rating

	int pub_elo;	// pub elo rating
	int max_pub_elo;// max elo rating in pub

	bool stats_from_db;	// set if the stats were retrieved from db
};


inline
PlayerInfo*
pi(CORE_DATA *cd, PLAYER *p)  {
	return (PlayerInfo*)GetPlayerInfo(cd, p);
}

void pi_clear(CORE_DATA *cd, PLAYER *p);
