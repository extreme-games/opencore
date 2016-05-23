
#include <assert.h>

#include "userdata.hpp"
#include "duel.hpp"
#include "elo.hpp"
#include "db.hpp"

#include "dueltypes.hpp"



//
// Build a text string describing a team.
//
void
duel_getteamtext(CORE_DATA *cd, uint16_t freq, char *buf, int bufl)
{
	int num_names = 0;
	int use_comma = 0;

	if (bufl < 1) {
		return;
	}
	*buf = '\0';

	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		if (p->freq == freq) {
			if (num_names < 3) {
				if (use_comma) {
					strlcat(buf, ", ", bufl);
				}
				strlcat(buf, p->name, bufl);
				use_comma = 1;
			} else if (num_names == 3) {
				strlcat(buf, " and ...", bufl);
			} else if (num_names > 3) {
				// do nothing
			}

			++num_names;
		}
	}
}


void
duel_warpteamstocentersafe(CORE_DATA *cd, DUEL *d)
{
	uint32_t time = GetTicksMs();
	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		if (p->freq == d->freq[TEAM1] || p->freq == d->freq[TEAM2]) {
			PrivMessage(p, "*prize #7");
			pi(cd, p)->lastwarptick = time;
			pi(cd, p)->nfdw = false;
			pi(cd, p)->waskilled = false;
			pi(cd, p)->gotsafed = false;
			pi(cd, p)->stats->ckills = 0;
		}
	}
}


void
duel_settimer(CORE_DATA *cd, DUEL *d)
{
	if (d->timer) {
		KillTimer(d->timer);
		d->timer = 0;
	}

	// never set a timer for fast duels
	if (d->is_fast) {
		return;
	}

	if (d->size[TEAM1] == 1 && d->size[TEAM2] == 1) {
		d->timer = SetTimer(ud(cd)->SMALLTEAMKILLTIMER, 0, 0);
	} else {
		d->timer = SetTimer(ud(cd)->KILLTIMER, 0, 0);
	}
}

//
// end a duel, this function must only be called in the context of EVENT_CHANGE for
// some types of dissolves since it relies on cd->old_freq
//
void
duel_dissolve(CORE_DATA *cd, DUEL *d, int dissolve_reason, PLAYER *p, PLAYER *q)
{
	int winner;
	if (dissolve_reason == DISSOLVE_INTER) {
		winner = p->freq == d->freq[TEAM1] ? TEAM2 : TEAM1;	// the player didnt change freqs
	} else {
		winner = cd->old_freq == d->freq[TEAM1] ? TEAM2 : TEAM1;
	}

	duel_computestats(cd, d, winner, p, d->freq[winner]);

	char buf[256];
	switch (dissolve_reason) {
		case DISSOLVE_QUIT:
			snprintf(buf, 256, "%s left the arena.", p->name);
			break;
		case DISSOLVE_FREQCHANGE:
			snprintf(buf, 256, "%s changed freq.", p->name);
			// warp player to center
			PrivMessage(p, "*prize #7");
			break;
		case DISSOLVE_SPEC:
			snprintf(buf, 256, "%s specced.", p->name);
			break;
		case DISSOLVE_INTER:
			snprintf(buf, 256, "%s/%s interfered.", p->name, q->name);
			// also warp players in this situation
			PrivMessage(p, "*prize #7");
			PrivMessage(q, "*prize #7");
			break;
		default:
			snprintf(buf, 256, "No reason.");
			assert(0);
			break;
	}

	// announce duel end
	ArenaMessageFmt("(%s) defeated (%s) %d to %d (%db%d): %s",
		d->names[winner], d->names[!winner],
		d->score[winner], d->score[!winner],
		d->num_rounds, d->win_by,
		buf);

	duel_warpteamstocentersafe(cd, d);

	duel_clear(d);
}


void
duel_announcewin(CORE_DATA *cd, DUEL *d)
{
	int winners = d->score[TEAM1] >= d->score[TEAM2] ? TEAM1 : TEAM2;
	
	duel_computestats(cd, d, winners, NULL, FREQ_NONE);

	char buf[32];
	cycad::TimeParse_ttoas(buf, 32, GetTicksMs() - d->start_tick);

	ArenaMessageFmt("(%s) defeated (%s) %d to %d (%db%d) in %s",
		d->names[winners], d->names[!winners], 
		d->score[winners], d->score[!winners],
		d->num_rounds, d->win_by, buf);
}

