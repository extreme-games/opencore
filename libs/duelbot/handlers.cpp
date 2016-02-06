
#include <assert.h>

#include "util.hpp"
#include "globals.hpp"
#include "userdata.hpp"
#include "pinfo.hpp"
#include "duel.hpp"
#include "db.hpp"
#include "elo.hpp"

#include "handlers.hpp"


void
handleDeadSlot(CORE_DATA *cd)
{

}

void
handleEnter(CORE_DATA *cd)
{
	PLAYER *p = cd->p1;

	if (pi(cd,p)->pinfo_reused != true) {
		// first time player is seen, give an elo rating until db tells it
		pi(cd,p)->elo = 1200;
		pi(cd,p)->pub_elo = 1200;
		pi(cd,p)->max_elo = 1200;
		pi(cd,p)->max_pub_elo = 1200;

		db_selectplayersdata(cd, p->name);
	
		pi(cd,p)->pinfo_reused = true; // set this for the future
	}

	pi_clear(cd, p);

	if (ud(cd)->SENDWATCHDAMAGE && p->pid != cd->bot_pid) {
		PrivMessage(p, "*watchdamage");
	}

	// if player entered to a freq that has a duel going on, spec him up
	if (ud_getduelbyfreq(cd, p->freq)) {
		cycad::specup(cd, p);
		pi(cd, p)->allowed_to_change_freq = true;
	}
}


