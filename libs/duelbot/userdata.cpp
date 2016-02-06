
#include "opencore.hpp"

#include "userdata.hpp"
#include "duel.hpp"
#include "dueltypes.hpp"


//
// setup and allocate user cd
//
void
ud_init(CORE_DATA *cd)
{
	ud(cd)->NUMCIRCLES = GetConfigInt("duelbot.NumCircles", 0);

	ud(cd)->duels = (DUEL_CIRCLE*)calloc(ud(cd)->NUMCIRCLES, sizeof(DUEL_CIRCLE));
	ud(cd)->ch = (CHALLENGE*)calloc(ud(cd)->NUMCIRCLES, sizeof(CHALLENGE));

	if (ud(cd)->duels == NULL || ud(cd)->ch == NULL) {
		StopBot("No Memory");
		return;
	}

	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		DUEL_CIRCLE *dcptr = &ud(cd)->duels[i];
		DUEL *d = (DUEL*)malloc(sizeof(DUEL));
		memset(d, 0, sizeof(DUEL));
		
		// set up cross references, these must stay constant
		dcptr->duel = d;
		d->duel_circle = dcptr;
	}
}



//
// free user cd
//
void
ud_delete(CORE_DATA *cd)
{
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		free(ud(cd)->duels[i].duel);
	}

	free(ud(cd)->duels);
	free(ud(cd)->ch);
}

//
// load everything in config
//
void
ud_loadconfig(CORE_DATA *cd)
{
	char value[64];

	for (int i = 1; i <= ud(cd)->NUMCIRCLES; ++i) {
		DUEL_CIRCLE *dc = &ud(cd)->duels[i - 1];

		char key[32];
		snprintf(key, 32, "duelbot.Circle%d", i);
		key[sizeof(key) - 1] = '\0';

		GetConfigString(key, value, sizeof(value), "512:512:512:512");
		dc->x[TEAM1] = cycad::atoiarg(value, 0, ':');
		dc->y[TEAM1] = cycad::atoiarg(value, 1, ':');
		dc->x[TEAM2] = cycad::atoiarg(value, 2, ':');
		dc->y[TEAM2] = cycad::atoiarg(value, 3, ':');
		cycad::delimargs(dc->name, value, 4, ':', 32);
	}

	GetConfigString("duelbot.dbprefix", ud(cd)->DBPREFIX, 64, "duel_");
	ud(cd)->USEDB = GetConfigInt("duelbot.usedb", 1) ? true : false;

	GetConfigString("duelbot.centersafe", value, 64, "512:512:512:512");
	ud(cd)->CSX1 = cycad::atoiarg(value, 0, ':');
	ud(cd)->CSY1 = cycad::atoiarg(value, 1, ':');
	ud(cd)->CSX2 = cycad::atoiarg(value, 2, ':') + 1;
	ud(cd)->CSY2 = cycad::atoiarg(value, 3, ':') + 1;

	GetConfigString("duelbot.centersafewarpto", value, 64, "512:512:512:512");
	ud(cd)->CSWTX1 = cycad::atoiarg(value, 0, ':');
	ud(cd)->CSWTY1 = cycad::atoiarg(value, 1, ':');
	ud(cd)->CSWTX2 = cycad::atoiarg(value, 2, ':') + 1;
	ud(cd)->CSWTY2 = cycad::atoiarg(value, 3, ':') + 1;

	GetConfigString("duelbot.graveyard", value, 64, "512:512:512:512");
	ud(cd)->GYX1 = cycad::atoiarg(value, 0, ':');
	ud(cd)->GYY1 = cycad::atoiarg(value, 1, ':');
	ud(cd)->GYX2 = cycad::atoiarg(value, 2, ':') + 1;
	ud(cd)->GYY2 = cycad::atoiarg(value, 3, ':') + 1;
	
	ud(cd)->BOTX = GetConfigInt("duelbot.BotX", 512);
	ud(cd)->BOTY = GetConfigInt("duelbot.BotY", 512);

	ud(cd)->BOTX = GetConfigInt("duelbot.botx", 512);
	ud(cd)->BOTY = GetConfigInt("duelbot.boty", 512);
	
	ud_loadcommon(cd);
}