// compute and record stats
void
duel_computestats(CORE_DATA *cd, DUEL *d, int winning_team, PLAYER *absent, uint16_t absent_old_freq)
{
	uint16_t mostkills = 0;
	uint16_t mostdeaths = 0;
	uint16_t bestspree = 0;
	uint32_t mostassists = 0;
	double highestrated = 0;

	uint16_t winner_freq = d->freq[winning_team];
	uint16_t loser_freq = d->freq[!winning_team];

	uint32_t mostdt = 0;
	uint32_t mostdg = 0;

	// record wins/losses to pinfo, deaths are recorded as pinfo
	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		if (p->freq == winner_freq) {
			if (d->use_elo) {
				pi(cd,p)->all_stats->rated->wins++;
			} else {
				pi(cd,p)->all_stats->unrated->wins++;
			}
		} else if (p->freq == loser_freq) {
			if (d->use_elo) {
				pi(cd,p)->all_stats->rated->losses++;
			} else {
				pi(cd,p)->all_stats->unrated->losses++;
			}
		}
	}
	if (absent) {
		PLAYER *p = absent;
		if (absent_old_freq == winner_freq) {
			if (d->use_elo) {
				pi(cd,p)->all_stats->rated->wins++;
			} else {
				pi(cd,p)->all_stats->unrated->wins++;
			}
		} else {
			if (d->use_elo) {
				pi(cd,p)->all_stats->rated->losses++;
			} else {
				pi(cd,p)->all_stats->unrated->losses++;
			}
		}
	}

	// do the elo stuff here
	// in this block temp_elo is used first for the new elo rating, then later for the elo diff
	if (d->use_elo && (d->score[TEAM1] > 0 || d->score[TEAM2] > 0 || (ud(cd)->NCINTERVAL && GetTicksMs() - d->start_tick > ud(cd)->NCINTERVAL))) {
		for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == d->freq[winning_team]) {
				pi(cd, p)->temp_elo = elo_compute_win(pi(cd, p)->elo, d->avg_elo[!winning_team]);
				pi(cd, p)->max_elo = MAX(pi(cd, p)->max_elo, pi(cd, p)->temp_elo);
			} else if (p->freq == d->freq[!winning_team]) {
				pi(cd, p)->temp_elo = elo_compute_loss(pi(cd, p)->elo, d->avg_elo[winning_team]);
			}
		}

		for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == d->freq[TEAM1] || p->freq == d->freq[TEAM2]) {
				// store diff in temp_elo and copy new elo value
				int elo_diff = abs(pi(cd, p)->elo - pi(cd, p)->temp_elo);
				pi(cd, p)->elo = pi(cd, p)->temp_elo;
				pi(cd, p)->temp_elo = elo_diff;
			}
		}

		// this block is basically an interation of the above loop for the absent player
		if (absent) {
			if (absent_old_freq == d->freq[winning_team]) {
				// player won
				pi(cd, absent)->temp_elo = elo_compute_win(pi(cd, absent)->elo, d->avg_elo[!winning_team]);
				pi(cd, absent)->max_elo = MAX(pi(cd, absent)->max_elo, pi(cd, absent)->temp_elo);

				int elo_diff = abs(pi(cd, absent)->elo - pi(cd, absent)->temp_elo);
				pi(cd, absent)->elo = pi(cd, absent)->temp_elo;
				pi(cd, absent)->temp_elo = elo_diff;
				
				db_storeplayersdata(cd, absent);
			} else {
				pi(cd, absent)->temp_elo = elo_compute_loss(pi(cd, absent)->elo, d->avg_elo[winning_team]);

				int elo_diff = abs(pi(cd, absent)->elo - pi(cd, absent)->temp_elo);
				pi(cd, absent)->elo = elo_compute_loss(pi(cd, absent)->elo, d->avg_elo[winning_team]);
				pi(cd, absent)->temp_elo = elo_diff;
			}
		}
	} else {
		// zero elo diffs
		for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == d->freq[winning_team] || p->freq == d->freq[!winning_team]) {
				pi(cd, p)->temp_elo = 0;
			}
		}

		if (absent) {
			pi(cd, absent)->temp_elo = 0;
		}
	}

	// if theres an absent player, include his stats
	if (absent) {
		if (pi(cd,absent)->stats->kills > mostkills) {
			mostkills = pi(cd,absent)->stats->kills;
		}

		if (pi(cd,absent)->stats->deaths > mostdeaths) {
			mostdeaths = pi(cd,absent)->stats->deaths;
		}

		if (pi(cd,absent)->stats->mostckills > bestspree) {
			bestspree = pi(cd,absent)->stats->mostckills;
		}

		if (pi(cd,absent)->stats->dg > mostdg) {
			mostdg = pi(cd,absent)->stats->dg;
		}

		if (pi(cd,absent)->stats->dt > mostdt) {
			mostdt = pi(cd,absent)->stats->dt;
		}

		if (pi(cd,absent)->stats->assists > mostassists) {
			mostassists = pi(cd,absent)->stats->assists;
		}

		if (RATIO(pi(cd,absent)->stats->kills, pi(cd,absent)->stats->deaths) > highestrated) {
			highestrated = RATIO(pi(cd,absent)->stats->kills, pi(cd,absent)->stats->deaths);
		}
	}

	// loop through both teams and check their stats
	for (PLAYER *p = cd->ship_first; p; p = p->ship_next) {
		if (p->freq == winner_freq || p->freq == loser_freq) {
			if (pi(cd,p)->stats->kills > mostkills) {
				mostkills = pi(cd,p)->stats->kills;
			}

			if (pi(cd,p)->stats->deaths > mostdeaths) {
				mostdeaths = pi(cd,p)->stats->deaths;
			}

			if (pi(cd,p)->stats->mostckills > bestspree) {
				bestspree = pi(cd,p)->stats->mostckills;
			}

			if (pi(cd,p)->stats->dg > mostdg) {
				mostdg = pi(cd,p)->stats->dg;
			}

			if (pi(cd,p)->stats->dt > mostdt) {
				mostdt = pi(cd,p)->stats->dt;
			}

			if (pi(cd,p)->stats->assists > mostassists) {
				mostassists = pi(cd,p)->stats->assists;
			}

			if (RATIO(pi(cd,p)->stats->kills, pi(cd,p)->stats->deaths) > highestrated) {
				highestrated = RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths);
			}
		}
	}

	char btytext[16];
	memset(btytext, 0, 16);
	snprintf(btytext, 15, "(%hu) ", bestspree);

	char dgtext[16];
	memset(dgtext, 0, 16);
	snprintf(dgtext, 15, "(%u) ", mostdg);

	char dttext[16];
	memset(dttext, 0, 16);
	snprintf(dttext, 15, "(%u) ", mostdt);
	
	char assiststext[16];
	memset(assiststext, 0, 16);
	snprintf(assiststext, 15, "(%u) ", mostassists);

	// iterate through and announce winner_freq
	
	char buf[256];
	bool sendarena = d->size[TEAM1] + d->size[TEAM2] >= ud(cd)->PLAYERSFORARENASPEW;
	for (PLAYER *p = cd->ship_first; p; p = p->ship_next) {
		snprintf(buf, 256, "[Winners] %-15s  (%d/+%2d) %2hu/%2hu/%3u (%.2lf) %s%s%s%s%s%s%s%s%s%s%s", p->name, pi(cd,p)->elo, pi(cd,p)->temp_elo,
					 pi(cd, p)->stats->kills,
					 pi(cd, p)->stats->deaths,
					 pi(cd, p)->stats->assists,
					 RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths),

					 (highestrated!=0 && RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths) >= highestrated) ? "HighestRated " : "",
					 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? "BestSpree" : "",
					 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? btytext : "",
					 (mostkills && pi(cd, p)->stats->kills >= mostkills) ? "MostKills " : "",
					 (mostdeaths && pi(cd, p)->stats->deaths >= mostdeaths) ? "MostDeaths " : "",
					 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? "MostDG" : "",
					 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? dgtext : "",
					 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? "MostDT" : "",
					 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? dttext : "",
					 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? "MostAssists" : "",
					 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? assiststext : "");

		if (p->freq == winner_freq) {
			if (sendarena) {
				ArenaMessageFmt("%s", buf);
			} else {
				cycad::PrivFreqsShipsFmt(cd, d->freq[TEAM1], d->freq[TEAM2], "%s", buf);
			}
		}
	}

	// add the absent player to the winner_freq list
	if (absent && absent_old_freq == winner_freq) {
		PLAYER *p = absent;
		snprintf(buf, 256, "[Winners] %-15s  (%d/+%2d) %2hu/%2hu/%3u (%.2lf) %s%s%s%s%s%s%s%s%s%s%s", p->name, pi(cd,p)->elo, pi(cd,p)->temp_elo,
			 pi(cd, p)->stats->kills,
			 pi(cd, p)->stats->deaths,
			 pi(cd, p)->stats->assists,
			 RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths),
			 (highestrated!=0 && RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths) >= highestrated) ? "HighestRated " : "",
			 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? "BestSpree" : "",
			 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? btytext : "",
			 (mostkills && pi(cd, p)->stats->kills >= mostkills) ? "MostKills " : "",
			 (mostdeaths && pi(cd, p)->stats->deaths >= mostdeaths) ? "MostDeaths " : "",
			 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? "MostDG" : "",
			 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? dgtext : "",
			 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? "MostDT" : "",
			 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? dttext : "",
			 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? "MostAssists" : "",
			 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? assiststext : "");

		if (sendarena) {
			ArenaMessageFmt("%s", buf);
		} else {
			cycad::PrivFreqsShipsFmt(cd, d->freq[TEAM1], d->freq[TEAM2], "%s", buf);
		
		}
	}

	// iterate through and announce loser_freq
	for (PLAYER *p = cd->ship_first; p; p = p->ship_next) {
		if (p->freq == loser_freq) {
			snprintf(buf, 256, "[Losers]  %-15s  (%d/-%2d) %2hu/%2hu/%3u (%.2lf) %s%s%s%s%s%s%s%s%s%s%s", p->name, pi(cd,p)->elo, pi(cd,p)->temp_elo,
					 pi(cd, p)->stats->kills,
					 pi(cd, p)->stats->deaths,
					 pi(cd, p)->stats->assists,
					 RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths),
					 (highestrated!=0 && RATIO(pi(cd, p)->stats->kills, pi(cd, p)->stats->deaths) >= highestrated) ? "HighestRated " : "",
					 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? "BestSpree" : "",
					 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? btytext : "",
					 (mostkills && pi(cd, p)->stats->kills >= mostkills) ? "MostKills " : "",
					 (mostdeaths && pi(cd, p)->stats->deaths >= mostdeaths) ? "MostDeaths " : "",
					 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? "MostDG" : "",
					 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? dgtext : "",
					 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? "MostDT" : "",
					 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? dttext : "",
					 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? "MostAssists" : "",
					 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? assiststext : "");

			if (sendarena) {
				ArenaMessageFmt("%s", buf);
			} else {
				cycad::PrivFreqsShipsFmt(cd, d->freq[TEAM1], d->freq[TEAM2], "%s", buf);
			}
		}
	}

	// add absent to the loser_freq list
	if (absent && absent_old_freq == loser_freq) {
		PLAYER *p = absent;
			snprintf(buf, 256, "[Losers]  %-15s  (%d/-%2d) %2hu/%2hu/%3u (%.2lf) %s%s%s%s%s%s%s%s%s%s%s", p->name, pi(cd,p)->elo, pi(cd,p)->temp_elo,
				 pi(cd, p)->stats->kills,
				 pi(cd, p)->stats->deaths,
				 pi(cd, p)->stats->assists,
				 RATIO((uint16_t)pi(cd, p)->stats->kills, (uint16_t)pi(cd, p)->stats->deaths),
				 (highestrated!=0 && RATIO((uint16_t)pi(cd, p)->stats->kills, (uint16_t)pi(cd, p)->stats->deaths) >= highestrated) ? "HighestRated " : "",
				 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? "BestSpree" : "",
				 (bestspree && pi(cd, p)->stats->mostckills >= bestspree) ? btytext : "",
				 (mostkills && pi(cd, p)->stats->kills >= mostkills) ? "MostKills " : "",
				 (mostdeaths && pi(cd, p)->stats->deaths >= mostdeaths) ? "MostDeaths " : "",
				 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? "MostDG" : "",
				 (mostdg && pi(cd, p)->stats->dg >= mostdg) ? dgtext : "",
				 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? "MostDT" : "",
				 (mostdt && pi(cd, p)->stats->dt >= mostdt) ? dttext : "",
				 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? "MostAssists" : "",
				 (mostassists && pi(cd, p)->stats->assists >= mostassists) ? assiststext : "");

		if (sendarena) {
			ArenaMessageFmt("%s", buf);
		} else {
			cycad::PrivFreqsShipsFmt(cd, d->freq[TEAM1], d->freq[TEAM2], "%s", buf);
		}
	}

	// store stats to db
	for (PLAYER *p = cd->ship_first; p; p = p->ship_next) {
		if (p->freq == winner_freq || p->freq == loser_freq) {
			if (pi(cd, p)->stats_from_db == true) {
				db_storeplayersdata(cd, p);
			}
		}
	}
	if (absent) {
		if (pi(cd, absent)->stats_from_db == true) {
			db_storeplayersdata(cd, absent);
		}
	}
}