void
handleQueryResult(CORE_DATA *cd)
{
	char ***res = cd->query_resultset;
	char **row = NULL;
	uint32_t i = 0;

	if (!cd->query_success) {
		LogFmt(OP_SMOD, "Query Error: Query %d", cd->query_type);
		return;
	}

	if (cd->query_type == DUELQUERY_RETRIEVESTATS) {
		if (cd->query_nrows == 1) {
			row = res[0];

			if (cd->query_ncols == 14) {
				PLAYER *p = FindPlayerName(cd->query_name, 0);
				if (p) {
					// initialize pinfo from db
					pi(cd,p)->elo = atoi(row[0]);
					pi(cd,p)->pub_elo = atoi(row[1]);

					pi(cd,p)->max_elo = atoi(row[2]);
					pi(cd,p)->max_pub_elo = atoi(row[3]);

					pi(cd,p)->all_stats->rated->kills = atoi(row[4]);
					pi(cd,p)->all_stats->rated->deaths = atoi(row[5]);
					pi(cd,p)->all_stats->rated->wins = atoi(row[6]);
					pi(cd,p)->all_stats->rated->losses = atoi(row[7]);
					
					pi(cd,p)->all_stats->unrated->kills = atoi(row[8]);
					pi(cd,p)->all_stats->unrated->deaths = atoi(row[9]);
					pi(cd,p)->all_stats->unrated->wins = atoi(row[10]);
					pi(cd,p)->all_stats->unrated->losses = atoi(row[11]);

					pi(cd,p)->all_stats->pub->kills = atoi(row[12]);
					pi(cd,p)->all_stats->pub->deaths = atoi(row[13]);
					
					pi(cd,p)->stats_from_db = true;
				}
			} else {
				Log(OP_SMOD, "Stat retrieval query returned unexpected number of columns.");
			}
		} else {
			// player not in db, give him an elo rating
			PLAYER *p = FindPlayerName(cd->query_name, 0);

			if (p) {
				db_addnewplayer(cd, p);
				
				// initilize pinfo from nothing, all other fields are 0
				pi(cd,p)->elo = 1200;
				pi(cd,p)->pub_elo = 1200;

				pi(cd,p)->stats_from_db = true;
			}
		}
	} else if (cd->query_type == DUELQUERY_ELORESET) {
		if (cd->query_nrows == 1) {
			row = res[0];

			if (cd->query_ncols == 9) {
				char *requester = cd->query_name;
				char *target = row[0];

				// get stats from db
				int elo = atoi(row[1]);
				int pub_elo = atoi(row[2]);
				int max_elo = atoi(row[3]);
				int max_pub_elo = atoi(row[4]);
				uint32_t kills = atoi(row[5]);
				uint32_t deaths = atoi(row[6]);
				uint32_t wins = atoi(row[7]);
				uint32_t losses = atoi(row[8]);

				LogFmt(OP_MOD, "Elo for \"%s\" reset by \"%s\": Elo:%d PubElo:%d MaxElo:%d MaxPubElo:%d Kills:%lu Deaths:%lu Wins:%lu Losses:%lu",
					target, requester, elo, pub_elo, max_elo, max_pub_elo, kills, deaths, wins, losses);
				RmtMessageFmt(requester, "Old stats for \"%s\": Elo:%d PubElo:%d MaxElo:%d MaxPubElo:%d Kills:%lu Deaths:%lu Wins:%lu Losses:%lu", 
					target, elo, pub_elo, max_elo, max_pub_elo, kills, deaths, wins, losses);
				RmtMessageFmt(requester, "New stats for \"%s\": Elo:1200 PubElo:1200 MaxElo:1200 MaxPubElo:1200 Kills:0 Deaths:0 Wins:0 Losses:0", target);
			} else {
				Log(OP_MOD, "Elo reset query returned unexpected number of columns.");
			}
		} else {
			RmtMessageFmt(cd->query_name, "PLAYER not found in database.");
		}
	} else if (cd->query_type == DUELQUERY_ELOCHECK) {
		PLAYER *p = FindPlayerName(cd->query_name, MATCH_HERE);
		if (p) {
			if (cd->query_nrows == 1 && cd->query_ncols == 2) {
				char **row = cd->query_resultset[0];
				PrivMessageFmt(p, "%s is rated %d.", row[0], row[1]);
			} else {
				PrivMessageFmt(p, "PLAYER not found.");
			}
		}
	} else if (cd->query_type == DUELQUERY_TOP10) {
		PLAYER *p = FindPlayerName(cd->query_name, MATCH_HERE);

		if (p) {
			PrivMessageFmt(p, "+--------------------+------+");
			PrivMessageFmt(p, "| PLAYER             + Elo  +");
			PrivMessageFmt(p, "+--------------------+------+");

			for (i = 0; i < cd->query_nrows; ++i) {
				row = cd->query_resultset[i];
				PrivMessageFmt(p, "| %-18s | %4s |", row[0], row[1]);
			}
			
			PrivMessageFmt(p, "+--------------------+------+");
		}
	} else if (cd->query_type == DUELQUERY_RANK) {
		PLAYER *p = FindPlayerName(cd->query_name, MATCH_HERE);
		if (p) {
			if (cd->query_nrows == 1 && cd->query_ncols == 1) {
				char **row = cd->query_resultset[0];
				PrivMessageFmt(p, "You are ranked in place %d.", atoi(row[0]) + 1);
			} else {
				PrivMessageFmt(p, "PLAYER not found.");
			}
		}
	} else if (cd->query_type == DUELQUERY_STATSCHECK) {
		PLAYER *p = FindPlayerName(cd->query_name, MATCH_HERE);
		if (p) {

			if (cd->query_nrows == 1 && cd->query_ncols == 11) {
				row = cd->query_resultset[0];
				PrivMessageFmt(p, "+--------------------+--------+");
				PrivMessageFmt(p, "| %-18s + Stats  +", row[0]);
				PrivMessageFmt(p, "+--------------------+--------+");

				PrivMessageFmt(p, "| %-18s | %6s |", "ELO Rating", row[1]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Max ELO Rating", row[2]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Rated Wins", row[3]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Rated Losses", row[4]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Pub ELO Rating", row[5]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Max Pub ELO Rating", row[6]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Pub Kills", row[7]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Pub Deaths", row[8]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Total Kills", row[9]);
				PrivMessageFmt(p, "| %-18s | %6s |", "Total Deaths", row[10]);
			
				PrivMessageFmt(p, "+--------------------+--------+");
			} else {	
				PrivMessageFmt(p, "PLAYER not found.");
			}
		}
	} else if (cd->query_type == DUELQUERY_QUERYIGNORE) {
		// do nothing
	}
}