//
// find an available challenge
//
CHALLENGE*
ud_getopenchallenge(CORE_DATA *cd) {
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		CHALLENGE *ch = &ud(cd)->ch[i];

		if (ch->in_use == 0) {
			return ch;
		}
	}

	return NULL;
}


//
// get a challenge by freq
//
CHALLENGE*
ud_getchallengebyfreq(CORE_DATA *cd, uint16_t freq) {
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		CHALLENGE *ch = &ud(cd)->ch[i];
		if (ch->in_use &&
			(ch->freq[TEAM1] == freq ||
			ch->freq[TEAM2] == freq)) {
			return ch;
		}
	}

	return NULL;
}


//
// reload config
//
void
ud_reloadconfig(CORE_DATA *cd)
{
	// reloading full config is too complex (lots of state), but some
	// parameters can be easily changed
	//ud_loadconfig(cd);
	
	ud_loadcommon(cd);
}


//
// reload simple config values that can change without reload
//
void
ud_loadcommon(CORE_DATA *cd)
{
	ud(cd)->ENTERDELAY	= GetConfigInt("duelbot.EnterDelay", 90) * 10; // convert to ms
	ud(cd)->SENDWATCHDAMAGE = GetConfigInt("duelbot.SendWatchDamage", 0) != 0;
	ud(cd)->WAITFORPOSPACKET = GetConfigInt("duelbot.WaitForPosPacket", 1) != 0;
	ud(cd)->MAXPERTEAM = GetConfigInt("duelbot.MaxPerTeam", 0);
	ud(cd)->MAXBYVAL = GetConfigInt("duelbot.MaxByVal", 5);
	ud(cd)->SWAPPEDSIDESENDSDUEL	= GetConfigInt("duelbot.SwappedSidesEndsDuel", 0) != 0;
	ud(cd)->MAXROUNDS = GetConfigInt("duelbot.MaxRounds", 10);
	ud(cd)->KILLTIMER = GetConfigInt("duelbot.KillTimer", 3500) + ud(cd)->ENTERDELAY;
	ud(cd)->GONETIME = GetConfigInt("duelbot.GoneTime", 2000);
	ud(cd)->SMALLTEAMGONETIME = GetConfigInt("duelbot.SmallTeamGoneTime", 1500);
	ud(cd)->SMALLTEAMKILLTIMER = GetConfigInt("duelbot.SmallTeamKillTimer", 3500) + ud(cd)->ENTERDELAY;
	ud(cd)->SPAWNPOINTRADIUS = GetConfigInt("duelbot.SpawnPointRadius", 3);
	ud(cd)->SHIPRESETONGYWARP = GetConfigInt("duelbot.ShipResetOnGYWarp", 0) != 0;
	ud(cd)->SWAPCENTERANDGRAVEYARD= GetConfigInt("duelbot.SwapCenterAndGraveyard", 0) != 0;
	ud(cd)->REMOVEANTI = GetConfigInt("duelbot.RemoveAnti", 0) != 0;
	ud(cd)->PLAYERSFORARENASPEW = GetConfigInt("duelbot.PlayersForArenaSpew", 2);
	ud(cd)->DEFAULT1V1FAST = GetConfigInt("duelbot.Default1v1Fast", 1) != 0;
	ud(cd)->ASSISTINTERVAL = GetConfigInt("duelbot.AssistInterval", 3000) != 0;
	ud(cd)->NCINTERVAL = GetConfigInt("duelbot.NcInterval", 30000) * 1000;

	char temp[128];
	GetConfigString("duelbot.NoAntiBoundary", temp, 128, "512:512:512:512");
	ud(cd)->NABX1 = cycad::atoiarg(temp, 0, ':') * 16;
	ud(cd)->NABY1 = cycad::atoiarg(temp, 1, ':') * 16;
	ud(cd)->NABX2 = cycad::atoiarg(temp, 2, ':') * 16;
	ud(cd)->NABY2 = cycad::atoiarg(temp, 3, ':') * 16;
}