int
duel_isfinished(DUEL *d, int team1_adjust, int team2_adjust)
{
	if (d->score[TEAM1] + team1_adjust >= d->num_rounds || d->score[TEAM2] + team2_adjust  >= d->num_rounds) {
		// teams have passed the round threshhold
		if (abs((d->score[TEAM1] + team1_adjust) - (d->score[TEAM2] + team2_adjust)) >= d->win_by) {
			// the difference between scores is >= the win by value
			return 1;
		}
	}

	return 0;
}

void
duel_init(CORE_DATA *cd, DUEL *d, uint16_t team1freq, uint16_t team2freq, bool use_elo, int team1elo, int team2elo, int rounds, int by, int fast)
{
	duel_clear(d);

	// store arguments passed
	d->freq[TEAM1] = team1freq;
	d->freq[TEAM2] = team2freq;
	d->num_rounds = rounds;
	d->win_by = by;
	d->is_fast = fast;

	d->use_elo = use_elo;
	d->avg_elo[TEAM1] = team1elo;
	d->avg_elo[TEAM2] = team2elo;

	d->start_tick = GetTicksMs();

	// count team sizes
	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		if (p->freq == team1freq) {
			d->size[TEAM1] += 1;
		}
		if (p->freq == team2freq) {
			d->size[TEAM2] += 1;
		}
	}

	// build team descriptions
	duel_getteamtext(cd, team1freq, d->names[TEAM1], 256);
	duel_getteamtext(cd, team2freq, d->names[TEAM2], 256);
}