void
handleKill(CORE_DATA *cd)
{
	uint32_t now = GetTicksMs();

	pi(cd, cd->p1)->stats->kills++;
	pi(cd, cd->p2)->stats->deaths++;

	pi(cd, cd->p1)->stats->ckills++;
	if (pi(cd, cd->p1)->stats->ckills > pi(cd, cd->p1)->stats->mostckills) {
		pi(cd, cd->p1)->stats->mostckills = pi(cd, cd->p1)->stats->ckills;
	}

	pi(cd, cd->p2)->stats->ckills = 0;

	pi(cd, cd->p2)->nfdw = true; // in case of fast deaths, handle in handleUpdate

	DUEL *d = ud_getduelbypid(cd, cd->p1->pid);		// killers duel pointer

	// here we add stats for players not in a duel
	// if one player is in a duel and the other isnt, then
	// no stats are added and the kill is ignored
	if (d == NULL && ud_getduelbypid(cd, cd->p2->pid) == NULL) {
		// there is no duel for either player, player is in pub
		
		// stats and pub elo
		pi(cd, cd->p1)->all_stats->pub->kills++;
		pi(cd, cd->p2)->all_stats->pub->deaths++;

		int p1elo = pi(cd, cd->p1)->pub_elo;
		int p2elo = pi(cd, cd->p2)->pub_elo;
		pi(cd, cd->p1)->pub_elo = elo_compute_win(p1elo, p2elo);
		pi(cd, cd->p1)->max_pub_elo = MAX(pi(cd, cd->p1)->pub_elo, pi(cd, cd->p1)->max_pub_elo);
		pi(cd, cd->p2)->pub_elo = elo_compute_loss(p2elo, p1elo);

		db_updatepubstats(cd, cd->p1);
		db_updatepubstats(cd, cd->p2);
		return;
	} else if (d != ud_getduelbypid(cd, cd->p2->pid)) { // players arent in the same duel
		// you could dissolve both duels here but prizing works better and
		// also gets players out of duel circles who shouldnt be in them
		
		PrivMessage(cd->p2, "*prize #7");
		PrivMessage(cd->p1, "*prize #7");
		
		return;
	}

	// stats
	if (d->use_elo == true) {
		// rated
		pi(cd, cd->p1)->all_stats->rated->kills++;
		pi(cd, cd->p2)->all_stats->rated->deaths++;
	} else {
		pi(cd, cd->p1)->all_stats->unrated->kills++;
		pi(cd, cd->p2)->all_stats->unrated->deaths++;
	}
	for (uint32_t i = 0; i < MAX_ASSISTS; ++i) {
		uint16_t pid = pi(cd, cd->p2)->last_damage[i].pid;
		uint32_t ticks = pi(cd, cd->p2)->last_damage[i].ticks;
		uint32_t interval = ud(cd)->ASSISTINTERVAL;

		if (pid != PID_NONE && pid != cd->p1->pid && now - ticks <= interval) {
			PLAYER *assist = FindPlayerPid(pid, 0);
			if (assist) pi(cd,assist)->stats->assists += 1;
		}
	}

	// kill the timer, if present (gets reset later on)
	if (d->timer) {
		KillTimer(d->timer);
		d->timer = 0;
	}

	// always catch last kill in 1v1 duels
	// otherwise players would spec up while waiting in graveyard
	// and cause the duel to be ended by a spec message
	if (d->size[TEAM1] == 1 && d->size[TEAM2] == 1) {
		int add1 = (cd->p1->freq == d->freq[TEAM1]) ? 1 : 0;
		int add2 = (cd->p1->freq == d->freq[TEAM2]) ? 1 : 0;

		if (duel_isfinished(d, add1, add2)) {
			// this was the last kill

			if (add1) {
				duel_teamscored(cd, d, TEAM1);
			}
			if (add2) {
				duel_teamscored(cd, d, TEAM2);
			}

			duel_announcewin(cd, d);
			duel_warpteamstocentersafe(cd, d);
			duel_clear(d);

			// the duel is done, dont do anything else
			return;	
		} else if (d->is_fast) {
			// handle fast duels specially

			int winner = cd->p1->freq == d->freq[TEAM1] ? TEAM1 : TEAM2;

			duel_teamscored(cd, d, winner);
			dc_warpteams(cd, d->duel_circle);
			return;
		} else {
			// this is a 1v1 slow-warp duel, handle it like any other slow duel
		}
	}

	// if someone has been safed dont let them get away with killing
	if (pi(cd,cd->p1)->gotsafed) {
		PrivMessage(cd->p1, "*prize #7");
	}

	pi(cd, cd->p2)->waskilled = true;

	if (d->is_fast) {
		int winner = (cd->p1->freq == d->freq[TEAM1]) ? TEAM1 : TEAM2;
		duel_teamscored(cd, d, winner);
	} 

	duel_settimer(cd, d);
}


