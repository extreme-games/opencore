
// XXX: this file sucks

#include <stdarg.h>

#include "util.hpp"

using namespace cycad;


bool cycad::QueryFmt(CORE_DATA * cd, int query_type, uintptr_t user_data, const char *name, const char *query_fmt, ...)
{
	char query[8192];
	
	va_list ap;
	va_start(ap, query_fmt);

	vsnprintf(query, sizeof(query), query_fmt, ap);
	query[sizeof(query) - 1] = '\0';

	va_end(ap);
	
	int res = Query(query_type, user_data, name, query);

	return res >= 0 ? true : false;
}

int
cycad::atoiarg (const char *source, unsigned int arg, const char delim)
{
  unsigned int index = 0;
  unsigned int delims = 0;

  while (source[index])
    {
      if (source[index] == delim)
	{
	  delims++;
	}
      else
	{
	  if (delims == arg)
	    return cycad::cyatoi (source + index);
	}

      index++;
    }

  return 0;
}

int
cycad::cyatoi (const char *text)
{
  if (!text)
    return 0;

  int result = 0;
  unsigned int i = 0;

  unsigned int start = i;
  while (text[i] && !isdigit (text[i]))
    ++i;


  while (text[i] && isdigit (text[i]))
    {
      result = (10 * result) + (text[i] - '0');
      ++i;
    }

  if (text[start] == '-')
    result = result * -1;

  return result;
}


//
// Check if a string is numeric
//
bool
cycad::isnumeric(const char *text)
{				// Determine if a string is numeric
  int offset = 0;
  while (text[offset])
    {
      if (!isdigit (text[offset]))
	return false;

      ++offset;
    }

  return (offset != 0);
}

//
// compare two std strings ignoring case
//
bool
cycad::stringiequals(const std::string & string1,
		      const std::string & string2)
{
  return (string1.size () == string2.size ()) &&
    std::equal (string1.begin (), string1.end (),
		string2.begin (), cycad::icasecmp);
}


//
// Count players on a freq
//
unsigned int
cycad::playersonfreq(CORE_DATA * cd, uint16_t freq)
{
  int count = 0;

  FORHERELIST if (x->pid != cd->bot_pid && x->freq == freq)
    ++count;

  return count;
}

//
// Count players on 2 freqs
//
unsigned int
cycad::playersonfreqs(CORE_DATA * cd, uint16_t freq, uint16_t freq2)
{
  int count = 0;

  FORHERELIST
    if (x->pid != cd->bot_pid && (x->freq == freq || x->freq == freq2))
    ++count;

  return count;
}

//
// Count players on a freq who are in a specific ship
//
unsigned int
cycad::playersonfreq (CORE_DATA * cd, uint16_t freq, uint16_t shiptype)
{
  unsigned int count = 0;

  FORHERELIST
    if (x->pid != cd->bot_pid && x->freq == freq && x->ship == shiptype)
    ++count;

  return count;
}

//
// Count players in ship on a freq
//
unsigned int
cycad::playersinshiponfreq (CORE_DATA * cd, uint16_t freq)
{
  unsigned int count = 0;

  FORSHIPLIST if (x->pid != cd->bot_pid && x->freq == freq)
    ++count;

  return count;
}

//
// Count players in ship
//
unsigned int
cycad::playersinship (CORE_DATA * cd)
{
  unsigned int count = 0;

  FORSHIPLIST++ count;

  return count;
}

//
// Match a player
//
PLAYER*
cycad::matchplayer(CORE_DATA * cd, char *name)
{
  PLAYER *r = FindPlayerName(name, MATCH_HERE);

  if (!r)
    r = FindPlayerName(name, MATCH_PREFIX | MATCH_HERE);

  return r;
}

//
// Checks if a player is alone on a freq
//
bool
cycad::isAloneOnFreq (CORE_DATA * cd, PLAYER * p)
{

  FORHERELIST if (x->freq == p->freq && x->pid != p->pid)
    return false;

  return true;
}

//
// Match a player on freq (for " messages)
//
PLAYER *
cycad::matchplayeronfreq (CORE_DATA * cd, uint16_t freq)
{
  FORHERELIST if (x->freq == freq)
    return x;

  return NULL;
}

//
// Send a player to the spec freq
//
// This assumes the bots freq is the spec freq
void
cycad::specup (CORE_DATA * cd, PLAYER * p)
{
  if (!p)
    return;
  else if (p->ship == SHIP_SPECTATOR && p->freq == cd->bot_freq)
    return;

  if (p->ship == SHIP_SPECTATOR)
    PrivMessage (p, "*setship 3");

  PrivMessage (p, "*spec");
  PrivMessage (p, "*spec");
}