void
duel_clear(DUEL *d)
{
	// minor hack so i can memset here...
	DUEL_CIRCLE *dc = d->duel_circle;
	memset(d, 0, sizeof(DUEL));
	d->duel_circle = dc;

	d->in_use = 0;
	d->timer = 0;
}

//
// a team in the duel scored
//
void
duel_teamscored(CORE_DATA *cd, DUEL *d, int winner)
{
	d->score[winner] += 1;

	// only announce scores if the duel continues.
	if (duel_isfinished(d, 0, 0) == 0) {
		// the duel continues. announce scores

		int game_point = duel_isfinished(d, 1, 0) || duel_isfinished(d, 0, 1);

		if (d->score[winner] > d->score[!winner]) {
			cycad::PrivFreqShipsFmt(cd, d->freq[winner],
				"Your team scored and leads %d to %d (%db%d) %s",
				d->score[winner], d->score[!winner], d->num_rounds, d->win_by,
				game_point ? "Game point!" : "");
			cycad::PrivFreqShipsFmt(cd, d->freq[!winner],
				"Your team lost and trails %d to %d (%db%d) %s",
				d->score[!winner], d->score[winner], d->num_rounds, d->win_by,
				game_point ? "Game point!" : "");
		} else if (d->score[winner] == d->score[!winner]) {
			cycad::PrivFreqShipsFmt(cd, d->freq[winner],
				"Your team scored and is tied %d to %d (%db%d) %s",
				 d->score[winner], d->score[winner], d->num_rounds, d->win_by,
				 game_point ? "Game point!" : "");
			cycad::PrivFreqShipsFmt(cd, d->freq[!winner],
				"Your team lost and is tied %d to %d (%db%d) %s",
				 d->score[winner], d->score[winner], d->num_rounds, d->win_by,
				 game_point ? "Game point!" : "");
		} else { // winner score < !winner score
			cycad::PrivFreqShipsFmt(cd, d->freq[winner],
				"Your team scored but trails %d to %d (%db%d) %s",
				d->score[winner], d->score[!winner], d->num_rounds, d->win_by,
				game_point ? "Game point!" : "");
			cycad::PrivFreqShipsFmt(cd, d->freq[!winner],
				"Your team lost but still leads %d to %d (%db%d) %s",
				d->score[!winner], d->score[winner], d->num_rounds, d->win_by,
				game_point ? "Game point!" : "");
		}
	}
}

