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

#ifndef _PKT_SEND_HPP
#define _PKT_SEND_HPP

void pkt_send_ack(uint32_t ack_id);
void pkt_send_arena_login(uint8_t ship, char *arena);
void pkt_send_client_key(int32_t client_key);
void pkt_send_disconnect();
void pkt_send_login(char *botname, char *password, 
    uint32_t machine_id, uint8_t newuser, uint32_t permission_id);
void pkt_send_message(THREAD_DATA *td, MSG_TYPE type,
    SOUND sound, PLAYER_ID target_pid, const char *msg);
void pkt_send_position_update(uint16_t x, uint16_t y, uint16_t xy,
    uint16_t yv);
void pkt_send_server_key(uint32_t server_key);
void pkt_send_sync_request();
void pkt_send_sync_response();

#endif

