
#include "dueltypes.hpp"
#include "userdata.hpp"
#include "duel.hpp"
#include "db.hpp"
#include "elo.hpp"

#include "commands.hpp"



void
cmd_Stats(CORE_DATA *cd)
{
	PLAYER *r = NULL;

	char *name = cd->cmd_argc == 1 ? cd->p1->name : cd->cmd_argr[1];
	if (name[0] == ':') {
		++name;
		r = FindPlayerName(name, 0);
	} else {
		r = FindPlayerName(name, MATCH_HERE);

		if (r == NULL) {
			r = FindPlayerName(name, MATCH_PREFIX | MATCH_HERE);
		}

		if (r == NULL) {
			r = FindPlayerName(name, MATCH_PREFIX);
		}
	}

	if (r) {
		ReplyFmt("+--------------------+--------+");
		ReplyFmt("| %-18s + Stats  +", r->name);
		ReplyFmt("+--------------------+--------+");

		ReplyFmt("| %-18s | %6d |", "ELO Rating", pi(cd,r)->elo);
		ReplyFmt("| %-18s | %6d |", "Max ELO Rating", pi(cd,r)->max_elo);
		ReplyFmt("| %-18s | %6d |", "Rated Wins",
			pi(cd,r)->all_stats->rated->wins);
		ReplyFmt("| %-18s | %6d |", "Rated Losses",
			pi(cd,r)->all_stats->rated->losses);
		ReplyFmt("| %-18s | %6d |", "Pub ELO Rating", pi(cd,r)->pub_elo);
		ReplyFmt("| %-18s | %6d |", "Max Pub ELO Rating", pi(cd,r)->max_pub_elo);
		ReplyFmt("| %-18s | %6d |", "Pub Kills",
			pi(cd,r)->all_stats->pub->kills);
		ReplyFmt("| %-18s | %6d |", "Pub Deaths",
			pi(cd,r)->all_stats->pub->deaths);

		ReplyFmt("| %-18s | %6d |", "Total Kills",
			pi(cd,r)->all_stats->pub->kills +
			pi(cd,r)->all_stats->rated->kills +
			pi(cd,r)->all_stats->unrated->kills);
		ReplyFmt("| %-18s | %6d |", "Total Deaths",
			pi(cd,r)->all_stats->pub->deaths +
			pi(cd,r)->all_stats->rated->deaths +
			pi(cd,r)->all_stats->unrated->deaths);
		
		ReplyFmt("+--------------------+--------+");
	} else {
		if (ud(cd)->USEDB) {
			db_statscheck(cd, cd->p1->name, name);
		} else {
			ReplyFmt("Error: \"%s\" not found.", name);
		}
	}
}

void
cmd_DuelInfo(CORE_DATA *cd)
{
	uint32_t time = GetTicksMs();
	bool displayed = false;
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		DUEL_CIRCLE *dc = &ud(cd)->duels[i];
		DUEL *d = dc->duel;
		if (d->in_use) {
			char timebuf[32];
			cycad::TimeParse_ttoas(timebuf, 32, time - d->start_tick);

			ReplyFmt("Box %s: %s (%d points) vs. %s (%d points): %db%d Time: %s",
				dc->name, d->names[TEAM1], d->score[TEAM1], d->names[TEAM2], d->score[TEAM2],
				d->num_rounds, d->win_by, timebuf);

			displayed = true;
		}
	}

	if (!displayed) {
		Reply("No duels.");
	}
}

void
cmd_Top10(CORE_DATA *cd, bool pub)
{
	if (ud(cd)->USEDB) {
		db_top10(cd, cd->p1->name, pub);
	} else {
		Reply("This bot is not currently using DB stats.");
	}
}


void
cmd_Rank(CORE_DATA *cd, bool pub)
{
	if (ud(cd)->USEDB) {
		db_rank(cd, cd->p1->name, pub, pub ? pi(cd,cd->p1)->pub_elo : pi(cd,cd->p1)->elo);
	} else {
		Reply("This bot is not currently using DB stats.");
	}
}


