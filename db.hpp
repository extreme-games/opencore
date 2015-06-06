/* Copyright 2015 cycad <cycad@greencams.net>. All rights reserved. */

#ifndef __DB_HPP
#define __DB_HPP 1

#include "util.hpp"

void db_init();
void db_shutdown();

void db_instance_init();
void db_instance_shutdown();
void db_instance_export_events(ticks_ms_t max_time);

#endif

