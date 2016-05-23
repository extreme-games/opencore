
#pragma once

int elo_compute(int elo, int opponent_elo, bool win);
int elo_compute_win(int elo, int opponent_elo);
int elo_compute_loss(int elo, int opponent_elo);

int elo_diff(int elo, int opponent_elo, bool win);
int elo_diff_win(int elo, int opponent_elo);
int elo_diff_loss(int elo, int opponent_elo);

float elo_expected_score(int elo, int opponent_elo);