void
handleUpdate(CORE_DATA *cd)
{
	uint32_t time = GetTicksMs();

	if (cd->p1->status & STATUS_SAFE) {
		if (ud_iscenterarea(cd, cd->p1->pos->x ,cd->p1->pos->y)) {
			pi(cd, cd->p1)->lastcenterareaupdatetick = time;
		}
	}
	
	// if player has anti on, remove it
	if (ud(cd)->REMOVEANTI && !(cd->p1->status & STATUS_SAFE) && (cd->p1->status & STATUS_ANTIWARP) && ud_isnoantiboundary(cd, cd->p1->pos->x, cd->p1->pos->y)) {
		if (time - pi(cd, cd->p1)->lastantitakentick > 1000) {
			PrivMessage(cd->p1, "*prize #-20");
			PrivMessage(cd->p1, "*warn Antiwarp is not allowed in center!");
			pi(cd, cd->p1)->lastantitakentick = time;
		}
	}
	
	// only give further handling to safe-zone packets
	if ((cd->p1->status & STATUS_SAFE) == 0) {
		return;
	}

	// if player is not in center AREA (not center safe), assume he is in a duel circle
	if (ud_iscenterarea(cd, cd->p1->pos->x, cd->p1->pos->y) == false) {
		pi(cd, cd->p1)->nfdw = true; // player will need to be warped if he returns to center

		if (ud(cd)->SWAPPEDSIDESENDSDUEL) {
			// check if a team is being 'safed'
			// this whole block could be a lot cleaner, but it works for now...

			DUEL *d = ud_getduelbypid(cd, cd->p1->pid); // get his duel
			if (d) {
				// player is in a duel
				DUEL_CIRCLE *dcptr = d->duel_circle;

				int res = d->warpcount % 2;

				// swap the warp coordinates (alternate warping)
				int t1x, t1y, t2x, t2y;
				if (res == 0) {
					t1x = dcptr->x[TEAM1];
					t1y = dcptr->y[TEAM1];
					t2x = dcptr->x[TEAM2];
					t2y = dcptr->y[TEAM2];
				} else {
					t1x = dcptr->x[TEAM2];
					t1y = dcptr->y[TEAM2];
					t2x = dcptr->x[TEAM1];
					t2y = dcptr->y[TEAM1];
				}

				uint16_t px, py;
				px = cd->p1->pos->x;
				py = cd->p1->pos->y;

				px = px/16;
				py = py/16;

				uint16_t spr = ud(cd)->SPAWNPOINTRADIUS;

				// check for safing
				if (cd->p1->freq == d->freq[TEAM1]) {
					if (px >= t2x-spr && py >= t2y-spr && px <= t2x+spr && py <= t2y+spr) { // team reached enemies spawn point
						for (PLAYER *x = cd->ship_first; x != NULL; x = x->ship_next) {
							if (x->freq == d->freq[TEAM2] && pi(cd, x)->gotsafed == false) {
								PrivMessageFmt(x, "Your team lost: %s reached your team's spawn point.", cd->p1->name);
								PrivMessage(x, "*prize #7");
								pi(cd, x)->gotsafed = true;
							}
						}
					}
				} else {
					if (px >= t1x-spr && py >= t1y-spr && px <= t1x+spr && py <= t1y+spr) { // team reached enemies spawn point
						for (PLAYER *x = cd->ship_first; x != NULL; x = x->ship_next) {
							if (x->freq == d->freq[TEAM1] && pi(cd, x)->gotsafed == false) {
								PrivMessageFmt(x, "Your team lost: %s reached your team's spawn point.", cd->p1->name);
								PrivMessage(x, "*prize #7");
								pi(cd, x)->gotsafed = true;
							}
						}
					}
				}

				duel_settimer(cd, d);
			} else {
				// player is not in a duel, do nothing
			}
		}

		return; // players in circles dont need further handling
	}

	DUEL *d = ud_getduelbypid(cd, cd->p1->pid); // get his duel
	if (d) {
		// player in in a duel

		if (d->is_fast) { // fast duels
			if (((ud(cd)->SWAPCENTERANDGRAVEYARD && ud_isgraveyard(cd, cd->p1->pos->x, cd->p1->pos->y)) || ud_iscentersafe(cd, cd->p1->pos->x, cd->p1->pos->y)) && pi(cd, cd->p1)->nfdw) {
				if (pi(cd, cd->p1)->waskilled == false) {
					// player was NOT killed, warped to center on their own

					int winner = cd->p1->freq == d->freq[TEAM1] ? TEAM2 : TEAM1;

					duel_teamscored(cd, d, winner);

					if (duel_isfinished(d, 0, 0)) {
						// the duel is over
						duel_announcewin(cd, d);
						duel_warpteamstocentersafe(cd, d);
						duel_clear(d);
					} else {
						dc_warpteams(cd, d->duel_circle);
					}
				} else {
					// player died, kill pts have already been added (in EVENT_KILL)
					dc_warpteams(cd, d->duel_circle);
				}
			}
		} else {
			// slow warp duel handling block, the player just repsawned

			if (ud(cd)->SWAPCENTERANDGRAVEYARD && ud_isgraveyard(cd, cd->p1->pos->x, cd->p1->pos->y)) {
				// they are in the graveyard and thats where players spawn, do nothing
			} else if (ud_iscentersafe(cd, cd->p1->pos->x, cd->p1->pos->y) && time - pi(cd, cd->p1)->lastwarptick > 1000) {
				// they are in the center safe, so warp them out

				// set the 'last warp' time
				pi(cd, cd->p1)->lastwarptick = time;

				ud_warptograveyard(cd, cd->p1);

				duel_settimer(cd, d);
			}
		}
	} else {
		// player is not in a duel
		// warp the player out of the graveyard if thats where they are
		if (ud(cd)->SWAPCENTERANDGRAVEYARD &&
			ud_isgraveyard(cd, cd->p1->pos->x, cd->p1->pos->y)
			&& time - pi(cd, cd->p1)->lastcenterwarptotick > 1000) {
				ud_warptocenterwithwarpto(cd, cd->p1);
				pi(cd, cd->p1)->lastcenterwarptotick = time;
		}
	}
}


