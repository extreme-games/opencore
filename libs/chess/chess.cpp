/*
 * Copyright (c) 2007 cycad <cycad@zetasquad.com>
 *
 * This software is provided 'as-is', without any express or implied 
 * warranty. In no event will the author be held liable for any damages 
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you
 *     must not claim that you wrote the original software. If you use 
 *     this software in a product, an acknowledgment in the product 
 *     documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and
 *     must not be misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */
/*
 * A chess bot, 11/12/07.
 */

#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "opencore.hpp"

#if 0
// This small program is used to generate the LVZ file data.
#include <stdio.h>

#define BASEX 16
#define BASEY 16
#define  TILE 86
int main(int argc, char *argv[]) {
	FILE *f = fopen("out.txt", "ab");
	for (int y = 0; y < 8; ++y)
	for (int x = 0; x < 8; ++x)
	for (int t = 0; t < 6; ++t) {
		fprintf(f, "%d,%d,IMAGE%d,AfterWeapons,ServerControlled,0,%d%d%d%d\r\n",
		    BASEX + x * TILE,
		    BASEY + (7 * TILE) - (y * TILE),
		    t, x, y, 0, t);
		fprintf(f, "%d,%d,IMAGE%d,AfterWeapons,ServerControlled,0,%d%d%d%d\r\n",
		    BASEX + x * TILE,
		    BASEY + (7 * TILE) - (y * TILE),
		    6 + t, x, y, 1, t);
	}

	fclose(f);
	return 0;
}
#endif


// exported symbol declarations
extern "C" void CmdBlack(CORE_DATA *cd);
extern "C" void CmdGameInfo(CORE_DATA *cd);
extern "C" void CmdChessHelp(CORE_DATA *cd);
extern "C" void CmdMove(CORE_DATA *cd);
extern "C" void CmdQuit(CORE_DATA *cd);
extern "C" void CmdWhite(CORE_DATA *cd);
extern "C" void GameEvent(CORE_DATA *cd);
extern "C" LIB_DATA REGISTRATION_INFO;

// exported registration info for the core
LIB_DATA REGISTRATION_INFO = {
	"Chess",
	"cycad",
	"1.1",
	__DATE__,
	__TIME__,
	"A Player-vs-Player chess bot",
	CORE_VERSION,
	0,
	GameEvent
};

#define UD(ptr)		((USER_DATA*)ptr->user_data)
#define MIN(a,b)	((a) < (b) ? (a) : (b))

typedef char		COLOR;
#define COLOR_NONE	((COLOR)0)
#define COLOR_WHITE	((COLOR)1)
#define COLOR_BLACK	((COLOR)2)

typedef char		TYPE;
#define TYPE_KING	'K'
#define TYPE_QUEEN	'Q'
#define TYPE_BISHOP	'B'
#define TYPE_KNIGHT	'N'
#define TYPE_ROOK	'R'
#define TYPE_PAWN	'P'
#define TYPE_NONE	'\0'

// pieces on the chessboard
typedef struct piece PIECE;
struct piece {
	TYPE	type;
	COLOR	color;	
};

// the board
typedef PIECE**		BOARD;

// bot-specific data
typedef struct user_data USER_DATA;
struct user_data {
	BOARD	  board;	// the playing board

	char	 white[24];	// white player's name
	char	 black[24];	// black player's name

	bool	 white_can_castle;	// set when white can castle
	bool	 black_can_castle;	// set when black can castle

	COLOR	 to_move;	// whos turn it is it move

	long	 timer;		// set when a player leaves the arena

	ticks_ms_t start_tick;	// game start tick

	bool	 in_progress;	// set when a game is in progress
};


// local prototypes
static COLOR	GetColor(USER_DATA *ud, char *name);
static COLOR	GetOppositeColor(COLOR c);
static PIECE*	GetPieceAt(BOARD b, int x, int y);
static bool	ColorCanCastle(USER_DATA *ud, COLOR c);
static bool	IsCheckmatedBy(BOARD b, COLOR c, int king, int kingy);
static bool	IsCoordAttackedBy(BOARD b, COLOR c, int x, int y);
static bool	IsPlaying(USER_DATA *ud, char *name);
static bool	ParseCoords(char *loc, int *x, int *y);
static bool	PieceCanAttack(BOARD b, COLOR c, TYPE t, int x, int y, int dx, int dy);
static char*	GetColorText(COLOR c);
static char*	GetOpponentName(USER_DATA *ud, char *name);
static char*	GetPlayerName(USER_DATA *ud, COLOR c);
static char*	GetTypeName(TYPE t);
static char*	TryMove(USER_DATA *ud, COLOR c, int x, int y, int dx, int dy);
static void	FindKing(BOARD b, COLOR c, int *x, int *y);
static void	LvzActivate(USER_DATA *ud, PLAYER *p, COLOR c, TYPE t, int x, int y, bool show);
static void	LvzActivateBoard();
static void	LvzClearAll(USER_DATA *ud, PLAYER *p);
static void	LvzDrawAll(USER_DATA *ud, PLAYER *p, bool show);
static void	LvzToMove(COLOR c, PLAYER *p);
static void	MoveAndReplaceTarget(USER_DATA *ud, BOARD b, int x, int y, int dx, int dy);
static void	UnsetPlayerName(USER_DATA *ud, char *name);
static void	BoardReset(USER_DATA *ud, BOARD b);
static void	SetPieceAt(BOARD b, char *loc, COLOR c, TYPE t);
static void	SetPieceAt(BOARD b, int x, int y, COLOR c, TYPE t);
static void	StartGame(USER_DATA *ud);
static void	StopGame(USER_DATA *ud, char *fmt_reason, ...);