//
// Get coord of world loc
//
unsigned int
cycad::getcoord (unsigned int loc)
{
  return ((loc % 16384) / 16) / 52;
}

//
// Get alphanumeric coord
//
// EXAMPLE:  printf("Coords: %c%u", alphacoord(getcoord(x)), getcoord(y)); //x, y are world coords
//
char
cycad::alphacoord (unsigned int loc)
{
  return "ABCDEFGHIJKLMNOPQRST"[loc % 20];
}



//
// Priv players in ship on a freq
//
void
cycad::PrivFreqShipsFmt (CORE_DATA * cd, uint16_t freq, const char *message, ...)
{
  char buffer[256];
  memset (buffer, 0, 256);

  va_list ap;
  va_start (ap, message);
  vsnprintf (buffer, 250, message, ap);
  va_end (ap);

  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {				//change to a shiplist traversal
      if (p->freq == freq && p->ship != SHIP_SPECTATOR)
	{
	  PrivMessage (p, buffer);
	}
    }
}

void
cycad::PrivFreqShips (CORE_DATA * cd, uint16_t freq, const char *message)
{
  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {				//change to a shiplist traversal
      if (p->freq == freq && p->ship != SHIP_SPECTATOR)
	{
	  PrivMessage (p, (char *) message);
	}
    }
}

void
cycad::PrivFreqsShipsFmt (CORE_DATA * cd, uint16_t freq1, uint16_t freq2,
			  const char *message, ...)
{
  char buffer[256];
  memset (buffer, 0, 256);

  va_list ap;
  va_start (ap, message);
  vsnprintf (buffer, 250, message, ap);
  va_end (ap);

  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {				//change to a shiplist traversal
      if ((p->freq == freq1 || p->freq == freq2) && p->ship != SHIP_SPECTATOR)
	{
	  PrivMessage (p, buffer);
	}
    }
}

void
cycad::PrivFreqsShips (CORE_DATA * cd, uint16_t freq1, uint16_t freq2,
		       const char *message)
{
  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {				//change to a shiplist traversal
      if ((p->freq == freq1 || p->freq == freq2) && p->ship != SHIP_SPECTATOR)
	{
	  PrivMessage (p, (char *) message);
	}
    }
}

//
// Priv a freq
//
void
cycad::PrivFreqFmt (CORE_DATA * cd, uint16_t freq, const char *message, ...)
{
  char buffer[256];
  memset (buffer, 0, 256);

  va_list ap;
  va_start (ap, message);
  vsnprintf (buffer, 255, message, ap);
  va_end (ap);

  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {
      if (p->freq == freq)
	{
	  PrivMessage (p, buffer);
	}
    }
}

void
cycad::PrivFreq (CORE_DATA * cd, uint16_t freq, const char *message)
{
  for (PLAYER * p = cd->here_first; p != NULL; p = p->here_next)
    {
      if (p->freq == freq)
	{
	  PrivMessage (p, (char *) message);
	}
    }
}

//
// Get a zero-based arg from a string with an arbitrary delimiter
//
// Example:   Get arg 2, no more than 24 chars, delimiter ':', out of name:name2:name3
//
// delimargs(dest, source, 2, ':', 24);
//
char *
cycad::delimargs (char *dest, const char *source, unsigned int arg,
		  const char delim, size_t len, bool getremain)
{
  unsigned int delimcount = 0;
  unsigned int destindex = 0;
  unsigned int sourceindex = 0;

  while ((source[sourceindex]) && ((delimcount <= arg) || getremain))
    {
      if ((source[sourceindex] == delim)
	  && (!getremain || (delimcount < arg)))
	{
	  ++delimcount;
	}
      else if (delimcount == arg)
	{
	  dest[destindex] = source[sourceindex];

	  if (destindex + 1 >= len)
	    break;

	  ++destindex;
	}

      ++sourceindex;
    }

  dest[destindex] = '\0';
  return dest;
}


//
// Count delimiters in a string
//

unsigned int
cycad::delimcount (const char *source, const char delim)
{
  unsigned int index = 0;
  unsigned int result = 0;

  while (source[index])
    {
      if (source[index++] == delim)
	++result;
    }

  return result;
}

//
// Convert a string to an uint32_t
//
uint32_t
cycad::atoul (const char *text)
{
  if (!text)
    return 0;

  uint32_t result = 0;
  unsigned int i = 0;

  while (text[i] && !isdigit (text[i]))
    ++i;

  while (text[i] && isdigit (text[i]))
    {
      result = (10 * result) + (text[i] - '0');
      ++i;
    }

  return result;
}