void
handleChange(CORE_DATA *cd)
{
	if (cd->p1->ship == cd->old_ship && cd->p1->ship == SHIP_SPECTATOR) {
		// players can move around freely in spec
		return;
	}

	if (cd->old_freq != cd->p1->freq && cd->old_ship != SHIP_SPECTATOR) { 
		// a non-spec player changed freqs, dissolve any pending challenge
		CHALLENGE *ch = ud_getchallengebyfreq(cd, cd->old_freq);

		if (ch) {
			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1], "Challenge rejected, %s changed freqs.", cd->p1->name);
			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "Challenge rejected, %s changed freqs.", cd->p1->name);

			ch_clear(cd, ch, true);
		}
	}

	// allow the player to switch freq (hes being setshipped away from an ongoing duel)
	if (pi(cd, cd->p1)->allowed_to_change_freq) {
		pi(cd, cd->p1)->allowed_to_change_freq = false;
		return;
	}

	DUEL *d = NULL;

	if (cd->old_freq != cd->p1->freq && cd->p1->ship != SHIP_SPECTATOR) {
		// freq change from a non-spec ship
		
		d = ud_getduelbyfreq(cd, cd->old_freq);
		if (d && cd->old_ship != SHIP_SPECTATOR) {
			// player left an ongoing duel
			duel_dissolve(cd, d, DISSOLVE_FREQCHANGE, cd->p1, NULL);
		}

		d = ud_getduelbyfreq(cd, cd->p1->freq);
		if (d) {
			// the targets new freq is engaged in a duel, kick him out
			cycad::specup(cd, cd->p1);
			PrivMessageFmt(cd->p1, "Freq %hu is enganged in a duel and is locked to non-spectators.", cd->p1->freq);

			// allow him to be specced later on
			pi(cd, cd->p1)->allowed_to_change_freq = true;
		}
	} else if (cd->old_ship != cd->p1->ship) {
		// ship change
		if (cd->old_freq != cd->p1->freq) {
			// player went to spec
			d = ud_getduelbyfreq(cd, cd->old_freq);
		} else {
			d = ud_getduelbyfreq(cd, cd->p1->freq);
		}

		if (d) {
			if (cd->old_ship != SHIP_SPECTATOR && cd->p1->ship == SHIP_SPECTATOR) {
				// player went to spec
				duel_dissolve(cd, d, DISSOLVE_SPEC, cd->p1, NULL);
			} else {
				// normal shipchange, update duel timer

				duel_settimer(cd, d);
			}
		}
	}
}


