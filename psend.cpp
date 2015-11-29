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

#include <sys/param.h>

#include "phand.hpp"

#include "util.hpp"

/*
 * Send an ack reply to the server.
 */
void
pkt_send_ack(uint32_t ack_id)
{
	PACKET *p = allocate_packet(6);

	build_packet(p->data, "AAC",
	    0x00,
	    0x04,
	    ack_id
	    );

	queue_packet(p, SP_HIGH);
}

/*
 * Send an arena change/enter request. This sends a bot to the named arena,
 * joining it with a ship type of 'ship'. To send a bot to a random pub, specify
 * a non-existent arena name ("99").
 */
void
pkt_send_arena_login(uint8_t ship, char *arena)
{
	char arena_[16];
	bzero(arena_, 16);
	strlcpy(arena_, arena, 16);

	/* 0xFFFD for named sub, or actual number for pub */
	uint16_t join_type = 0xFFFD;
	if (IsPub(arena))
		join_type = (uint16_t)atoi(arena);

	PACKET *p = allocate_packet(26);
	build_packet(p->data, "AABBBBz",
	    0x01,
	    ship,	/* ship type */
	    0,
	    4096,	/* resolution */
	    4096,	/* resolution */
	    join_type,
	    arena_, 16	/* arena name */
	    );

	queue_packet_reliable(p, SP_HIGH);
}

/*
 * Send a client key to the server. This is used to initiate a connection.
 */
void
pkt_send_client_key(int32_t client_key)
{
	PACKET *p = allocate_packet(8);
	build_packet(p->data, "AAcB",
	    0x00,
	    0x01,
	    client_key,
	    0x01
	    );

	queue_packet(p, SP_HIGH);
}

/*
 * Send a disconnect packet to the server.
 */
void
pkt_send_disconnect()
{
	PACKET *p = allocate_packet(2);

	build_packet(p->data, "AA",
	    0x00,
	    0x07
	    );

	/* hah! if this is SP_HIGH, when a bot is stopping
	   the disconnect gets sent first before other queues
	   are cleared, this causes the server to process
	   the disconnect before the message packets are sent
	   */
	queue_packet(p, SP_NORMAL);
}

void
pkt_send_namereg(char *regname, char *regemail)
{
	PACKET *p = allocate_packet(766);

	char name[32] = { 0 };
	char username[40] = { 0 };
	char email[64] = { 0 };
	char city[32] = { 0 };
	char state[24] = { 0 };

	strncpy(name, regname, sizeof(name)-1);
	strncpy(email, regemail, sizeof(email)-1);
	strncpy(username, regname, sizeof(username)-1);
	strncpy(city, "internet", sizeof(city)-1);
	strncpy(state, "internet", sizeof(state)-1);

	char block_data[14][40];
	memset(block_data, 0, sizeof(block_data));
	for (size_t i = 0; i < 14; ++i) {
		strncpy(block_data[i], "OpenCore", 40);
	}

	build_packet(p->data, "AZZZZAAAAACBBZZ",
	    0x17,
	    name, 32,
		email, 64,
		city, 32,
		state, 24,
		(rand() > RAND_MAX/2) ? 'M' : 'F',
		69,
		1,
		1,
		1,
		586,
		0xC000,
		2036,
		username, 40,
		block_data, 14 * 40
	    );

	queue_packet_large(p, SP_HIGH);
}

/*
 * Send a login packet to the server.
 *
 * TODO: Disable machine_id parameter and use something that
 * comes from the machine directly, instead of the user.
 */
void
pkt_send_login(char *botname, char *password, 
    uint32_t machine_id, uint8_t newuser, uint32_t permission_id)
{
	char botname_[32];
	char password_[32];

	strlcpy(botname_, botname, 32);
	strlcpy(password_, password, 32);

	PACKET *p = allocate_packet(101);
	build_packet(p->data, "AAzzCAbBbCCCCCC",
	    0x09,		/* packet type */
	    newuser,		/* new user (boolean) */
	    botname_, 32,	/* username, length */
	    password_, 32,	/* password, length */
	    machine_id,		/* machine id */
	    0x00,		/* magic number */
	    0,
	    0x6f9d,		/* magic number */
	    0x0086,		/* client version */
	    444,		/* magic number */
	    555,		/* magic number */
	    permission_id,	/* permission id */
	    0,			/* padding */
	    0,			/* padding */
	    0			/* padding */
	    );

	queue_packet_reliable(p, SP_HIGH);
}