//
// ParseAlert()
//
// type[16]
// name[24];
// arena[16];
// msg[256];
bool
cycad::ParseAlert (char *type, char *name, char *arena, char *msg,
		   const char *source)
{
  const char *base = source;
  int endoffset = 0;

  int i = 0;
  while (source[i] && (source[i] != ':'))
    ++i;

  if (i == 0)
    return false;

  if (i > 15)
    return false;
  else if (source[i] != ':')
    return false;

  strncpy (type, source, i);
  type[i] = '\0';

  if (source[i + 1] != ' ')
    return false;
  else if (source[i + 2] != '(')
    return false;
  else if (!source[i + 3])
    return false;

  base = source + i + 3;

  i = 0;
  while (base[i] && (base[i] != ':'))
    ++i;

  if (base[i] != ':')
    return false;
  else if (base[i - 1] != ')')
    return false;
  else if (base[i + 1] != ' ')
    return false;
  else if (base[i + 2] == '\0')
    return false;

  strncpy (msg, base + i + 2, 255);
  msg[255] = '\0';

  endoffset = i - 1;

  char *separator = strstr ((char *) source, ") (");

  if (!separator)
    return false;
  else if (separator > (base + endoffset))
    return false;

  char *newseparator;

  while ((newseparator = strstr (separator + 3, ") (")) != NULL)
    {
      if (newseparator > (base + endoffset))
	break;

      separator = newseparator;
    }

  if ((separator + 4)[0] == '\0' || (separator + 4)[0] == ')')
    return false;


  memset (arena, 0, 16);
  strncpy (arena, separator + 3, 16);

  if (arena[0] == '\0')
    return false;

  bool found = false;

  for (i = 0; (!found) && (i < 16); ++i)
    {
      if (arena[i] == ')')
	{
	  arena[i] = '\0';
	  found = true;
	}
    }

  if (!found)
    return false;

  if (separator - base == 0)
    return false;
  else if (separator - base > 24)
    return false;

  strncpy (name, base, separator - base);

  name[separator - base] = '\0';

  return true;
}

bool
cycad::ParseAlert (cycad::AlertStruct * alertstruct, const char *source)
{
  return cycad::ParseAlert (alertstruct->type, alertstruct->name,
			    alertstruct->arena, alertstruct->msg, source);
}


bool
cycad::ParseListMod (ListModStruct * listmodstruct, const char *source)
{
  char result[256];
  strncpy (result, source, 255);
  result[255] = '\0';

  char *lastsep = NULL;
  char *nextlastsep = NULL;
  char *temp = result;

  // find the last and 2nd to last " - "
  while ((temp = strstr (temp, " - ")) != NULL)
    {
      nextlastsep = lastsep;
      lastsep = temp;

      temp += 3;
    }

  if (!lastsep || !nextlastsep)	// if there werent two " - " strings, exit
    return false;

  lastsep += 3;
  nextlastsep += 3;

  if (lastsep[0] == '\0' || strlen (lastsep) > 15)
    return false;

  strcpy (listmodstruct->arena, lastsep);

  if (strncmp (nextlastsep, "SMod", 4) == 0)
    {
      if (nextlastsep + 4 != lastsep - 3)
	return false;

      strcpy (listmodstruct->type, "SMod");
    }
  else if (strncmp (nextlastsep, "Mod", 3) == 0)
    {
      if (nextlastsep + 3 != lastsep - 3)
	return false;

      strcpy (listmodstruct->type, "Mod");
    }
  else if (strncmp (nextlastsep, "Sysop", 5) == 0)
    {
      if (nextlastsep + 5 != lastsep - 3)
	return false;

      strcpy (listmodstruct->type, "Sysop");
    }
  else
    {
      return false;
    }

  nextlastsep -= 3;
  nextlastsep[0] = '\0';

  size_t namelen = strlen (result);

  if (namelen == 0 || namelen > 23)
    return false;

  strcpy (listmodstruct->name, result);

  return true;
}

bool
cycad::is_dhmst (const char *text)
{
  int offset = 0;

  while (text[offset])
    {
      switch (text[offset])
	{
	case 't':
	case 'T':
	case 's':
	case 'S':
	case 'm':
	case 'M':
	case 'h':
	case 'H':
	case 'd':
	case 'D':
	  return true;
	}

      ++offset;
    }

  return false;
}

