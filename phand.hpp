/*
 * Copyright (c) 2006 cycad <cycad@zetasquad.com>
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

#ifndef _PHAND_HPP
#define _PHAND_HPP

#include <pthread.h>

#include "core.hpp"

typedef
void (*pkt_handler)(THREAD_DATA*, uint8_t*, int);


/*
 * A null handler that takes no action other than to log packets
 * if the show_unhandled_packets configuration option is set.
 */
void null_handler(THREAD_DATA *td, uint8_t *buf, int len); 

/*
 * Various packet handlers declarations for core and game packets.
 * The identifying byte value that is handles is indicated in the 
 * function name. Function details can be found by their definitions.
 */
void pkt_handle_core_0x02(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x03(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x04(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x05(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x06(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x07(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x0A(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_core_0x0E(THREAD_DATA *td, uint8_t *buf, int len);

void pkt_handle_game_0x02(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x03(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x04(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x06(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x07(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x0A(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x0D(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x19(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x1C(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x1D(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x27(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x28(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x29(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x2E(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x2F(THREAD_DATA *td, uint8_t *buf, int len);
void pkt_handle_game_0x31(THREAD_DATA *td, uint8_t *buf, int len);

#endif