void
handleLeave(CORE_DATA *cd)
{
	CHALLENGE *ch = ud_getchallengebyfreq(cd, cd->old_freq);
	// see if player had any pending challenges
	if (ch) { // he had a pending CHALLENGE
		// alert freqs the CHALLENGE is being cleared
		cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1], "Challenge rejected, %s left the arena.", cd->p1->name);
		cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "Challenge rejected, %s left the arena.", cd->p1->name);

		ch_clear(cd, ch, true);
	}

	// dissolve any old duels the player was invovled in
	if (cd->old_ship != SHIP_SPECTATOR) {
		// player was in a ship
		for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
			DUEL *d = ud(cd)->duels[i].duel;
			if (d->in_use &&
				(cd->old_freq == d->freq[TEAM1] ||
				cd->old_freq == d->freq[TEAM2])) {
				// player was in a duel
				duel_dissolve(cd, d, DISSOLVE_QUIT, cd->p1, NULL);
			}
		}
	}
}


void
handleTimer(CORE_DATA *cd)
{
	if (cd->timer_data1) {
		// cd->timer_data! is only set in !ch auto-reject timers
		
		PLAYER *p = FindPlayerPid((uint16_t)cd->timer_data1, 0);
		if (p == NULL || p->ship == SHIP_SPECTATOR) {
			// the player has left the arena
			// clearing the CHALLENGE takes place in handleLeave() if the player left
			// clearing the timer takes place in handleChange()
			// if the player sc'ed (you cant be in spec to set a CHALLENGE)
			return;
		}
			
		CHALLENGE *ch = ud_getchallengebyfreq(cd, p->freq);
		if (ch) {
			// clear the challenge and alert the players
			cycad::PrivFreqShips(cd, ch->freq[TEAM1], "Challenge auto-rejected after 30 seconds.");
			cycad::PrivFreqShips(cd, ch->freq[TEAM2], "Challenge auto-rejected after 30 seconds.");

			ch_clear(cd, ch, false);
		}
	} else {
		// this is a timer associated with a duel
		
		// find the duel matching the timer
		DUEL *d = NULL;
		for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
			if (ud(cd)->duels[i].duel->in_use && cd->timer_id == ud(cd)->duels[i].duel->timer) {
				d = ud(cd)->duels[i].duel;
				break;
			}
		}

		if (d == NULL) {
			// ignore the timer, it has no matching in-use duel (timer value set withing killing the old one)
			return;
		}

		DUEL_CIRCLE *dcptr = d->duel_circle;

		// are teams in safe?
		int team1done = duel_teamisinsafe(cd, d, TEAM1);
		int team2done = duel_teamisinsafe(cd, d, TEAM2);

		// reset assists tracking
		if (team1done || team2done) {
			for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
				if (p->freq == d->freq[TEAM1] || p->freq == d->freq[TEAM2]) {
					for (uint32_t i = 0; i < MAX_ASSISTS; ++i) pi(cd, p)->last_damage[i].pid = PID_NONE;
				}
			}
		}
		
		if (team1done && team2done) {
			// both teams died
			duel_nc(cd, d);
			dc_warpteams(cd, dcptr);
		} else if (team1done || team2done) {
			// one and only one team died
			int winner = team1done ? TEAM2 : TEAM1;

			duel_teamscored(cd, d, winner);
			if (duel_isfinished(d, 0, 0)) {
				duel_announcewin(cd, d);
				duel_warpteamstocentersafe(cd, d);
				duel_clear(d);

				// the duel is finished, dont warp teams or set a timer
				return;
			}

			dc_warpteams(cd, d->duel_circle);
		}

		// always keep a timer for the duel going
		// dont call duel_settimer here because the timer no longer exists
		// and we dont want to kill the old one (pb would crash), plus the
		// extra timeout time is added here too
		d->timer = SetTimer(ud(cd)->KILLTIMER + ud(cd)->ENTERDELAY, 0, 0);
	}
}

void
handleIniChange(CORE_DATA *cd)
{
	ud_reloadconfig(cd);
}


void
handleAttach(CORE_DATA *cd)
{
	// anti-ghosting?
	if (pi(cd,cd->p1)->gotsafed) {
		ud_warptograveyard(cd, cd->p2);
		ud_warptograveyard(cd, cd->p1);
	}
}


void
handleLogin(CORE_DATA *cd)
{
	SetPosition(ud(cd)->BOTX * 16, ud(cd)->BOTY * 16, 0, 0);
}