void
cmd_Rating(CORE_DATA *cd, bool pub)
{
	PLAYER *r = NULL;

	char *name = cd->cmd_argc == 1 ? cd->p1->name : cd->cmd_argr[1];
	if (name[0] == ':') {
		++name;
		r = FindPlayerName(name, 0);
	} else {
		r = FindPlayerName(name, MATCH_HERE);

		if (r == NULL) {
			r = FindPlayerName(name, MATCH_PREFIX | MATCH_HERE);
		}

		if (r == NULL) {
			r = FindPlayerName(name, MATCH_PREFIX);
		}
	}

	if (r) {
		ReplyFmt("%s is rated %d.", r->name, pub ? pi(cd,r)->pub_elo : pi(cd, r)->elo);
	} else {
		if (ud(cd)->USEDB) {
			db_elocheck(cd, cd->p1->name, name, pub);
		} else {
			ReplyFmt("Error: \"%s\" not found.", name);
		}
	}
}


void
cmd_DuelHelp(CORE_DATA *cd)
{
	Reply("To Duel:  !ch <# deaths>[b<# win by>][:CircleName] <player>");
	Reply("Example:  !ch 5:7 cycad       (Challenge cycad to a 5 round duel in circle 7)");
	Reply("Example:  !ch 5b2 cycad       (Challenge cycad to a 5 round duel, winner must be ahead by 2 deaths)");
	Reply("If a duel is 1v1 it can be SLOW-WARP (warping is NOT immediately on a kill) by using !slowchallenge/!sch");
	Reply("--");
	Reply("Team Duel: Challenges work by freq. Being on a freq of 2 and challenging someone on a freq of 2 will start a 2v2 duel.");
	Reply("--");
	Reply("Other Commands:  !noduel       (Stop people from challenging you)");
	Reply("Other Commands:  !scorereset   (Reset your score in the arena)");
	Reply("Other Commands:  !duelinfo     (Show current duels)");
}


void
cmd_NoDuel(CORE_DATA *cd)
{
	if (pi(cd, cd->p1)->noduel_enabled) {
		pi(cd, cd->p1)->noduel_enabled = 0;
		Reply("Dueling state set to ON. Players can !CHALLENGE you. Use !NODUEL again to turn it back off.");
	} else {
		pi(cd, cd->p1)->noduel_enabled = 1;
		Reply("Dueling state set to OFF. Players cannot !CHALLENGE you or your freq. Use !NODUEL again to turn it back on.");
	}
}


void
cmd_ScoreReset(CORE_DATA *cd)
{
	if (cd->msg_type == MSG_PUBLIC) {
		Reply("This command only works in private message.");
	} else {
		Reply("*scorereset");
		Reply("Score reset.");
	}
}


void
cmd_EloReset(CORE_DATA *cd)
{
	if (cd->cmd_argc <= 1 || strlen(cd->cmd_argr[1]) > 20) {
		Reply("Usage: !eloreset <player's full name>");
		return;
	}

	char *requester = cd->cmd_name;
	char *target = cd->cmd_argr[1];

	// reset player's cd in cache if found
	PLAYER *p = FindPlayerName(target, 0);
	if (p) {
		if (p->here && ud_getduelbyfreq(cd, p->freq)) {
			Reply("Target player must not be in a duel.");
			return;
		}
		pi(cd, p)->elo = 1200;
		pi(cd, p)->max_elo = 1200;
		pi(cd, p)->pub_elo = 1200;
		pi(cd, p)->max_pub_elo = 1200;
		pi(cd, p)->all_stats->rated->kills = 0;
		pi(cd, p)->all_stats->rated->deaths = 0;
		pi(cd, p)->all_stats->rated->wins = 0;
		pi(cd, p)->all_stats->rated->losses = 0;
	}

	// reset stats in database
	db_eloreset(cd, requester, target);
}


