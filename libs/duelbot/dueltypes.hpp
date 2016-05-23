
#pragma once

#include "opencore.hpp"

#include "globals.hpp"
#include "pinfo.hpp"
#include "util.hpp"

#define TEAM1 0
#define TEAM2 1

typedef struct duel DUEL;
typedef struct duel_circle DUEL_CIRCLE;

struct duel_circle {
	int x[2];		// x coords for both teams
	int y[2];		// y coords for both teams

	char name[32];	// name of the duel circle

	DUEL *duel;		// the duel being held in this circle, NULL if none
};
void dc_warpteams(CORE_DATA *cd, DUEL_CIRCLE *dc);

struct duel {
	uint16_t	freq[2];		// freqs
	int		score[2];		// scores
	int		size[2];		// team size
	char	names[2][256];	// names list

	bool	use_elo;		// use elo? is this duel rated?
	int		avg_elo[2];		// avg elo

	int		is_fast;	// TRUE if fast, FALSE if slow
	int		win_by;		// teams must win by this number
	int		num_rounds;	// number of rounds
	int		timer;		// timer for safe checks
	int		in_use;		// set if the duel is in use -- this should be moved to DUEL_CIRCLE
	int		warpcount;	// number of warps (used to alternate sides teams start on)
	uint32_t	start_tick;	// tick when the duel started

	DUEL_CIRCLE	*duel_circle;	// the duel circle the duel is going on in
};
void	duel_init(CORE_DATA *cd, DUEL *d, uint16_t team1freq, uint16_t team2freq, bool use_elo, int team1elo, int team2elo, int rounds, int by, int fast);
void	duel_clear(DUEL *d);
int	duel_isfinished(DUEL *d, int team1_adjust, int team2_adjust);
void	duel_teamscored(CORE_DATA *cd, DUEL *d, int winner);
void	duel_nc(CORE_DATA *cd, DUEL *d);
int	duel_teamisinsafe(CORE_DATA *cd, DUEL *d, int team);
void	duel_computestats(CORE_DATA *cd, DUEL *duel, int winning_team, PLAYER *absent, uint16_t old_freq);
void	duel_settimer(CORE_DATA *cd, DUEL *d);
void	duel_warpteamstocentersafe(CORE_DATA *cd, DUEL *d);
void	duel_announcewin(CORE_DATA *cd, DUEL *d);
void	duel_getteamtext(CORE_DATA *cd, uint16_t freq, char *buf, int bufl);

#define DISSOLVE_QUIT 0			// set p
#define DISSOLVE_FREQCHANGE 1	// set p
#define DISSOLVE_SPEC 2			// set p
#define DISSOLVE_INTER 3		// set p and q
void	duel_dissolve(CORE_DATA *cd, DUEL *d, int dissolve_reason, PLAYER *p, PLAYER *q);


typedef struct challenge CHALLENGE;
struct challenge {
	int in_use;		// set when in use

	uint16_t freq[2];	// freqs, TEAM1 is the challenger, TEAM2 is the challenged
	uint16_t size[2];	// team sizes

	int avg_elo[2];	// avg elo
	bool use_elo;	// use elo?

	int num_rounds;	// number of rounds
	int win_by;		// win by value

	int timer;		// timer
	int is_fast;	// set when the duel is fast

	char dc_name[32];	// name of the requested duel circle
};

void ch_clear(CORE_DATA *cd, CHALLENGE *ch, bool killtimers);
