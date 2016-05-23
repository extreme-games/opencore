
#include "userdata.hpp"
#include "db.hpp"

#include "util.hpp"


void
db_selectplayersdata(CORE_DATA *cd, char *name)
{
	if (ud(cd)->USEDB == false) return;

	char sf_name[64];
	QueryEscape(sf_name, sizeof(sf_name), name);

	cycad::QueryFmt(cd, DUELQUERY_RETRIEVESTATS, 0, name,
		"SELECT elo, pub_elo, max_elo, max_pub_elo, rated_kills, rated_deaths, rated_wins, rated_losses, " \
		"unrated_kills, unrated_deaths, unrated_wins, unrated_losses, pub_kills, pub_deaths " \
		"FROM %selo WHERE name = '%s';",
		ud(cd)->DBPREFIX, sf_name);
}


void
db_statscheck(CORE_DATA *cd, char *requester, char *target)
{
	if (ud(cd)->USEDB == false) return;

	char sf_name[64];
	QueryEscape(sf_name, sizeof(sf_name), target);

	cycad::QueryFmt(cd, DUELQUERY_STATSCHECK, 0, requester,
		"SELECT name, elo, max_elo, rated_wins, rated_losses, pub_elo, max_pub_elo, pub_kills, pub_deaths, " \
		"pub_kills + rated_kills + unrated_kills, pub_deaths + rated_deaths + unrated_deaths " \
		"FROM %selo WHERE name = '%s';",
		ud(cd)->DBPREFIX, sf_name);
}

// for efficiency
void
db_updatepubstats(CORE_DATA *cd, PLAYER *p)
{
	if (ud(cd)->USEDB == false) return;

	// dont store stats that werent retrieved from db (
	if (pi(cd, p)->stats_from_db == false) return;

	char sf_name[64];
	QueryEscape(sf_name, sizeof(sf_name), p->name);

	cycad::QueryFmt(cd, DUELQUERY_QUERYIGNORE, 0, NULL,
		"UPDATE %selo SET pub_elo = %d, max_pub_elo = %d, pub_kills = %d, pub_deaths = %d " \
		"WHERE name = '%s';",
		ud(cd)->DBPREFIX,
		pi(cd, p)->pub_elo,
		pi(cd, p)->max_pub_elo,
		pi(cd, p)->all_stats->pub->kills,
		pi(cd, p)->all_stats->pub->deaths,
		sf_name);
}


void
db_top10(CORE_DATA *cd, char *requester, bool pub)
{
	if (ud(cd)->USEDB == false) return;

	cycad::QueryFmt(cd, DUELQUERY_TOP10, 0, requester,
		"SELECT name, %s from %selo ORDER BY %s DESC LIMIT 10;",
		pub ? "pub_elo" : "elo",
		ud(cd)->DBPREFIX,
		pub ? "pub_elo" : "elo");
}


void
db_rank(CORE_DATA *cd, char *requester, bool pub, int elo)
{
	if (ud(cd)->USEDB == false) return;

	cycad::QueryFmt(cd, DUELQUERY_RANK, 0, requester,
		"SELECT COUNT(%s) FROM %selo WHERE %s > %d;",
		pub ? "pub_elo" : "elo",
		ud(cd)->DBPREFIX,
		pub ? "pub_elo" : "elo",
		elo);
}


void
db_addnewplayer(CORE_DATA *cd, PLAYER *p)
{
	if (ud(cd)->USEDB == false) return;

	char sf_name[64];
	QueryEscape(sf_name, sizeof(sf_name), p->name);

	char sf_squad[64];
	QueryEscape(sf_squad, sizeof(sf_squad), p->squad);

	cycad::QueryFmt(cd, DUELQUERY_QUERYIGNORE, 0, NULL,
		"INSERT INTO %selo (name, squad, elo, max_elo, pub_elo, max_pub_elo, rated_kills, rated_deaths, rated_wins, rated_losses, " \
		"unrated_kills, unrated_deaths, unrated_wins, unrated_losses, pub_kills, pub_deaths) " \
		"VALUES ('%s', '%s', 1200, 1200, 1200, 1200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);",
		ud(cd)->DBPREFIX, sf_name, sf_squad);
}