// function definitions
static inline
PIECE*
GetPieceAt(BOARD b, int x, int y)
{
	assert (x >= 0 && x < 8 && y >=0 && y < 8);

	return &b[x][y];
}


static inline
void
SetPieceAt(BOARD b, int x, int y, COLOR c, TYPE t)
{
	assert (x >= 0 && x < 8 && y >=0 && y < 8);

	b[x][y].color = c;
	b[x][y].type = t;
}


static
bool
ParseCoords(char *loc, int *x, int *y)
{
	char cx = tolower(loc[0]);
	char cy = loc[1];
	if (cx >= 'a' && cx <= 'h' &&
	    cy >= '1' && cy <= '8') {
		*x = cx - 'a';
		*y = cy - '1';
		return true;
	}

	return false;
}


static
void
SetPieceAt(BOARD b, char *loc, COLOR c, TYPE t)
{
	int x, y;
	bool rv = ParseCoords(loc, &x, &y);

	// this is a utility function so asserting here is okay
	// since the argument isnt user input
	assert (rv == true);

	SetPieceAt(b, x, y, c, t);
}


static
void
BoardReset(USER_DATA *ud, BOARD b) {
	// clear the board
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			SetPieceAt(b, x, y, COLOR_NONE, TYPE_NONE);
		}
	}

	// setup the pawns
	for (int i = 0; i < 8; ++i) {
		SetPieceAt(b, i, 1, COLOR_WHITE, TYPE_PAWN);
		SetPieceAt(b, i, 6, COLOR_BLACK, TYPE_PAWN);
	}

	// setup the rest of the board

	// white
	SetPieceAt(b, "a1", COLOR_WHITE, TYPE_ROOK);
	SetPieceAt(b, "b1", COLOR_WHITE, TYPE_KNIGHT);
	SetPieceAt(b, "c1", COLOR_WHITE, TYPE_BISHOP);
	SetPieceAt(b, "d1", COLOR_WHITE, TYPE_QUEEN);
	SetPieceAt(b, "e1", COLOR_WHITE, TYPE_KING);
	SetPieceAt(b, "f1", COLOR_WHITE, TYPE_BISHOP);
	SetPieceAt(b, "g1", COLOR_WHITE, TYPE_KNIGHT);
	SetPieceAt(b, "h1", COLOR_WHITE, TYPE_ROOK);

	// black
	SetPieceAt(b, "a8", COLOR_BLACK, TYPE_ROOK);
	SetPieceAt(b, "b8", COLOR_BLACK, TYPE_KNIGHT);
	SetPieceAt(b, "c8", COLOR_BLACK, TYPE_BISHOP);
	SetPieceAt(b, "d8", COLOR_BLACK, TYPE_QUEEN);
	SetPieceAt(b, "e8", COLOR_BLACK, TYPE_KING);
	SetPieceAt(b, "f8", COLOR_BLACK, TYPE_BISHOP);
	SetPieceAt(b, "g8", COLOR_BLACK, TYPE_KNIGHT);
	SetPieceAt(b, "h8", COLOR_BLACK, TYPE_ROOK);
}


static
void
LvzActivateBoard()
{
	PubMessageFmt("*objon 6");
}

uint32_t
LvzGetObjId(COLOR c, TYPE t, int x, int y)
{
	int type = 0;
	switch (t) {
	case TYPE_KING:		type = 0; break;
	case TYPE_QUEEN:	type = 1; break;
	case TYPE_BISHOP:	type = 2; break;
	case TYPE_KNIGHT:	type = 3; break;
	case TYPE_ROOK:		type = 4; break;
	case TYPE_PAWN:		type = 5; break;
	default:		assert(0);break;
	}
	// build the objects lvz id, format is
	// [X Square][Y Square][Color][Piece Type]
	char lvz[32];
	snprintf(lvz, 32, "%d%d%d%d",
		x, y, c == COLOR_WHITE ? 0 : 1, type);
	
	return (uint32_t)atoi(lvz);
}

static
void
LvzActivate(USER_DATA *ud, PLAYER *p, COLOR c, TYPE t, int x, int y, bool show)
{
	uint32_t objid = LvzGetObjId(c, t, x, y);

	if (show) {
		if (p) {
			PrivMessageFmt(p, "*objon %u", objid);
		} else {
			PubMessageFmt("*objon %u", objid);
		}
	} else {
		if (p) {
			PrivMessageFmt(p, "*objoff %u", objid);
		} else {
			PubMessageFmt("*objoff %u", objid);
		}
	}
}


/*
 * Display the LVZ that shows whos turn it is.
 */
static
void
LvzToMove(COLOR c, PLAYER *p)
{
	if (c == COLOR_WHITE) {
		if (p) {
			PrivMessageFmt(p, "*objon 7");
			PrivMessageFmt(p, "*objoff 8");
		} else {
			PubMessageFmt("*objon 7");
			PubMessageFmt("*objoff 8");
		}
	} else if (c == COLOR_BLACK) {
		if (p) {
			PrivMessageFmt(p, "*objon 8");
			PrivMessageFmt(p, "*objoof 7");

		} else {
			PubMessageFmt("*objon 8");
			PubMessageFmt("*objoff 7");
		}

	} else {
		if (p) {
			PrivMessageFmt(p, "*objoff 7");
			PrivMessageFmt(p, "*objoff 8");

		} else {
			PubMessageFmt("*objoff 7");
			PubMessageFmt("*objoff 8");

		}
	}
}