/*
 * Send a message packet. 'msg' is automatically truncated.
 */
void
pkt_send_message(THREAD_DATA *td, MSG_TYPE type,
    SOUND sound, PLAYER_ID target_pid, const char *msg)
{
	int msg_len = strlen(msg);
	msg_len = MIN(msg_len, 243); // XXX should be 247 but long packets can get cluster or stream headers added on or something that adds 4 bytes, queue_packet() should be fixed

	/* header + msg len + terminating null */
	PACKET *p = allocate_packet(5 + msg_len + 1);
	build_packet(p->data, "AAABza",
	    0x06,
	    type,		/* message type, MSG_xxx */
	    sound,		/* sound type */
	    target_pid,		/* target pid */
	    msg, msg_len,	/* message and length */
	    (char)'\0');	/* terminating null */

	queue_packet_reliable(p, SP_NORMAL);
}

/*
 * Send a position update to the server.
 */
void
pkt_send_position_update(uint16_t x, uint16_t y, uint16_t xv, uint16_t yv)
{
	PACKET *p = allocate_packet(22);	

	build_packet(p->data, "AACBBAABBBBB",
	    0x03,		/* type */
	    0x00,		/* orientation */
	    get_ticks_ms(),	/* tick count */
	    xv,			/* x velocity */
	    y,			/* y pixels */
	    0,			/* checksum */
	    0,			/* status */
	    x,			/* x pixels */
	    yv,			/* y velocity */
	    0,			/* bounty */
	    0,			/* energy */
	    0			/* weapon info */
	    );

	uint8_t checksum = 0;
	for (int i = 0; i < 22; ++i)
		checksum ^= p->data[i];
	p->data[10] = checksum;

	queue_packet(p, SP_NORMAL);
}

/*
 * Send a sync request to the server.
 */
void
pkt_send_sync_request()
{
	THREAD_DATA *td = get_thread_data();

	PACKET *p = allocate_packet(14);
	build_packet(p->data, "AAcCC", 
	    0x00,
	    0x05,
	    get_ticks_hs(), /* local tick count, time */
	    td->net->stats->packets_sent,
	    td->net->stats->packets_read
	    );
		
	queue_packet_reliable(p, SP_HIGH);
}

/*
 * Send a sync response to the server. Tick counts are in hundreths of seconds.
 */
void
pkt_send_sync_response()
{
	THREAD_DATA::net_t *n = get_thread_data()->net;
	PACKET *p = allocate_packet(10);
	
	ticks_ms_t cur_ticks = get_ticks_ms();
	build_packet(p->data, "AACC",
	    0x00,
	    0x06, 
	    cur_ticks/10, 
	    (cur_ticks - n->ticks->last_sync_received)/10
	    );

	queue_packet_reliable(p, SP_HIGH);
}

/*
 * Send the server key back to the server. This is part of the initial
 * connection initiation handshake.
 */
void
pkt_send_server_key(uint32_t server_key)
{
	PACKET *p = allocate_packet(10);
	
	build_packet(p->data, "AACC",
	    0x00,
	    0x06, 
	    server_key, 
	    get_ticks_hs() /* local tick count, ss time */
	    );

	queue_packet(p, SP_HIGH);
}

/*
 * Send a frequency change packet to the server.
 */
void
pkt_send_freq_change(uint16_t freq)
{
	PACKET *p = allocate_packet(3);
	build_packet(p->data, "AB",
	    0x0F,
	    freq
	    );

	queue_packet_reliable(p, SP_NORMAL);
}

/*
 * Send a ship change packet to the server.
 */
void
pkt_send_ship_change(uint8_t ship)
{
	PACKET *p = allocate_packet(2);
	build_packet(p->data, "AA",
	    0x18,
	    ship
	    );

	queue_packet_reliable(p, SP_NORMAL);
}

/*
 * Send a flag pickup request packet to the server.
 */
void
pkt_send_flag_pickup_request(uint16_t flag_id)
{
	PACKET *p = allocate_packet(3);
	build_packet(p->data, "AB",
	    0x18,
	    flag_id
	    );

	queue_packet_reliable(p, SP_NORMAL);
}