void
db_storeplayersdata(CORE_DATA *cd, PLAYER *p)
{
	if (ud(cd)->USEDB == false) return;

	// dont store stats that werent retrieved from db (
	if (pi(cd, p)->stats_from_db == false) return;

	char sf_name[64];
	QueryEscape(sf_name, sizeof(sf_name), p->name);

	char sf_squad[64];
	QueryEscape(sf_squad, sizeof(sf_squad), p->squad);

	cycad::QueryFmt(cd, DUELQUERY_QUERYIGNORE, 0, NULL,
		"UPDATE %selo SET squad = '%s', elo = %d, pub_elo = %d, " \
		"max_elo = %d, max_pub_elo = %d, " \
		"rated_kills = %d, rated_deaths = %d, rated_wins = %d, rated_losses = %d, " \
		"unrated_kills = %d, unrated_deaths = %d, unrated_wins = %d, unrated_losses = %d, " \
		"pub_kills = %d, pub_deaths = %d " \
		"WHERE name = '%s';",
		ud(cd)->DBPREFIX,
		sf_squad,
		pi(cd, p)->elo,
		pi(cd, p)->pub_elo,
		pi(cd, p)->max_elo,
		pi(cd, p)->max_pub_elo,
		pi(cd, p)->all_stats->rated->kills,
		pi(cd, p)->all_stats->rated->deaths,
		pi(cd, p)->all_stats->rated->wins,
		pi(cd, p)->all_stats->rated->losses,
		pi(cd, p)->all_stats->unrated->kills,
		pi(cd, p)->all_stats->unrated->deaths,
		pi(cd, p)->all_stats->unrated->wins,
		pi(cd, p)->all_stats->unrated->losses,
		pi(cd, p)->all_stats->pub->kills,
		pi(cd, p)->all_stats->pub->deaths,
		sf_name);
}


void
db_initdb(CORE_DATA *cd)
{
	if (ud(cd)->USEDB == false) return;

	cycad::QueryFmt(cd, DUELQUERY_QUERYIGNORE, 0, NULL, "CREATE TABLE IF NOT EXISTS `%selo` ( " \
		"`name` VARCHAR(24) PRIMARY KEY NOT NULL, " \
		"`squad` VARCHAR(24), " \
		"`elo` INT, " \
		"`max_elo` INT, " \
		"`pub_elo` INT, " \
		"`max_pub_elo` INT, " \
		"`rated_kills` INT UNSIGNED, " \
		"`rated_deaths` INT UNSIGNED, " \
		"`rated_wins` INT UNSIGNED, " \
		"`rated_losses` INT UNSIGNED, " \
		"`unrated_kills` INT UNSIGNED, " \
		"`unrated_deaths` INT UNSIGNED, " \
		"`unrated_wins` INT UNSIGNED, " \
		"`unrated_losses` INT UNSIGNED, " \
		"`pub_kills` INT UNSIGNED, " \
		"`pub_deaths` INT UNSIGNED " \
		");",
		ud(cd)->DBPREFIX);
}


void
db_elocheck(CORE_DATA *cd, char *requester, char *target, bool pub)
{
	if (ud(cd)->USEDB == false) return;

	char sf_target[64];
	QueryEscape(sf_target, sizeof(sf_target), target);

	cycad::QueryFmt(cd, DUELQUERY_ELOCHECK, 0, requester,
		"SELECT name, %s FROM %selo WHERE name = '%s';",
		pub ? "pub_elo" : "elo",
		ud(cd)->DBPREFIX, sf_target);
}


void
db_eloreset(CORE_DATA *cd, char *requester, char *target)
{
	if (ud(cd)->USEDB == false) {
		RmtMessage(requester, "This bot is not configured to use a database.");
		return;
	}
	
	char sf_target[64];
	QueryEscape(sf_target, sizeof(sf_target), target);

	// pull old db stats
	cycad::QueryFmt(cd, DUELQUERY_ELORESET, 0, requester,
		"SELECT '%s', elo, pub_elo, max_elo, max_pub_elo, rated_kills, rated_deaths, rated_wins, rated_losses " \
		"FROM %selo WHERE name = '%s';",
		sf_target, ud(cd)->DBPREFIX, sf_target);

	// update the stats
	cycad::QueryFmt(cd, DUELQUERY_QUERYIGNORE, 0, NULL,
		"UPDATE %selo SET elo = 1200, pub_elo = 1200, max_elo = 1200, max_pub_elo = 1200, " \
		"rated_kills = 0, rated_deaths = 0, rated_wins = 0, rated_losses = 0 " \
		"WHERE name = '%s';",
		ud(cd)->DBPREFIX,
		sf_target);

	// refresh the players stats
	db_selectplayersdata(cd, target);
}