void
cmd_Challenge(CORE_DATA *cd, bool slow, bool rated)
{
	// basic error checking
	char *err = NULL;
	if (ud(cd)->LOCKED) {
		err = "Dueling is temporarily disabled.";
	} else if (pi(cd, cd->p1)->noduel_enabled) {
		err = "Your dueling state is OFF. Toggle it on with !noduel to be able to use this command.";
	} else if (GetTicksMs() - pi(cd, cd->p1)->lastchallengetick < 15000) {
		err = "You can only !ch once every 15 seconds.";
	} else if (cd->p1->ship == SHIP_SPECTATOR) {
		err = "Spectators can't use this command.";
	}

	if (err) {
		Reply(err);
		return;
	}
	
	// proper usage check
	if (cd->cmd_argc < 3) {
		ReplyFmt("Usage: %s <# deaths> <name>", cd->cmd_argv[0]);
		return;
	}

	// check for a preexisting challenge
	CHALLENGE *ch = ud_getchallengebyfreq(cd, cd->p1->freq);
	if (ch) {
		Reply("You already have a pending challenge, use !a or !r to accept or reject it first.");
		return;
	}

	// check if the player is in a duel
	DUEL *sdptr = ud_getduelbyfreq(cd, cd->p1->freq);
	if (sdptr) {
		Reply("You area already engaged in a duel.");
		return;
	}

	// check an open challenge
	ch = ud_getopenchallenge(cd);
	if (ch == NULL) {
		Reply("No open challenge slots available, try again shortly.");
		return;
	}

	// read the duel length
	ch->num_rounds = cycad::atoiarg(cd->cmd_argv[1], 0, 'b');
	if (ch->num_rounds <= 0) {
		ch->num_rounds = 1;
	}
	if (ud(cd)->MAXROUNDS && ch->num_rounds > ud(cd)->MAXROUNDS) {
		ch->num_rounds = ud(cd)->MAXROUNDS;
		ReplyFmt("Limiting the number of rounds to %d.", ch->num_rounds);
	}

	// read the by value
	ch->win_by = cycad::atoiarg(cd->cmd_argv[1], 1, 'b');
	if (ch->win_by <= 0) {
		ch->win_by = 1;
	}
	if(ud(cd)->MAXBYVAL && ch->win_by > ud(cd)->MAXBYVAL) {
		ch->win_by = ud(cd)->MAXBYVAL;
		ReplyFmt("Lowering by value to %d.", ch->win_by);
	}

	// read the requested duel circle name if present
	char duel_circle[24];
	cycad::delimargs(duel_circle, cd->cmd_argv[1], 1, ':', 24, false);
	if (strlen(duel_circle) > 0) {
		DUEL *d = ud_getopenduelbyname(cd, duel_circle);
		if (d == NULL) {
			ReplyFmt("No available duel circle named \"%s\" found.", ch->dc_name);
			return;
		} 

		// copy out the duel circles full name
		cycad::strncpyt(ch->dc_name, d->duel_circle->name, 32);
	}
	
	PLAYER *p = cycad::matchplayer(cd, cd->cmd_argr[2]);

	// error checking on target
	err = NULL;
	if (p == NULL) {
		err = "Opponent not found.";
	} else if(p->pid == cd->bot_pid) {
		err = "Laff. Denied.";
	} else if(p->ship == SHIP_SPECTATOR) {
		err = "You can't challenge players in spec.";
	} else if(cd->p1->pid == p->pid) {
		err = "You can't challenge yourself. Struggler.";
	} else if(cd->p1->freq == p->freq) {
		err = "You can't challenge people on your freq.";
	} else if (ud_getchallengebyfreq(cd, p->freq) != NULL) {
		err = "Opponent already has a pending challenge.";
	} else if (ud_getduelbyfreq(cd, p->freq)) {
		err = "Opponent is already engaged in a duel.";
	}

	if (err) {
		Reply(err);
		return;
	}

	// store freq numbers and sizes
	ch->freq[TEAM1] = cd->p1->freq;
	ch->freq[TEAM2] = p->freq;
	ch->size[TEAM1] = cycad::playersinshiponfreq(cd, ch->freq[TEAM1]);
	ch->size[TEAM2] = cycad::playersinshiponfreq(cd, ch->freq[TEAM2]);

	
	// check that db stats have been retrieved, if appropriate
	if (ud(cd)->USEDB) {
		for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == ch->freq[TEAM1] || p->freq == ch->freq[TEAM2]) {
				if (pi(cd,p)->stats_from_db == false) {
					ReplyFmt("DB stats for \"%s\" are still pending. Retry in a few seconds.", p->name);
					return;
				}
			}
		}
	}

	// check for appropriate team sizes
	if (ud(cd)->MAXPERTEAM && (ch->size[TEAM1] > ud(cd)->MAXPERTEAM || ch->size[TEAM2] > ud(cd)->MAXPERTEAM)) {
		ReplyFmt("Max team size is %d.", ud(cd)->MAXPERTEAM);
		return;
	}

	// allow fast duels for 1v1 duels only
	if (slow == false && ch->size[TEAM1] == 1 && ch->size[TEAM2] == 1) {
		if (ud(cd)->DEFAULT1V1FAST == true) {
			ch->is_fast = 1;
		} else {
			ch->is_fast = 0;
		}
	} else {
		ch->is_fast = 0;
	}

	if ((ch->size[TEAM1] > 1 || ch->size[TEAM2] > 1) && ch->is_fast) {
		Reply("Fast duels must be 1v1.");
		return;
	}

	// check both teams for players who have dueling disabled
	for (PLAYER *r = cd->ship_first; r != NULL; r = r->ship_next) {
		if (r->freq == ch->freq[TEAM1] || r->freq == ch->freq[TEAM2]) {
			if (pi(cd, r)->noduel_enabled) {
				ReplyFmt("%s has dueling disabled and can't be challenged.", r->name);
				return;
			}
		}
	}

	ch->use_elo = false;
	if (rated == true) {
		if (ch->size[TEAM1] != ch->size[TEAM2]) {
			ReplyFmt("Uneven team sizes are only allowed in unrated duels. Retry with !uch for an unrated challenge.");
			return;
		}

		ch->use_elo = true;
	}

	char rated_str[2][128] = { {0}, {0} };
	int avg_elo[2] = { 0, 0 };
	if (ch->use_elo) {
		avg_elo[TEAM1] = 0;
		avg_elo[TEAM2] = 0;

		for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
			if (p->freq == ch->freq[TEAM1]) {
				avg_elo[TEAM1] += pi(cd, p)->elo;
			} else if (p->freq == ch->freq[TEAM2]) {
				avg_elo[TEAM2] += pi(cd, p)->elo;
			}
		}

		avg_elo[TEAM1] /= ch->size[TEAM1];
		avg_elo[TEAM2] /= ch->size[TEAM2];

		snprintf(rated_str[TEAM1], 128,  "@%d ", avg_elo[TEAM1]);
		snprintf(rated_str[TEAM2], 128,  "@%d ", avg_elo[TEAM2]);
	}
	ch->avg_elo[TEAM1] = avg_elo[TEAM1];
	ch->avg_elo[TEAM2] = avg_elo[TEAM2];

	// the duel is acceptable at this point
	char team_text[2][128];
	duel_getteamtext(cd, ch->freq[TEAM1], team_text[TEAM1], 128);
	duel_getteamtext(cd, ch->freq[TEAM2], team_text[TEAM2], 128);

	// announce the duel to the teams
	cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1],
		"%s: You (%s)%s have challenged (%s)%s to a %dv%d %db%d duel%s%s",
		ch->use_elo ? "Rated Duel" : "Unrated Duel",
		team_text[TEAM1], rated_str[TEAM1],
		team_text[TEAM2], rated_str[TEAM2],
		ch->size[TEAM1], ch->size[TEAM2],
		ch->num_rounds, ch->win_by, strlen(ch->dc_name) > 0 ? " in Box " : "",
		strlen(ch->dc_name) > 0 ? ch->dc_name : "");
	cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2],
		"%s: You (%s)%s have been challenged by (%s)%s  to a %dv%d %db%d duel%s%s",
		ch->use_elo ? "Rated Duel" : "Unrated Duel",
		team_text[TEAM2], rated_str[TEAM2],
		team_text[TEAM1], rated_str[TEAM1],
		ch->size[TEAM1], ch->size[TEAM2],
		ch->num_rounds, ch->win_by, strlen(ch->dc_name) > 0 ? " in Box " : "",
		strlen(ch->dc_name) > 0 ? ch->dc_name : "");
		if (ch->use_elo) {
			float expected = elo_expected_score(ch->avg_elo[TEAM2], ch->avg_elo[TEAM1]);
			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "Rated Duel: Opponent@%d (%4.2f%% to win)  You@%d: (%4.2f%% to win)  ",
				ch->avg_elo[TEAM1], (1.0f - expected) * 100.0f,
				ch->avg_elo[TEAM2], expected * 100.0f);

			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1], "Rated Duel: Opponent@%d (%4.2f%% to win)  You@%d: (%4.2f%% to win)  ",
				ch->avg_elo[TEAM2], expected * 100.0f,
				ch->avg_elo[TEAM1], (1.0f - expected) * 100.0f);

			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "Rated Duel: Type !a to accept or !r to reject.");
		} else {
			cycad::PrivFreqShips(cd, ch->freq[TEAM2], "Unrated Duel: Type !a to accept or !r to reject.");
		}

	ch->timer = SetTimer(30000, cd->p1->pid, 0);

	pi(cd, cd->p1)->lastchallengetick = GetTicksMs();

	// set the challenge to in use
	ch->in_use = 1;
}