#if 0
/*
 * This function is no longer necessary with LVZ graphics but
 * may still be useful for debugging at some point.
 */
static
void
DrawBoard(BOARD b)
{
	text display
	char line[128];
	int linei = 0;

	ArenaMessage("+---------------------+");
	ArenaMessage("|   a b c d e f g h   |");

	PIECE *p;
	for (int y = 7; y >= 0; --y) {
		line[linei++] = '|';
		line[linei++] = ' ';
		line[linei++] = '1' + y;
		line[linei++] = '|';
		for (int x = 0; x < 8; ++x) {
			p = GetPieceAt(b, x, y);
			char c;
			if (p->type == TYPE_NONE) {
				c = ' ';
			} else {
				if (p->color == COLOR_WHITE) {
					c = p->type;
				} else {
					c = p->type + ('a' - 'A');
				}
			}
			line[linei++] = c;
			line[linei++] = '|';
		}
		line[linei++] = '1' + y;
		line[linei++] = ' ';
		line[linei++] = '|';
		line[linei] = '\0';
		linei = 0;

		ArenaMessage("|  +-+-+-+-+-+-+-+-+  |");
		ArenaMessage(line);
	}
	ArenaMessage("|  +-+-+-+-+-+-+-+-+  |");
	ArenaMessage("|   a b c d e f g h   |");
	ArenaMessage("|_____________________|");
}
#endif


static
bool
IsPlaying(USER_DATA *ud, char *name)
{
	if (strncasecmp(ud->white, name, 24) == 0 ||
	    strncasecmp(ud->black, name, 24) == 0) {
		return true;
	}

	return false;	
}


static
void
StartGame(USER_DATA *ud)
{
	ud->start_tick = GetTicksMs();
	ud->white_can_castle = true;
	ud->black_can_castle = true;

	// clear the existing board
	LvzClearAll(ud, NULL);

	BoardReset(ud, ud->board);

	// draw the board
	LvzDrawAll(ud, NULL, true);
	LvzToMove(COLOR_WHITE, NULL);

	ud->in_progress = true;
	ud->to_move = COLOR_WHITE;

	ArenaMessageFmt("%s (White) vs. %s (Black) Has Begun!", ud->white, ud->black);
	ArenaMessageFmt("%s (White) to Move", ud->white);
}


static
void
StopGame(USER_DATA *ud, char *fmt_reason, ...)
{
	va_list ap;
	va_start(ap, fmt_reason);

	char reason[256];
	vsnprintf(reason, 256, fmt_reason, ap);

	ArenaMessageFmt("%s (White) vs. %s (Black) has ended: %s",
	    ud->white, ud->black, reason);
	ud->in_progress = false;
	ud->white[0] = '\0';
	ud->black[0] = '\0';

	va_end(ap);
}


static
void
UnsetPlayerName(USER_DATA *ud, char *name)
{
	if (strncasecmp(ud->white, name, 24) == 0) {
		ud->white[0] = '\0';
	}
	if (strncasecmp(ud->black, name, 24) == 0) {
		ud->black[0] = '\0';
	}
}


static
char*
GetOpponentName(USER_DATA *ud, char *name)
{
	if (strncasecmp(ud->white, name, 24) == 0) {
		return ud->black;
	} else {
		return ud->white;
	}
}


static
char*
GetPlayerName(USER_DATA *ud, COLOR c)
{
	if (c == COLOR_WHITE) {
		return ud->white;
	} else if (c == COLOR_BLACK) {
		return ud->black;
	}

	return "";
}


static
COLOR
GetColor(USER_DATA *ud, char *name)
{
	if (strncasecmp(ud->white, name, 24) == 0) {
		return COLOR_WHITE;
	} else if (strncasecmp(ud->black, name, 24) == 0) {
		return COLOR_BLACK;
	}

	return COLOR_NONE;
}


static
char*
GetTypeName(TYPE t)
{
	switch (t) {
	case TYPE_KING:		return "King";
	case TYPE_QUEEN:	return "Queen";
	case TYPE_BISHOP:	return "Bishop";
	case TYPE_KNIGHT:	return "Knight";
	case TYPE_ROOK:		return "Rook";
	case TYPE_PAWN:		return "Pawn";
	default:		return "Unknown";
	}
}


static
char*
GetColorText(COLOR c)
{
	if (c == COLOR_WHITE) {
		return "White";
	} else if (c == COLOR_BLACK) {
		return "Black";
	}

	return "None";
}


static
COLOR
GetOppositeColor(COLOR c)
{
	if (c == COLOR_WHITE) {
		return COLOR_BLACK;
	} else if (c == COLOR_BLACK) {
		return COLOR_WHITE;
	}

	return COLOR_NONE;
}


static
void
FindKing(BOARD b, COLOR c, int *x, int *y)
{
	*x = 0;
	*y = 0;

	PIECE *p;
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			p = GetPieceAt(b, j, i);
			if (p->type == TYPE_KING && p->color == c) {
				*x = j;
				*y = i;
			}
		}
	}
}