//
// both teams blew it
//
void
duel_nc(CORE_DATA *cd, DUEL *d)
{
	int game_point = duel_isfinished(d, 1, 0) || duel_isfinished(d, 0, 1);

	cycad::PrivFreqShipsFmt(cd, d->freq[TEAM1],
		"Both teams died, score remains %d to %d (%db%d) %s",
		d->score[TEAM1], d->score[TEAM2], d->num_rounds, d->win_by,
		game_point ? "Game point!" : "");
	cycad::PrivFreqShipsFmt(cd, d->freq[TEAM2],
		"Both teams died, score remains %d to %d (%db%d) %s",
		d->score[TEAM2], d->score[TEAM1], d->num_rounds, d->win_by,
		game_point ? "Game point!" : "");
}

// XXX: xy_time?? should be since the last pos update was seen
int
duel_teamisinsafe(CORE_DATA *cd, DUEL *d, int team)
{
	uint32_t time = GetTicksMs();
	PLAYER *p;

	if (d->size[TEAM1] == 1 && d->size[TEAM2] == 1) {
		// 1v1 duels
		for (p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == d->freq[team] &&
				(!ud_iscenterarea(cd, p->pos->x, p->pos->y) || time - pi(cd, p)->lastcenterareaupdatetick > ud(cd)->SMALLTEAMGONETIME)) {
				return false;
			}
		}

	} else {
		// > 1v1 duels
		for (p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == d->freq[team] && 
				(!ud_iscenterarea(cd, p->pos->x, p->pos->y) || time - pi(cd, p)->lastcenterareaupdatetick > ud(cd)->GONETIME)) {
				// player is either not in center or has been away for GONETIME seconds
				return false;
			}
		}
	}

	return true;
}


