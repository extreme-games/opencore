/*
 * These functions were ported from twcore.
 */

#ifndef _ENCRYPT_HPP
#define _ENCRYPT_HPP

#include "core.hpp"

void init_encryption(THREAD_DATA *td);

void encrypt_buffer(THREAD_DATA *td, uint8_t *buf, int len);
void decrypt_buffer(THREAD_DATA *td, uint8_t *buf, int len);

#endif