void
cmd_Accept(CORE_DATA *cd)
{
	if (pi(cd, cd->p1)->noduel_enabled) {
		Reply("Your dueling state is OFF. Toggle it on with !noduel to be able to use this command.");
		return;
	} else if (cd->p1->ship == SHIP_SPECTATOR) {
		Reply("This command can only be used by players in ship.");
		return;
	}

	// find the pending challenge
	CHALLENGE* ch = NULL;
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		if (ud(cd)->ch[i].in_use && ud(cd)->ch[i].freq[TEAM2] == cd->p1->freq) {
			ch = &ud(cd)->ch[i];
		}
	}
	if (ch == NULL) {
		Reply("No challenges pending.");
		return;
	}

	DUEL *d = NULL;
	
	if (strlen(ch->dc_name) > 0) {
		// a specific duel circle was requested
		d = ud_getopenduelbyname(cd, ch->dc_name);

		if (d == NULL) {
			// the requested duel wasnt available
			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1], "No available duel circle named \"%s\" found.", ch->dc_name);
			cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "No available duel circle named \"%s\" found.", ch->dc_name);
			ch_clear(cd, ch, true);
			return;
		}
	} else {
		// find any random duel circle
		d = ud_getopenduel(cd);
	}

	if (d == NULL) {
		// no duel circles were available
		cycad::PrivFreqShips(cd, ch->freq[TEAM1], "No available duel circles, cancelling duel.");
		cycad::PrivFreqShips(cd, ch->freq[TEAM2], "No available duel circles, cancelling duel.");
		ch_clear(cd, ch, true);
		return;
	}

	// verify that the team sizes havent changed
	if (cycad::playersinshiponfreq(cd, ch->freq[TEAM1]) != ch->size[TEAM1] ||
		cycad::playersinshiponfreq(cd, ch->freq[TEAM2]) != ch->size[TEAM2]) {
		cycad::PrivFreqShips(cd, ch->freq[TEAM1], "Teams have changed, cancelling duel.");
		cycad::PrivFreqShips(cd, ch->freq[TEAM2], "Teams have changed, cancelling duel.");
		ch_clear(cd, ch, true);
		return;
	}

	duel_init(cd, d, ch->freq[TEAM1], ch->freq[TEAM2], ch->use_elo, ch->avg_elo[TEAM1], ch->avg_elo[TEAM2], ch->num_rounds, ch->win_by, ch->is_fast);
	d->in_use = 1;

	// announce the duel was accepted
	if (ch->size[TEAM2] == 1) {
		cycad::PrivFreqShips(cd, ch->freq[TEAM2], "Duel accepted, good luck!");
	} else {
		cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM2], "Duel accepted by %s, good luck!", cd->p1->name);
	}
	cycad::PrivFreqShipsFmt(cd, ch->freq[TEAM1], "Duel accepted by %s, good luck!", cd->p1->name);

	
	// start the duel off
	dc_warpteams(cd, d->duel_circle);
	if (d->is_fast) {
		d->timer = SetTimer(3000, 0, 0);
	}

	// initialize the per-player cd for each dueler
	for (PLAYER *p = cd->ship_first; p != NULL; p = p->ship_next) {
		if (p->freq == ch->freq[TEAM1] || p->freq == ch->freq[TEAM2]) {
			pi(cd, p)->nfdw = false;
			pi(cd, p)->stats->deaths = 0;
			pi(cd, p)->stats->kills = 0;
			pi(cd, p)->stats->mostckills = 0;
			pi(cd, p)->stats->ckills = 0;
			pi(cd, p)->stats->dg = 0;
			pi(cd, p)->stats->dt = 0;
			for (uint32_t i = 0; i < MAX_ASSISTS; ++i) pi(cd, p)->last_damage[i].pid = PID_NONE;
		}
	}

	// release the challenge request
	ch_clear(cd, ch, true);
}


void
cmd_Reject(CORE_DATA *cd)
{
	if (cd->p1->ship == SHIP_SPECTATOR) {
		Reply("This command can only be used by players in ship.");
		return;
	}

	CHALLENGE *ch = ud_getchallengebyfreq(cd, cd->p1->freq);
	if (ch == NULL) {
		Reply("No challenges pending.");
	} else {
		cycad::PrivFreqShips(cd, ch->freq[TEAM1], "Challenge rejected.");
		cycad::PrivFreqShips(cd, ch->freq[TEAM2], "Challenge rejected.");
		ch_clear(cd, ch, true);
	}
}


void
cmd_LockDuel(CORE_DATA *cd)
{
	ud(cd)->LOCKED = !ud(cd)->LOCKED;
	ReplyFmt("Duelbot %slocked. Use %s again to toggle.",
		ud(cd)->LOCKED ? "" : "un", cd->cmd_argv[0]);
}