void
handleStop(CORE_DATA *cd)
{
	if (cd->user_data) {
		ud_delete(cd);
	}
}

void
handleStart(CORE_DATA *cd)
{
	srand(GetTicksMs());

	RegisterPlugin(OPENCORE_VERSION, "duelbot", "cycad", "2.0", __DATE__, __TIME__, "duelbot", sizeof(UserData), sizeof(PlayerInfo));

	ud_init(cd);
	ud_loadconfig(cd);
	
	db_initdb(cd);

	// player commands
	RegisterCommand(CMD_CHALLENGE, "!challenge", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "<# deaths>[b#][:circle] <name>", "Challenge <name> to duel", NULL);
	RegisterCommand(CMD_UCHALLENGE, "!uchallenge", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "<# deaths>[b#][:circle] <name>", "Challenge <name> to an unrated duel", NULL);
	RegisterCommand(CMD_SLOWCHALLENGE, "!slowchallenge", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "<# deaths> <name>", "Challenge <name> to a slow duel", NULL);
	RegisterCommand(CMD_ACCEPT, "!accept", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Accept a CHALLENGE", NULL);
	RegisterCommand(CMD_REJECT, "!reject", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Reject a challenge", NULL);
	RegisterCommand(CMD_SCORERESET, "!scorereset", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Reset your score", NULL);
	RegisterCommand(CMD_NODUEL, "!noduel", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Toggle your ability to be !ch'ed", NULL);
	RegisterCommand(CMD_DUELHELP, "!duelhelp", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show duelbot usage information", NULL);
	RegisterCommand(CMD_DUELINFO, "!duelinfo", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show current duels", NULL);
	RegisterCommand(CMD_RATING, "!rating", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "<player>", "Show players rating", NULL);
	RegisterCommand(CMD_TOP10, "!top10", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show top 10 rated players", NULL);
	RegisterCommand(CMD_RANK, "!rank", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show your rank", NULL);
	RegisterCommand(CMD_PUBRATING, "!pubrating", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "<player>", "Show players rating in pub", NULL);
	RegisterCommand(CMD_PUBTOP10, "!pubtop10", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show top 10 rated pub players", NULL);
	RegisterCommand(CMD_PUBRANK, "!pubrank", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, NULL, "Show your rank in pub", NULL);
	RegisterCommand(CMD_STATS, "!stats", "duelbot", 0, CMD_PUBLIC | CMD_PRIVATE, "[player]", "Show your stats", NULL);

	// smod commands
	RegisterCommand(CMD_LOCKDUEL, "!lockduel", "duelbot", 5, CMD_PRIVATE, NULL, "Lock the duelbot", NULL);
	RegisterCommand(CMD_ELORESET, "!eloreset", "duelbot", 7, CMD_PRIVATE, NULL, "Reset a player's Elo; must use full player name", NULL);
}

#if 0
void
handleDamage(CORE_DATA *cd)
{
	// store the damage given/taken and assist cd
	uint32_t now = GetTicksMs();
	if (cd->p1->freq != cd->p2->freq) {
		// store damage taken and given
		pi(cd,cd->p1)->stats->dt += cd->dmg_taken;
		pi(cd,cd->p2)->stats->dg += cd->dmg_taken;

		// store the assist cd
		bool found = false;
		uint32_t oldest_idx = 0;
		uint32_t oldest_ticks = 0;
		for (uint32_t i = 0; i < MAX_ASSISTS; ++i) {
			// update an available or prexisting slot if available
			if (pi(cd,cd->p1)->last_damage[i].pid == cd->p2->pid || pi(cd,cd->p1)->last_damage[i].pid == PID_NONE) {
				pi(cd,cd->p1)->last_damage[i].ticks = now;
				pi(cd,cd->p1)->last_damage[i].pid = cd->p2->pid;
				found = true;
				break;
			}

			// keep track of the oldest tick index while iterating, used for replacement if
			// no slots are available
			if (now - pi(cd,cd->p1)->last_damage[i].ticks > oldest_ticks) {
				oldest_ticks = now - pi(cd,cd->p1)->last_damage[i].ticks;
				oldest_idx = i;
			}
		}

		// no available or preexisting entry found, replace the oldest entry
		if (!found) {
			pi(cd,cd->p1)->last_damage[oldest_idx].ticks = now;
			pi(cd,cd->p1)->last_damage[oldest_idx].pid = cd->p2->pid;
		}
	}
}
#endif

