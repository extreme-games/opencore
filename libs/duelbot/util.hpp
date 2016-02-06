
// XXX: this file sucks

#pragma once

#include <string>
#include <cstring>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <ios>
#include <sstream>
#include <time.h>

#include "opencore.hpp"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define FORHERELIST for(PLAYER *x = cd->here_first; x != NULL; x = x->here_next)
#define FORSHIPLIST for(PLAYER *x = cd->ship_first; x != NULL; x = x->ship_next)

#define ISVALIDPLAYER(p) ((p != NULL) && (p->pid != cd->botpid))

#define MAXFLAGS 256

#define RATIO(a,b) ((double)(a) / ((double)(b) + 1.0))


inline
void
strlcat(char *dst, const char *src, int size)
{
	int len = strlen(dst);
	if (len + 1 < size) {
		strncat(dst, src, size);
		dst[size - 1] = '\0';
	}
}


inline
void
strlcpy(char *dst, const char *src, int size)
{
	strncpy(dst, src, size);
	dst[size - 1] = '\0';
}


// a more random rand()
inline
int
rrand()
{
	return abs((rand() * INT_MAX) + rand());
}


namespace cycad
{
  // structs

  typedef struct AlertStruct_t
  {
    char type[16];
    char name[24];
    char arena[16];
    char msg[256];
  } AlertStruct;

  typedef struct ListModStruct_
  {
    char name[24];
    char arena[16];
    char type[8];
  } ListModStruct;

  // functions

  int lameness (const char *name);

  bool isnumeric (const char *text);	// determine if a string is numeric

  inline bool icasecmp (char c1, char c2)
  {
    return tolower (c1) == tolower (c2);
  }

  bool stringiequals (const std::string & string1,
		      const std::string & string2);

  inline char *strncpyt (char *dest, const char *source, size_t size);

  double ratio (uint16_t wins, uint16_t deaths);
  double ratio (const char *wins, const char *deaths);

  unsigned int playersonfreq (CORE_DATA * cd, uint16_t freq);	// count players on a freq
  unsigned int playersonfreqs (CORE_DATA * cd, uint16_t freq, uint16_t freq2);	// count players on 2 freqs
  unsigned int playersonfreq (CORE_DATA * cd, uint16_t freq, uint16_t shiptype);	// players on a freq in a ship

  unsigned int playersinship (CORE_DATA * cd);	// players in a ship
  unsigned int playersinshiponfreq (CORE_DATA * cd, uint16_t freq);

  PLAYER *matchplayer (CORE_DATA * cd, char *name);	// match player (MATCH_EXACT then MATCH_PREFIX)
  bool isAloneOnFreq (CORE_DATA * cd, PLAYER * p);	// checks if a player is alone on a freq
  PLAYER *matchplayeronfreq (CORE_DATA * cd, uint16_t freq);	// match a player on freq (for " messages)

  void specup (CORE_DATA * cd, PLAYER * p);	// send a player to the spec freq


  bool freqMsg (CORE_DATA * cd, uint16_t freq, const char *message);	// send a freq message
  bool freqMsgFmt (CORE_DATA * cd, uint16_t freq, const char *message, ...);	// send a freq message

  void PrivFreqFmt (CORE_DATA * cd, uint16_t freq, const char *message, ...);
  void PrivFreq (CORE_DATA * cd, uint16_t freq, const char *message);

  void PrivFreqShipsFmt (CORE_DATA * cd, uint16_t freq, const char *message, ...);
  void PrivFreqShips (CORE_DATA * cd, uint16_t freq, const char *message);

  void PrivFreqsShipsFmt (CORE_DATA * cd, uint16_t freq1, uint16_t freq2,
			  const char *message, ...);
  void PrivFreqsShips (CORE_DATA * cd, uint16_t freq1, uint16_t freq2,
		       const char *message);

  bool QueryFmt (CORE_DATA * cd, int query_type, uintptr_t user_data, const char *name, const char *query, ...);	// Query

  unsigned int getcoord (unsigned int loc);	// get coord of world loc

  char alphacoord (unsigned int loc);	// get alpha coord
  // EXAMPLE:  printf("Coords: %c%u", alphacoord(getcoord(x)), getcoord(y));

  //
  // Get a zero-based arg from a string with an arbitrary delimiter
  //
  // Parse args in the form "arg1:arg2:arg3:..."
  //
  // Example:   Get arg 2, no more than 24 chars (including terminating NULL), delimiter ':', out of name:name2:name3
  //
  // delimargs(dest, source, 2, ':', 24);
  //
  // dest will be "name3".
  //
  // Results are always null terminated.
  //
  char *delimargs (char *dest, const char *source, unsigned int arg,
		   const char delim, size_t len, bool getremain = false);


  int atoiarg (const char *source, unsigned int arg, const char delim);

  //
  // Count delimiters in a string
  //
  unsigned int delimcount (const char *source, const char delim);

  //
  // Convert text to an uint32_t value
  //
  uint32_t atoul (const char *text);
  int cyatoi (const char *text);

  //
  // Append type T to string.
  //
  // Example:
  //
  // std::string text = "Float: ";
  //
  // cycad::to_string(text, 3.0f);
  //
  // text is now "Float: 3.0"
  //
  // to_string() uses std::stringstream and is inefficient for lots of conversions.
  //
  template < typename T > void to_string (std::string & result, const T & t);

  //
  // Parse alert into individual peices
  //
  // char type[16];
  // char name[24];
  // char arena[16];
  // char msg[256];
  //
  // ParseAlert() returns TRUE if the message was successfully parsed (and is) an ?alert style message.
  //
  // All set values will be NULL terminated if ParseAlert() returns TRUE.
  //
  // Example:
  //
  // char type[16];
  // char name[24];
  // char arena[16];
  // char msg[256];
  //  if(cycad::ParseAlert(type, name, arena, msg, cd->msg)) {
  //              cd->PubMessageFmt("Type:   %s", type);
  //              cd->PubMessageFmt("Name:   %s", name);
  //              cd->PubMessageFmt("Arena:  %s", arena);
  //              cd->PubMessageFmt("Msg:    %s", msg);
  //      }
  //      else {
  //              cd->PubMessage("Non-alert message ignored.");
  //      }
  //
  bool ParseAlert (char *type, char *name, char *arena, char *msg,
		   const char *source);
  bool ParseAlert (AlertStruct * alertstruct, const char *source);

  bool ParseListMod (ListModStruct * listmodstruct, const char *source);

  //
  // Parse time in the format      1d6h3m
  // 
  // returns total minutes
  //
  // ascii to minutes
  bool is_dhmst (const char *text);
  uint32_t get_multiplier (char dhmst);
  uint32_t TimeParse_dhmsttoticks (const char *text, char dhmst_default =
					't');
  void TimeParse_mtoa (char *dest, size_t dest_size, uint32_t minutes);
  void TimeParse_ttoas (char *dest, size_t dest_size, uint32_t ticks);	// ticks to ascii seconds
  uint64_t TimeParse_dhmstou64s (const char *text, char defaulttype = 'm');

  // constants
  extern const unsigned int MAX_FLAGS;
}

//
// strncpy() wrapper that adds a null terminating string
//
inline char *
cycad::strncpyt (char *dest, const char *source, size_t size)
{
  if (size <= 0)
    return 0;

  dest[size - 1] = '\0';
  return std::strncpy (dest, source, size - 1);
}


template < typename T > void
cycad::to_string (std::string & result, const T & t)
{
  std::ostringstream oss;
  oss << t;
  result += oss.str ();
}