void
ch_clear(CORE_DATA *cd, CHALLENGE *ch, bool killtimers)
{
	if (killtimers && ch->timer) {
		KillTimer(ch->timer);
	}

	memset(ch, 0, sizeof(CHALLENGE));
}


void
dc_warpteams(CORE_DATA *cd, DUEL_CIRCLE *dc)
{
	DUEL *d = dc->duel;
	d->warpcount += 1;

	uint32_t time = GetTicksMs();
	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		// warp teams appropriately
		if (p->freq == d->freq[TEAM1]) {
			PrivMessageFmt(p, "*warpto %d %d",
				dc->x[d->warpcount % 2], dc->y[d->warpcount % 2]);
		} else if (p->freq == d->freq[TEAM2]) {
			PrivMessageFmt(p, "*warpto %d %d",
				dc->x[(d->warpcount + 1) % 2], dc->y[(d->warpcount + 1) % 2]);
		}

		// and do this for all players in the duel
		if (p->freq == d->freq[TEAM1] || p->freq == d->freq[TEAM2]) {
			PrivMessage(p, "*shipreset");
			pi(cd, p)->lastwarptick = time;
			pi(cd, p)->nfdw = true;
			pi(cd, p)->waskilled = false;
			pi(cd, p)->gotsafed = false;
			// commented out so spree doesnt reset after each round
			// pi(cd, p)->stats->ckills = 0;
		}
	}
	
	//duel->settimer(cd->SetTimer(1500, NULL)); // anti-ghost cheat hack
}
