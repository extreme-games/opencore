
#pragma once

#include "dueltypes.hpp"

//
// command types
//
enum CMD {
	CMD_CHALLENGE,		// Challenge someone to a duel
	CMD_UCHALLENGE,		// Challenge someone to an unrated duel
	CMD_SLOWCHALLENGE,	// Challenge someone to a slow duel
	CMD_ACCEPT,			// Accept a duel
	CMD_REJECT,			// Reject a duel
	CMD_NODUEL,			// Set yourself so nobody can !ch you
	CMD_SCORERESET,		// Reset your score
	CMD_DUELINFO,		// Show duel information
	CMD_DUELHELP,		// Show help
	CMD_RATING,			// Show elo rating
	CMD_TOP10,			// Show top 10 elo ratings
	CMD_RANK,			// Check rank
	CMD_PUBRATING,		// Check your pub rating
	CMD_PUBTOP10,		// Top 10 pub Elo
	CMD_PUBRANK,		// Ranking with pub Elo
	CMD_STATS,			// Stats from DB
	
	CMD_LOCKDUEL,		// Lock the duel arena
	CMD_ELORESET,		// Reset a target player's Elo
};