uint32_t
cycad::get_multiplier (char dhmst)
{
  switch (dhmst)
    {
    case 't':
    case 'T':
      return 1;
    case 's':
    case 'S':
      return 1000;
    case 'm':
    case 'M':
      return 1000 * 60;
    case 'h':
    case 'H':
      return 1000 * 60 * 60;
    case 'd':
    case 'D':
      return 1000 * 60 * 60 * 24;
    default:
      return 0;
    }
}


uint32_t
cycad::TimeParse_dhmsttoticks (const char *text, char dhmst_default)
{
  uint32_t result = 0;
  uint32_t value = 0;
  uint32_t valueindex = 0;
  int multiplier = 0;
  int offset = 0;

  while (true)
    {

      switch (text[offset])
	{
	case '\0':
	case 't':
	case 'T':
	  multiplier = 1;
	  break;
	case 's':
	case 'S':
	  multiplier = 1000;
	  break;
	case 'm':
	case 'M':
	  multiplier = 1000 * 60;
	  break;
	case 'h':
	case 'H':
	  multiplier = 1000 * 60 * 60;
	  break;
	case 'd':
	case 'D':
	  multiplier = 1000 * 60 * 60 * 24;
	  break;
	default:
	  multiplier = 0;
	  break;
	}

      if (text[offset] >= '0' && text[offset] <= '9')
	{
	  value = (10 * value) + (text[valueindex] - '0');
	  ++valueindex;
	}
      else if (multiplier)
	{
	  result += value * multiplier;
	  valueindex = 0;
	  value = 0;
	}
      else
	{
	  valueindex = 0;
	  value = 0;
	}

      if (text[offset] == '\0')
	return result;

      ++offset;
    }
}

uint64_t
cycad::TimeParse_dhmstou64s (const char *text, char defaulttype)
{
  uint64_t result = 0;
  uint64_t value = 0;
  uint64_t valueindex = 0;
  uint64_t multiplier = 0;
  int offset = 0;

  while (true)
    {

      char curchar = text[offset];
      if (!curchar)
	curchar = defaulttype;


      switch (curchar)
	{
	case 's':
	case 'S':
	  multiplier = 1;
	  break;
	case 'm':
	case 'M':
	  multiplier = 1 * 60;
	  break;
	case 'h':
	case 'H':
	  multiplier = 1 * 60 * 60;
	  break;
	case 'd':
	case 'D':
	  multiplier = 1 * 60 * 60 * 24;
	  break;
	case 'y':
	case 'Y':
	  multiplier = 1 * 60 * 60 * 24 * 365;
	  break;
	default:
	  multiplier = 0;
	  break;
	}

      if (text[offset] >= '0' && text[offset] <= '9')
	{
	  value = (10 * value) + (text[valueindex] - '0');
	  ++valueindex;
	}
      else if (multiplier)
	{
	  result += value * multiplier;
	  valueindex = 0;
	  value = 0;
	}
      else
	{
	  valueindex = 0;
	  value = 0;
	}

      if (text[offset] == '\0')
	return result;

      ++offset;
    }
}

void
cycad::TimeParse_mtoa (char *dest, size_t dest_size, uint32_t minutes)
{
  uint32_t m = minutes % 60;
  uint32_t h = (minutes / 60) % 60;
  uint32_t d = ((minutes / 60) / 60) % 24;

  dest[0] = '\0';

  if (d) {
      snprintf (dest, dest_size, "%ud, ", d);
    }

  size_t len = 0;
  if (h)
    {
      len = strlen (dest);
      snprintf (dest + len, dest_size - len, "%uh, ", h);
    }

  len = strlen (dest);
  snprintf (dest + len, dest_size - len, "%s%um", (len == 0) ? "" : "and ",
	     m);

  dest[dest_size - 1] = '\0';
}


void
cycad::TimeParse_ttoas (char *dest, size_t dest_size, uint32_t ticks)
{
  ticks = ticks / 1000;
  uint32_t s = ticks % 60;

  uint32_t minutes = ticks / 60;
  uint32_t m = minutes % 60;
  uint32_t h = (minutes / 60) % 24;
  uint32_t d = ((minutes / 60) / 24);

  dest[0] = '\0';

  if (d)
    {
      snprintf (dest, dest_size, "%ud, ", d);
    }

  size_t len = 0;
  if (h)
    {
      len = strlen (dest);
      snprintf (dest + len, dest_size - len, "%uh, ", h);
    }

  if (m)
    {
      len = strlen (dest);
      snprintf (dest + len, dest_size - len, "%um ", m);
    }

  len = strlen (dest);
  snprintf (dest + len, dest_size - len, "%s%us", (len == 0) ? "" : "and ",
	     s);

  dest[dest_size - 1] = '\0';
}


/* EOF */
