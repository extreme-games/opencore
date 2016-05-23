
#include <math.h>

#include "elo.hpp"

#define K_VALUE (20.0f)

#include <stdio.h>



float
elo_expected_score(int elo, int opponent_elo)
{
	return 1.0f / (1.0f + pow(10.0f, ((float)(opponent_elo - elo)) / 400.0f));
}

int
elo_compute(int elo, int opponent_elo, bool win)
{
	return elo + elo_diff(elo, opponent_elo, win);
}

int
elo_compute_win(int elo, int opponent_elo)
{
	return elo_compute(elo, opponent_elo, true);
}


int
elo_compute_loss(int elo, int opponent_elo)
{
	return elo_compute(elo, opponent_elo, false);
}


int elo_diff(int elo, int opponent_elo, bool win)
{
	return (int)((K_VALUE * ((win ? 1.0f : 0.0f) - elo_expected_score(elo, opponent_elo))) +
		(win ? 0.5f : -0.5f)); // for rounding properly
}


int elo_diff_win(int elo, int opponent_elo)
{
	return elo_diff(elo, opponent_elo, true);
}


int elo_diff_loss(int elo, int opponent_elo)
{
	return elo_diff(elo, opponent_elo, false);
}