static
bool
PieceCanAttack(BOARD b, COLOR c, TYPE t, int x, int y, int dx, int dy)
{
	if (dx < 0 || dx >= 8) {
		return false;
	} else if (dy < 0 || dy >= 8) {
		return false;
	} else if (x == dx && y == dy) {
		// cant attack self
		return false;
	}

	int tx, ty; // temp x and y values
	PIECE *p;

	switch (t) {
	case TYPE_KING:
		for (int j = x - 1; j <= x + 1; ++j)
		for (int i = y - 1; i <= y + 1; ++i) {
			if (j == dx && i == dy) {
				return true;
			}
		}
		break;
	case TYPE_QUEEN:
		return PieceCanAttack(b, c, TYPE_BISHOP, x, y, dx, dy) ||
		    PieceCanAttack(b, c, TYPE_ROOK, x, y, dx, dy);
	case TYPE_ROOK:
		// search up
		for (int i = x + 1; i < 8; ++i) {
			if (i == dx && y == dy) {
				return true;
			}
			p = GetPieceAt(b, i, y);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search down
		for (int i = x - 1; i >= 0; --i) {
			if (i == dx && y == dy) {
				return true;
			}
			p = GetPieceAt(b, i, y);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search right
		for (int i = y + 1; i < 8; ++i) {
			if (i == dy && x == dx) {
				return true;
			}
			p = GetPieceAt(b, x, i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search left
		for (int i = y - 1; i >= 0; --i) {
			if (i == dy && x == dx) {
				return true;
			}
			p = GetPieceAt(b, x, i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		break;
	case TYPE_BISHOP:
		// search up-right
		for (int i = 1; x + i < 8 && y + i < 8; ++i) {
			if (x + i == dx && y + i == dy) {
				return true;
			}
			p = GetPieceAt(b, x + i, y + i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search down-right
		for (int i = 1; x + i < 8 && y - i >= 0; ++i) {
			if (x + i == dx && y - i == dy) {
				return true;
			}
			p = GetPieceAt(b, x + i, y - i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search up-left
		for (int i = 1; x - i >= 0 && y + i < 8; ++i) {
			if (x - i == dx && y + i == dy) {
				return true;
			}
			p = GetPieceAt(b, x - i, y + i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		// search down-left
		for (int i = 1; x - i >= 0 && y - i >= 0; ++i) {
			if (x - i == dx && y - i == dy) {
				return true;
			}
			p = GetPieceAt(b, x - i, y - i);
			if (p->type != TYPE_NONE) {
				break;
			}
		}
		break;
	case TYPE_KNIGHT:
		tx = abs(dx - x);
		ty = abs(dy - y);
		if ((tx == 2 && ty == 1) ||
		    (tx == 1 && ty == 2)) {
			return true;
		}
		break;
	case TYPE_PAWN:
		// pawn special-case gheyness
		if (c == COLOR_WHITE) {
			if (y == 7) {
				return false;
			}
			if (x == dx) {
				// moving forward
				if (GetPieceAt(b, x, y + 1)->type == TYPE_NONE
				    && y + 1 == dy) {
					return true;
				}
				// initial 2 square move option
				if (y == 1 &&
				    GetPieceAt(b, x, y + 1)->type == TYPE_NONE &&
				    GetPieceAt(b, x, y + 2)->type == TYPE_NONE &&
				    y + 2 == dy) {
					return true;
				}
			}
			if (x > 0) {
				// check attack left
				if (GetPieceAt(b, x - 1, y + 1)->color == COLOR_BLACK &&
				    GetPieceAt(b, x - 1, y + 1)->type != TYPE_NONE) {
					if (dx == x - 1 && dy == y + 1) {
						return true;
					}

				}
			}

			if (x < 7) {
				// check attack right
				if (GetPieceAt(b, x + 1, y + 1)->color == COLOR_BLACK &&
				    GetPieceAt(b, x + 1, y + 1)->type != TYPE_NONE) {
					if (dx == x + 1 && dy == y + 1) {
						return true;
					}

				}
			}

		} else if (c == COLOR_BLACK) {
			if (y == 0) {
				return false;
			}
			if (x == dx) {
				// moving forward
				if (GetPieceAt(b, x, y - 1)->type == TYPE_NONE
				    && y - 1 == dy) {
					return true;
				}
				// initial 2 square move option
				if (y == 6 &&
				    GetPieceAt(b, x, y - 1)->type == TYPE_NONE &&
				    GetPieceAt(b, x, y - 2)->type == TYPE_NONE &&
				    y - 2 == dy) {
					return true;
				}
			}
			if (x > 0) {
				// check attack left
				if (GetPieceAt(b, x - 1, y - 1)->color == COLOR_WHITE &&
				    GetPieceAt(b, x - 1, y - 1)->type != TYPE_NONE) {
					if (dx == x - 1 && dy == y - 1) {
						return true;
					}

				}
			}
			if (x < 7) {
				// check attack right
				if (GetPieceAt(b, x + 1, y - 1)->color == COLOR_WHITE &&
				    GetPieceAt(b, x + 1, y - 1)->type != TYPE_NONE) {
					if (dx == x + 1 && dy == y - 1) {
						return true;
					}

				}
			}

		}
		break;
	}

	return false;
}


static
bool
IsCoordAttackedBy(BOARD b, COLOR c, int x, int y)
{
	PIECE *p;
	for (int j = 0; j < 8; ++j) {
	for (int i = 0; i < 8; ++i) {
		p = GetPieceAt(b, i, j);
		if (p->color == c &&
		    PieceCanAttack(b, c, p->type, i, j, x, y) == true) {
			return true;
		}
	}
	}
	return false;
}


static
void
LvzClearAll(USER_DATA *ud, PLAYER *p)
{
	PIECE *piece;
	for (int j = 0; j < 8; ++j)
	for (int i = 0; i < 8; ++i) {
		piece = GetPieceAt(ud->board, i, j);
		if (piece->type == TYPE_NONE) {
			continue;
		}

		LvzActivate(ud, p, piece->color, piece->type, i, j, false);
	}
}

static
void
LvzDrawAll(USER_DATA *ud, PLAYER *p, bool show)
{
	PIECE *piece;
	for (int j = 0; j < 8; ++j)
	for (int i = 0; i < 8; ++i) {
		piece = GetPieceAt(ud->board, i, j);
		if (piece->type == TYPE_NONE) {
			continue;
		}

		LvzActivate(ud, p, piece->color, piece->type, i, j, show);
	}
}


static
bool
ColorCanCastle(USER_DATA *ud, COLOR c)
{
	if (c == COLOR_WHITE) {
		return ud->white_can_castle;
	} else if (c == COLOR_BLACK) {
		return ud->black_can_castle;
	}

	return false;
}


static
bool
IsCastleMove(COLOR c, int x, int y, int dx, int dy)
{
	if (c == COLOR_WHITE && x == 4 && y == 0) {
		return (dx == 6 && dy == 0) ||
		    (dx == 2 && dy == 0);
	} else if (c == COLOR_BLACK && x == 4 && y == 7) {
		return (dx == 6 && dy == 7) ||
		    (dx == 2 && dy == 7);
	}	

	return false;
}


static
void
MoveAndReplaceTarget(USER_DATA *ud, BOARD b, int x, int y, int dx, int dy)
{
	PIECE *p = GetPieceAt(b, x, y);
	PIECE old_p = *p;

	// clear drawing at old location
	LvzActivate(ud, NULL, p->color, p->type, x, y, false);

	// unset target piece
	PIECE *t = GetPieceAt(b, dx, dy);
	if (t->type != TYPE_NONE) {
		LvzActivate(ud, NULL, t->color, t->type, dx, dy, false);
	}

	// clear old location
	SetPieceAt(b, x, y, COLOR_NONE, TYPE_NONE);

	// copy old piece to new location
	*t = old_p;

	// draw it
	LvzActivate(ud, NULL, t->color, t->type, dx, dy, true);
}


/*
 * TODO: rewrite this to remove duplicated code
 */
static
char*
TryCastle(USER_DATA *ud, COLOR c, int x, int y, int dx, int dy)
{
	BOARD b = ud->board;
	// coordinates were already verified by IsCastleMove()

	COLOR opp = GetOppositeColor(c);
	if (IsCoordAttackedBy(b, opp, x, y) == true) {
		return "Can't castle out of check";
	}

	if (c == COLOR_WHITE) {
		if (dx == 6) {
			if (IsCoordAttackedBy(b, opp, 5, 0) ||
			    IsCoordAttackedBy(b, opp, 6, 0)) {
				return "Can't castle through or into check";
			}
			if (GetPieceAt(b, 5, 0)->type != TYPE_NONE ||
			    GetPieceAt(b, 6, 0)->type != TYPE_NONE) {
				return "Can't castle through pieces";
			}
			if (GetPieceAt(b, 7, 0)->type != TYPE_ROOK ||
			    GetPieceAt(b, 7, 0)->color != COLOR_WHITE) {
				return "No rook";
			}

			MoveAndReplaceTarget(ud, b, x, y, dx, dy);
			MoveAndReplaceTarget(ud, b, 7, 0, 5, 0);

			ArenaMessageFmt("%s (%s) Moves: Castle kingside",
			    GetPlayerName(ud, c), GetColorText(c));

			return NULL;
		} else if (dx == 2) {
			// castle queenside
			if (IsCoordAttackedBy(b, opp, 2, 0) ||
			    IsCoordAttackedBy(b, opp, 3, 0)) {
				return "Can't castle through or into check";
			}
			if (GetPieceAt(b, 1, 0)->type != TYPE_NONE ||
			    GetPieceAt(b, 2, 0)->type != TYPE_NONE ||
			    GetPieceAt(b, 3, 0)->type != TYPE_NONE) {
				return "Can't castle through pieces";
			}
			if (GetPieceAt(b, 0, 0)->type != TYPE_ROOK ||
			    GetPieceAt(b, 0, 0)->color != COLOR_WHITE) {
				return "No rook";
			}

			MoveAndReplaceTarget(ud, b, x, y, dx, dy);
			MoveAndReplaceTarget(ud, b, 0, 0, 3, 0);

			ArenaMessageFmt("%s (%s) Moves: Castle queenside",
			    GetPlayerName(ud, c), GetColorText(c));

			return NULL;
		}
	} else if (c == COLOR_BLACK) {
		if (dx == 6) {
			// castle kingside
			if (IsCoordAttackedBy(b, opp, 5, 7) ||
			    IsCoordAttackedBy(b, opp, 6, 7)) {
				return "Can't castle through or into check";
			}
			if (GetPieceAt(b, 5, 7)->type != TYPE_NONE ||
			    GetPieceAt(b, 6, 7)->type != TYPE_NONE) {
				return "Can't castle through pieces";
			}
			if (GetPieceAt(b, 7, 7)->type != TYPE_ROOK ||
			    GetPieceAt(b, 7, 7)->color != COLOR_BLACK) {
				return "No rook";
			}

			MoveAndReplaceTarget(ud, b, x, y, dx, dy);
			MoveAndReplaceTarget(ud, b, 7, 7, 5, 7);

			ArenaMessageFmt("%s (%s) Moves: Castle kingside",
			    GetPlayerName(ud, c), GetColorText(c));

			return NULL;
		} else if (dx == 2) {
			// castle queenside
			if (IsCoordAttackedBy(b, opp, 2, 7) ||
			    IsCoordAttackedBy(b, opp, 3, 7)) {
				return "Can't castle through or into check";
			}
			if (GetPieceAt(b, 1, 7)->type != TYPE_NONE ||
			    GetPieceAt(b, 2, 7)->type != TYPE_NONE ||
			    GetPieceAt(b, 3, 7)->type != TYPE_NONE) {
				return "Can't castle through pieces";
			}
			if (GetPieceAt(b, 0, 7)->type != TYPE_ROOK ||
			    GetPieceAt(b, 0, 7)->color != COLOR_BLACK) {
				return "No rook";
			}

			MoveAndReplaceTarget(ud, b, x, y, dx, dy);
			MoveAndReplaceTarget(ud, b, 0, 7, 3, 7);

			ArenaMessageFmt("%s (%s) Moves: Castle queenside",
			    GetPlayerName(ud, c), GetColorText(c));

			return NULL;
		}

	} else {
		return "Error castling";
	}

	return "Could not castle";
}


/*
 * 'c' is the attackers color.
 * kingx and kingy is the target king location.
 */
static
bool
IsCheckmatedBy(BOARD b, COLOR c, int kingx, int kingy)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	COLOR defender = GetOppositeColor(c);

	PIECE *p;
	// for each piece on the board that is defending
	for (int x = 0; x < 8; ++x)
	for (int y = 0; y < 8; ++y) {
		p = GetPieceAt(b, x, y);
		if (p->color != defender) {
			continue;
		}

		// for each tile on the board, see if that piece can move there
		for (int j = 0; j < 8; ++j)
		for (int i = 0; i < 8; ++i) {
			if (PieceCanAttack(b, p->color, p->type, x, y, i, j) == true &&
			    GetPieceAt(b, i, j)->color != p->color) {
				PIECE old_p = *GetPieceAt(b, x, y);
				PIECE old_t = *GetPieceAt(b, i, j);

				// temporarily make the move
				SetPieceAt(b, x, y, COLOR_NONE, TYPE_NONE);
				SetPieceAt(b, i, j, old_p.color, old_p.type);

				// see if the move cancels check
				bool is_mate = true;
				int kx, ky;
				FindKing(b, defender, &kx, &ky);
				if (IsCoordAttackedBy(b, c, kx, ky) == false) {
					is_mate = false;
				}
				
				// revert the pieces
				SetPieceAt(b, x, y, old_p.color, old_p.type);
				SetPieceAt(b, i, j, old_t.color, old_t.type);

				if (is_mate == false) {
					struct timeval tv2;
					gettimeofday(&tv2, NULL);
					LogFmt(OP_MOD, "Checkmate Calculation: %lu.%06lu", tv2.tv_sec - tv.tv_sec, tv2.tv_usec - tv.tv_usec);
					return false;
				}
			}
		}
	}

	struct timeval tv2;
	gettimeofday(&tv2, NULL);

	LogFmt(OP_MOD, "Checkmate Calculation: %lu.%06lu", tv2.tv_sec - tv.tv_sec, tv2.tv_usec - tv.tv_usec);

	return true;
}


// TODO: Handle en passant
static
char*
TryMove(USER_DATA *ud, COLOR c, int x, int y, int dx, int dy)
{
	BOARD b = ud->board;

	PIECE *p = GetPieceAt(ud->board, x, y);
	if (p->type == TYPE_NONE) {
		return "No piece at that position!";
	} else if (p->color != c) {
		return "That piece is not yours!";
	}

	// check if this move is a castle attempt
	// TODO: check for rook movements
	if (p->type == TYPE_KING && IsCastleMove(c, x, y, dx, dy)) {
		if (ColorCanCastle(ud, c) == false) {
			return "You can no longer castle";
		}
		
		return TryCastle(ud, c, x, y, dx, dy);
	}

	// check that the piece can make this movement
	if (PieceCanAttack(ud->board, c, p->type, x, y, dx, dy) == false) {
		return "Piece can't make that movement";
	}

	// target piece
	PIECE *t = GetPieceAt(ud->board, dx, dy);
	if (t->type != TYPE_NONE && t->color == c) {
		return "You already have a piece in the destination position!";
	}

	// make the movements to check for check, but store the data
	// in case the board needs to be reverted
	PIECE old_p = *p;
	PIECE old_t = *t;
	SetPieceAt(b, dx, dy, p->color, p->type);
	SetPieceAt(b, x, y, COLOR_NONE, TYPE_NONE);

	// find the king and test for check
	int kingx, kingy;
	FindKing(b, c, &kingx, &kingy);
	if (IsCoordAttackedBy(b, GetOppositeColor(c), kingx, kingy) == true) {
		// restore saved positions
		SetPieceAt(b, x, y, old_p.color, old_p.type);
		SetPieceAt(b, dx, dy, old_t.color, old_t.type);
		return "That move would place your King in check!";
	}

	//
	// At this point, the move was successful
	//

	// make the pieces blink
	// TODO: this will erroneously leave pieces on if 2 moves
	// are made quickly
	uint32_t objid = LvzGetObjId(old_p.color, old_p.type, dx, dy);
	SetTimer(500, (void*)objid, (void*)0);
	SetTimer(1000, (void*)objid, (void*)1);
	SetTimer(1500, (void*)objid, (void*)0);
	SetTimer(2000, (void*)objid, (void*)1);
	SetTimer(2500, (void*)objid, (void*)0);
	SetTimer(3000, (void*)objid, (void*)1);
	if (old_t.type != TYPE_NONE) {
		// flash the captured piece
		objid = LvzGetObjId(old_t.color, old_t.type, dx, dy);
		SetTimer(500, (void*)objid, (void*)1);
		SetTimer(1000, (void*)objid, (void*)0);
		SetTimer(1500, (void*)objid, (void*)1);
		SetTimer(2000, (void*)objid, (void*)0);
		SetTimer(2500, (void*)objid, (void*)1);
		SetTimer(3000, (void*)objid, (void*)0);
	}

	if (old_p.type == TYPE_KING) {
		// kings cant castle once theyve moved
		if (old_p.color == COLOR_WHITE) {
			ud->white_can_castle = false;
		} else if (old_p.color == COLOR_BLACK) {
			ud->black_can_castle = false;
		}
	}

	// set if a piece has been captured
	bool capture = old_t.type != TYPE_NONE;

	// remove drawing of the old pieces
	LvzActivate(ud, NULL, old_p.color, old_p.type, x, y, false);
	if (capture) {
		LvzActivate(ud, NULL, old_t.color, old_t.type, dx, dy, false);
	}

	// check for pawn promotions
	// TODO: handle underpromotions
	bool promoted = false;
	p = GetPieceAt(b, dx, dy);
	if (p->type == TYPE_PAWN) {
		if (p->color == COLOR_WHITE && dy == 7) {
			p->type = TYPE_QUEEN;
			promoted = true;
		} else if (p->color == COLOR_BLACK && dy == 0) {
			p->type = TYPE_QUEEN;
			promoted = true;
		}
	}

	// draw the new piece
	LvzActivate(ud, NULL, old_p.color, old_p.type, dx, dy, true);
	objid = LvzGetObjId(old_p.color, old_p.type, dx, dy);

	// create move notation
	char move[6];
	move[0] = x + 'a';
	move[1] = y + '1';
	move[2] = ',';
	move[3] = dx + 'a';
	move[4] = dy + '1';
	move[5] = '\0';

	// announce the move
	char line[256];
	snprintf(line, 256, "%s (%s) Moves: %s",
	    GetPlayerName(ud, c), GetColorText(ud->to_move), move);
	if (capture) {
		// add capture line
		char capture[64];
		snprintf(capture, 64, ", capturing a %s!", GetTypeName(old_t.type));
		strlcat(line, capture, 256);
	}
	if (promoted == true) {
		// add promition line
		strlcat(line, " Pawn promoted to Queen!", 256);
	}
	FindKing(b, GetOppositeColor(c), &kingx, &kingy);
	if (IsCoordAttackedBy(b, c, kingx, kingy) == true) {
		if (IsCheckmatedBy(b, c, kingx, kingy) == true) {
			strlcat(line, " Checkmate!", 256);
			ArenaMessage(line);

			char gametime[32];
			TicksToText(gametime, 32, GetTicksMs() - ud->start_tick);
			StopGame(ud, "%s Wins in %s!", GetPlayerName(ud, c), gametime);
		} else {
			strlcat(line, " Check!", 256);
			ArenaMessage(line);
		}
	} else {
		ArenaMessage(line);
	}

	return NULL;
}


void
CmdChessHelp(CORE_DATA *cd)
{
	Reply("This is a Player-vs-Player Chess bot");
	Reply("It is in testing and doesn't support the following:");
	Reply("- En Passant");
	Reply("- Underpromotions");
}


void
CmdGameInfo(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);
	
	if (ud->in_progress == false) {
		Reply("There is no game going on");
		return;
	}

	ReplyFmt("%s (White) vs. %s (Black) is In Progress", ud->white, ud->black);
	char time[32];
	TicksToText(time, 32, GetTicksMs() - ud->start_tick);
	ReplyFmt("The match has been going on for %s", time);
	ReplyFmt("%s (White) to Move", ud->white);
}


void
CmdWhite(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);

	if (IsPlaying(ud, cd->cmd_name) == true) {
		Reply("You are already playing");
	} else if (ud->white[0] != '\0') {
		ReplyFmt("%s is White", ud->white);
	} else {
		strlcpy(ud->white, cd->cmd_name, 24);
		if (ud->black[0]) {
			StartGame(ud);
		} else {
			ArenaMessageFmt("%s is White and awaits a challenger!", ud->white);
		}
	}
}


void
CmdBlack(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);

	if (IsPlaying(ud, cd->cmd_name) == true) {
		Reply("You are already playing");
	} else if (ud->black[0] != '\0') {
		ReplyFmt("%s is Black", ud->black);
	} else {
		strlcpy(ud->black, cd->cmd_name, 24);
		if (ud->white[0]) {
			StartGame(ud);
		} else {
			ArenaMessageFmt("%s is Black and awaits a challenger!", ud->black);
		}
	}
}


void
CmdQuit(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);

	if (IsPlaying(ud, cd->cmd_name)) {
		if (ud->in_progress) {
			StopGame(ud, "%s cedes victory to %s!",
			    cd->cmd_name, GetOpponentName(ud, cd->cmd_name));
		} else {
			UnsetPlayerName(ud, cd->cmd_name);
			ArenaMessageFmt("%s is no longer playing", cd->cmd_name);
		}
	} else {
		Reply("You are not playing");
	}
}


void
CmdMove(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);

	// the game hasnt started
	if (ud->in_progress == false) {
		Reply("The game has not yet begun");
		return;
	}

	// check for correct arg count
	if (cd->cmd_argc != 2) {
		ReplyFmt("Usage: !move Coord,Coord");
		ReplyFmt("Example: !move e2,e4");
		return;
	}

	// make sure the player is playing
	if (IsPlaying(ud, cd->cmd_name) == false) {
		Reply("You are not playing in this match");
		return;
	}

	// replies to players in-game are via arena

	// check whos turn it is
	if (GetColor(ud, cd->cmd_name) != ud->to_move) {
		ArenaMessage("It is not your move");
		return;
	}
				
	// parse argument
	char xstr[3];
	char ystr[3];
	DelimArgs(xstr, 3, cd->cmd_argv[1], 0, ',', false);
	DelimArgs(ystr, 3, cd->cmd_argv[1], 1, ',', false);

	int x1, y1, x2, y2;
	if (ParseCoords(xstr, &x1, &y1) == true &&
	    ParseCoords(ystr, &x2, &y2) == true) {
		
		char *err = TryMove(ud, ud->to_move, x1, y1, x2, y2);
		if (err == NULL && ud->in_progress == true) {
			// successful moves are announced in TryMove()
			ud->to_move = GetOppositeColor(ud->to_move);
			LvzToMove(ud->to_move, NULL);
			ArenaMessageFmt("%s (%s) to Move",
			    GetPlayerName(ud, ud->to_move), GetColorText(ud->to_move));
		} else if (ud->in_progress == true) {
			ArenaMessageFmt("Couldn't move: %s", err);
		}
	} else {
		ArenaMessageFmt("Invalid coordinates");
	}
}


void
GameEvent(CORE_DATA *cd)
{
	USER_DATA *ud = UD(cd);

	switch (cd->event) {
	case EVENT_START:
		// allocate user data
		cd->user_data = xzmalloc(sizeof(USER_DATA));
		ud = UD(cd);

		// allocate chess board
		ud->board = (BOARD)calloc(8, sizeof(PIECE*));
		for (int i = 0; i < 8; ++i) {
			ud->board[i] = (PIECE*)calloc(8, sizeof(PIECE));
		}

		BoardReset(ud, ud->board);

		// draw the board
		LvzDrawAll(ud, NULL, true);

		RegisterCommand("!chesshelp", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    NULL,
		    "Get basic chessbot information",
		    NULL,
		    CmdChessHelp
		    );

		RegisterCommand("!gameinfo", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    NULL,
		    "Get information about the current game",
		    NULL,
		    CmdGameInfo
		    );

		RegisterCommand("!white", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    NULL,
		    "Play as White",
		    NULL,
		    CmdWhite
		    );

		RegisterCommand("!black", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    NULL,
		    "Play as Black",
		    NULL,
		    CmdBlack
		    );

		RegisterCommand("!move", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    "<coord1>,<coord2>",
		    "Move a chess piece",
		    NULL,
		    CmdMove
		    );

		RegisterCommand("!quit", "Chess", 0,
		    CMD_PUBLIC | CMD_PRIVATE,
		    NULL,
		    "Stop playing chess",
		    NULL,
		    CmdQuit
		    );

		break;
	case EVENT_MESSAGE:
#if 0
		// eventually handle commands in pubchat, like "b1,c3"
		if (cd->msg_type == MSG_PUBLIC &&
		    ud->in_progress &&
		    IsPlaying(cd->msg_name)) {
		}
#endif
		break;
	case EVENT_ENTER:
		LvzActivateBoard();
		LvzDrawAll(ud, cd->p1, true);
		LvzToMove(ud->to_move, cd->p1);

		if (IsPlaying(ud, cd->p1->name)) {
			// player rejoined
			ArenaMessageFmt("%s has returned to the game", cd->p1->name);
			KillTimer(ud->timer);
			ud->timer = 0;
		}
		break;
	case EVENT_LEAVE:
		// check if this player is relevant
		if (IsPlaying(ud, cd->p1->name)) {
			if (ud->in_progress && ud->timer) {
				// timer is only set when one player is already gone
				StopGame(ud, "Both players left the game");
				KillTimer(ud->timer);
				ud->timer = 0;
			} else if (ud->in_progress) {
				// someone left a game in progress
				ArenaMessageFmt("%s has left the game and has 2 minutes to return", cd->p1->name);
				ud->timer = SetTimer(2 * 60 * 1000, 0, 0);
			} else {
				// no need to wait for someone not playing
				ArenaMessageFmt("%s is no longer playing", cd->p1->name);
				UnsetPlayerName(ud, cd->p1->name);
			}
		}
		break;
	case EVENT_TIMER:
		// this game needs to be stopped due to timeout
		if (ud->in_progress && cd->timer_id == ud->timer) {
			StopGame(ud, "Player did not return to game");
			ud->timer = 0;
		} else if (cd->timer_data1) {
			uint32_t objid = (uint32_t)cd->timer_data1;
			uint32_t on = (uint32_t)cd->timer_data2;
			if (on) {
				PubMessageFmt("*objon %u", objid);
			} else {
				PubMessageFmt("*objoff %u", objid);
			}
		}
		break;
	case EVENT_STOP:
		// free chess board
		for (int i = 0; i < 8; ++i) {
			free(ud->board[i]);
		}
		free(ud->board);

		// free user data
		free(cd->user_data);
		break;
	}
}

