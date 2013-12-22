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

#ifndef _PKT_HPP
#define _PKT_HPP

#include <list>
#include <string.h>

#include "util.hpp"

#define DIR_INCOMING "Read"	/* Specify 'direction' for spew_packet() */
#define DIR_OUTGOING "Send"

/*
 * This is a type of packet used for immediate transmission and
 * deallocation. For packets that need to be stored for future
 * use (retransmission), use the RPACKET type.
 *
 * PACKETs should never be allocated directly and should instead
 * use the allocate_packet() function.
 */
typedef
struct PACKET_ {
	int	len;		/* length of data[] */
	uint8_t	data[0];	/* packet data */
} PACKET;

/*
 * Contains a reliable packet. It has space for a tick count and an
 * acknowledgement number which are used for sending and recieving
 * packets reliably.
 */
typedef
struct RPACKET_ {
	int		len;		/* length of data[] */
	ticks_ms_t	ticks;		/* timestamp (ticks received/sent) */
	uint32_t	ack_id;		/* ack id (received/sent) */
	uint8_t		data[0];	/* packet data */
} RPACKET;

/*
 * Lists of packets, used as packet queues.
 */
typedef std::list<PACKET*> packet_list_t;
typedef std::list<RPACKET*> rpacket_list_t;

typedef struct DOWNLOAD_ENTRY_ DOWNLOAD_ENTRY;
struct DOWNLOAD_ENTRY_ {
	char name[16];
	char initiator[24];
};
typedef std::list<DOWNLOAD_ENTRY> file_list_t;

/*
 * Packet chunks.
 */
typedef
struct CHUNK_ {
	int		len;
	uint8_t		data[0];	/* chunk data */
} CHUNK;
typedef std::list<CHUNK*> chunk_list_t;


typedef
struct UPLOAD_DATA_ {
	char filename[64];
	char initiator[24];
} UPLOAD_DATA;
typedef std::list<UPLOAD_DATA*> upload_list_t;


/*
 * Write a packet to the console. 'direction' is one of DIR_xxx. This shows
 * application-layer data only.
 */
void spew_packet(uint8_t *buf, int len, char *direction);

/*
 * Allocates a PACKET on the heap with a data[] length of 'len'.
 * PACKET.len is set to 'len' and data[] is zeroed.
 * 
 * PACKET*s must later be freed with free_packet().
 *
 * Note that 'len' is NOT the total amount of allocated space, merely
 * the length of the data[] array.
 */
inline
PACKET*
allocate_packet(int len)
{
	int total_len = sizeof(PACKET) + len;
	PACKET* p = (PACKET*)malloc(total_len);
	bzero(p, total_len);
	p->len = len;
	return p;
}

/*
 * This function frees a PACKET* that was allocated with allocate_packet().
 */ 
inline
void
free_packet(PACKET *p)
{
	free(p);
}

inline
CHUNK*
allocate_chunk(int len)
{
	int total_len = sizeof(CHUNK) + len;
	CHUNK *c = (CHUNK*)malloc(total_len);
	bzero(c, total_len);
	c->len = len;
	return c;
}

inline
void
free_chunk(CHUNK *c)
{
	free(c);
}

/*
 * Allocates a RPACKET on the heap with data[] of length 'len'. The tick count
 * and ack_id in the RPACKET are set to the values passed to the function.
 * 
 * RPACKET*s must later be freed with free_rpacket().
 *
 * Note that 'len' is not the total amount allocated, only the size of data[].
 */
inline
RPACKET*
allocate_rpacket(int len, ticks_ms_t ticks, uint32_t ack_id)
{
	int total_len = sizeof(RPACKET) + len;
	RPACKET *rp = (RPACKET*)malloc(total_len);
	bzero(rp, total_len);

	rp->ticks = ticks;
	rp->ack_id = ack_id;
	rp->len = len;

	return rp;
}

/*
 * Frees an RPACKET* that was allocated with allocate_rpacket().
 */ 
inline
void
free_rpacket(RPACKET *rp)
{
	free(rp);
}

/*
 * Functions for creating and extracting packets using buffers and
 * format strings.
 */
int build_packet(uint8_t *buf, char *fmt, ...);
int extract_packet(uint8_t *buf, char *fmt, ...);

#endif

