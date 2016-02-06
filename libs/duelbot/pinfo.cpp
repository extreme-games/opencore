
#include "pinfo.hpp"

void pi_clear(CORE_DATA *cd, PLAYER *p) {
	uint32_t time = GetTicksMs();

	pi(cd, p)->allowed_to_change_freq = false;
	pi(cd, p)->nfdw = false;
	pi(cd, p)->gotsafed = false;

	pi(cd, p)->lastwarptick = time - 30000;
	pi(cd, p)->lastchallengetick = time - 30000;
	pi(cd, p)->lastantitakentick = time - 30000;

	// leave this alone so people can switch arenas
	// without reenabling it
	// pi(cd, p)->noduel_enabled	= false;
}