//
// find an open duel
//
DUEL*
ud_getopenduel(CORE_DATA *cd)
{
	int random = rrand() % ud(cd)->NUMCIRCLES;

	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		DUEL_CIRCLE *dc = &ud(cd)->duels[(random + i) % ud(cd)->NUMCIRCLES];
	
		if (dc->duel->in_use == 0) {
			return dc->duel;
		}
	}

	return NULL;
}


//
// find an open duel circle with the specified name
//
DUEL*
ud_getopenduelbyname(CORE_DATA *cd, const char *name) {
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		DUEL_CIRCLE *dc = &ud(cd)->duels[i];

		if (dc->duel->in_use == 0 && strncasecmp(dc->name, name, strlen(name)) == 0) {
			return dc->duel;
		}
	}

	return NULL;
}


//
// find a duel associated with a player by pid
//
DUEL*
ud_getduelbypid(CORE_DATA *cd, uint16_t pid)
{
	PLAYER *p = FindPlayerPid(pid, MATCH_HERE);
	if (p == NULL) {
		return NULL;
	}

	return ud_getduelbyfreq(cd, p->freq);
}


//
// find a duel based on a freq
//
DUEL*
ud_getduelbyfreq(CORE_DATA *cd, uint16_t freq)
{
	for (int i = 0; i < ud(cd)->NUMCIRCLES; ++i) {
		DUEL *d = ud(cd)->duels[i].duel;
		if (d->in_use &&
			(freq == d->freq[TEAM1] ||
			freq == d->freq[TEAM2])) {
			return d;
		}
	}

	return NULL;
}


bool
ud_isduelcircle(CORE_DATA *cd, uint16_t x, uint16_t y)
{
	return !ud_iscenterarea(cd, x, y);
}


bool
ud_iscentersafe(CORE_DATA *cd, uint16_t x, uint16_t y)
{
	return x >= ud(cd)->CSX1 * 16 && y >= ud(cd)->CSY1 * 16 &&
		   x <  ud(cd)->CSX2 * 16 && y <  ud(cd)->CSY2 * 16;
}


bool
ud_isgraveyard(CORE_DATA *cd, uint16_t x, uint16_t y)
{
	return x >= ud(cd)->GYX1 * 16 && y >= ud(cd)->GYY1 * 16 &&
		   x <  ud(cd)->GYX2 * 16 && y <  ud(cd)->GYY2 * 16;
}


bool
ud_iscenterarea(CORE_DATA *cd, uint16_t x, uint16_t y)
{
	if (ud_isgraveyard(cd, x, y) || ud_iscentersafe(cd, x, y)) {
		return true;
	}

	return false;
}

bool
ud_isnoantiboundary(CORE_DATA *cd, uint16_t x, uint16_t y)
{
	if (x > ud(cd)->NABX1 && y > ud(cd)->NABY1 && x < ud(cd)->NABX2 && y < ud(cd)->NABY2)
		return true;

	return false;
}


void
ud_warptograveyard(CORE_DATA *cd, PLAYER *p) {
	PrivMessageFmt(p, "*warpto %d %d",
		ud(cd)->GYX1 + (rrand() % (ud(cd)->GYX2 - ud(cd)->GYX1)) + 1,
		ud(cd)->GYY1 + (rrand() % (ud(cd)->GYY2 - ud(cd)->GYY1)) + 1);

	if (ud(cd)->SHIPRESETONGYWARP) {
		PrivMessage(p, "*shipreset");
	}
}


void
ud_warptocenterwithwarpto(CORE_DATA *cd, PLAYER *p)
{
	PrivMessageFmt (p, "*warpto %d %d",
		ud(cd)->CSWTX1 + (rrand() % (ud(cd)->CSWTX2 - ud (cd)->CSWTX1)) + 1,
		ud(cd)->CSWTY1 + (rrand() % (ud(cd)->CSWTY2 - ud (cd)->CSWTY1)) + 1);
	PrivMessage (p, "*shipreset");
}
