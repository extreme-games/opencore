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

#ifndef _BOT_HPP
#define _BOT_HPP

#include <pthread.h>

#include <sys/types.h>
#include <sys/queue.h>

#include <netinet/in.h> /* for sockaddr_in */
#include <poll.h>

#include "core.hpp"

#include "parsers.hpp"
#include "pkt.hpp"


/*
 * These are the various network states used by the core to determine how
 * packets are handled. For example, a key request is ignored by the core
 * if a connection is already established.
 */
enum NS_TYPE {
	NS_CONNECTING = 1,
	NS_KEYEXCHANGE = 2,
	NS_LOGGINGIN = 3,
	NS_CONNECTED = 4,

	NS_DISCONNECTED = 5
};

/*
 * Packet sending priorities, used with queue_packet() and
 * queue_packet_reliable().
 */
#define SP_DEFERRED	1
#define SP_NORMAL	2
#define SP_HIGH		3

#define MAX_CHATS	10	/* the maximum number of chats the bot can join */
#define MAX_PACKET	512	/* the maximum size of a packet, including headers */

#define CMD_CHAR	('!')	/* commands sent to the bot must begin with this */

/*
 * Used by each bot to hold its bot-specific data.  This struct contains many other
 * structures. Their descriptions can be found by their declarations.
 */
typedef
struct THREAD_DATA_
{
	int		 running;	/* if this is less than 0, the bot will exit */

	void		*lib_entry;	/*
					 * The current library that controls execution,
					 * NULL if execution is controlled by the core 
					 */

	void		*botman_handle;	/* handle to the bot's bot manager data */

	int		 in_arena;	/* set when the bot is in an arena */
	unsigned int	 rand_seed;	/* seed for rand_r() calls */

	PLAYER_ID	 bot_pid;	/* the bot's pid */
	char		 bot_name[24];	/* the bot's name */
	char		 bot_email[64];	/* the bot's email */
	char		 bot_type[16];	/* the bot's type */
	SHIP		 bot_ship;	/* the bot's ship */
	FREQ		 bot_freq;	/* the bot's freq */
	POINT		 bot_pos[1];	/* the bots x/y position */
	struct {
		uint16_t	x;
		uint16_t	y;
	} bot_vel[1];

	char	libstring[512];		/* ' '-delimited string of libraries to load */
	char	arena_change_request[16];	/* name of the arena requested */

	/*
	 * Contains information that pertains to the logging in process.
	 */
	struct login_t {
		char	password[32];	/* the password the bot will log in with */
		char	arenaname[16];	/* name of the arena this bot is being sent to */
		char	autorun[256];	/* commands automatically run when the bot logs on */
		char	chats[256];	/* comma-delimited list of chats joined during login */
	} login[1];

	/*
	 * Arena-specific information
	 */
	struct arena_t {
		char name[16];	/* the name of the arena the bot is in */

		struct ticks_t {
			ticks_ms_t last_player_flush;
		} ticks[1];

		PLAYER	*here_first;
		PLAYER	*here_last;
		int	 here_count;
		PLAYER	*ship_first;
		PLAYER	*ship_last;
		int	 ship_count;
		PLAYER	*spec_first;
		PLAYER	*spec_last;
		int	 spec_count;
	} arena[1];

	/*
	 * Container for recording which chat the bot joined during login.
	 * This is used by chat_message() to determine which chat name
	 * corresponds to which chat number
	 */
	struct chat_t {
		char	chat[MAX_CHATS][16];	/* array of chats the bot has joined */
	} chats[1];

	/*
	 * Upon entering an arena, these booleans determine which messages are sent
	 * to players.
	 */
	struct enter_t
	{
		uint8_t	send_watch_damage;	/* send *watchdamage */
		uint8_t	send_info;		/* send *info */
		uint8_t	send_einfo;		/* send *einfo */
	} enter[1];

	/*
	 * Pointers to module (not library) specific data.
	 */
	struct mod_data_t
	{
		void	*cmd;		/* cmd.cpp */
		void	*config;	/* config.cpp */
		void	*lib;		/* lib.cpp */
		void	*player;	/* player.cpp */
		void	*timer;		/* timer.cpp */
	} mod_data[1];

	/*
	 * Contains all network-related configuration options, settings, and other
	 * data.
	 */
	struct net_t
	{
		struct pollfd	pfd[1];	/* for poll() */

		int		fd;	/* network file descriptor */
		int		state;	/* network state */

		struct sockaddr_in	serv_sin;	/* sockaddr struct for the server */

		char	servername[128];		/* server name to connect to */
		char	serverport[8];			/* servers port number (sanitized) */
		char	serverip[INET_ADDRSTRLEN];	/* servers ip address (sanitized) */

		/*
		 * Contains all temporal data, including intervals.
		 */
		struct ticks_t
		{
			ticks_ms_t	last_connection_request;	/* tick of last sent connection request */
			ticks_ms_t	last_deferred_flush;		/* tick of last deferred flush */
			ticks_ms_t	last_pkt_received;		/* tick of last packet received */
			ticks_ms_t	last_sync_received;		/* last sync received */
			ticks_ms_t	last_pos_update_sent;		/* tick of last pos update sent */
			ticks_ms_t	disconnected;			/* tick of when the client was disconnected */
		} ticks[1];

		/*
		 * Queues for high, normal, and deferred priority packets. High and normal
		 * priority packets are both sent at every loop iteration, but high priority
		 * packets are sent first. Deferred priority packets are sent every
		 * ticks->deferred_flush_interval milliseconds.
		 */
		struct queues_t
		{
			packet_list_t	*h_prio;	/* high priority */
			packet_list_t	*n_prio;	/* normal priority */
			packet_list_t	*d_prio;	/* deferred priority */
		} queues[1];

		/*
		 * Contains data related to encryption.
		 */
		struct encrypt_t
		{
			int32_t	client_key;	/* the client key */
			int32_t	server_key;	/* the server key */
			uint8_t	table[520];	/* table used for encryption */
			uint8_t	use_encryption;	/* set to non-zero to encrypt/decrypt packets */
		} encrypt[1];

		/*
		 * Variables related to reliable outgoing packets.
		 */
		struct rel_sent_t
		{
			rpacket_list_t	*queue;		/* queue of reliable outgoing packets */
			uint32_t	 next_ack_id;	/* next outgoing ack */
		} rel_o[1];

		/*
		 * Variables related to reliable incoming packets.
		 */
		struct rel_read_t
		{
			rpacket_list_t	*queue;		/* queue of reliable incoming packets */

			uint32_t	 next_ack_id;	/* next expected ack */
		} rel_i[1];

		/*
		 * Network statistics.
		 */
		struct stats_t
		{
			uint32_t	packets_sent;	/* packets received */
			uint32_t	packets_read;	/* packets received */
		} stats[1];

		struct stream_t
		{
			uint8_t data[1024 * 1024];
			uint8_t offset;
		} stream[1];

		/*
		 * Inbound stream data.
		 */
		struct chunk_in_t
		{
			int		 in_use;	/* set to non-zero if a chunk is pending */
			char		 filename[16];	/* destination file name */
			uint8_t		 content_type;	/* the type of transfer */
			uint32_t	 content_size;	/* total size expected */
			uint32_t	 read_size;	/* size of data read */
			chunk_list_t	*queue;		/* list of received chunks */
			file_list_t	*file_list;	/* file list of pending retrievals */
			char		 initiator[24];	/* name of player who initiated the transfer */
		} chunk_i[1];

		/* 
		 * File upload (*putfile).
		 */
		struct send_file_data_t
		{
			int		in_use;
			upload_list_t	*upload_list;
			char		cur_filename[64];
			char		cur_initiator[24];
		} send_file_data[1];

	} net[1];

	struct {
		int	info_line;	/* current line of parsing expected next */
		info_t	info;		/* struct of data for the info parser */
	} parser[1];

	struct {
		ticks_ms_t	info;		/* how often in ticks *einfo is sent */
		ticks_ms_t	last_info;	/* last *info */
		ticks_ms_t	einfo;		/* how often in ticks *info is sent */
		ticks_ms_t	last_einfo;	/* last *einfo */
	} periodic[1];

	/*
	 * Debugging information/controls.
	 */
	struct debug_t
	{
		int	spew_packets;		/* set to 1 to spew packets */
		int 	show_unhandled_packets;	/* set to 1 to show packets with no handler */
	} debug[1];

} THREAD_DATA;

/*
 * Global list of process ids that exist, and its mutex.
 */
typedef std::list<pthread_t>	pid_list_t; 
extern pid_list_t		g_pid_list;
extern pthread_mutex_t		g_pid_list_mutex;

/*
 * Permission ID / MachineID
 */
extern uint32_t			g_permissionid;
extern uint32_t			g_machineid;

/*
 * Global key used for TLS.
 */
extern pthread_key_t g_tls_key;

/*
 * Retrieve a thread's specific data from thread local storage.
 */
inline
THREAD_DATA*
get_thread_data()
{
	return (THREAD_DATA*)pthread_getspecific(g_tls_key);
}

/*
 * Entry point for the bot.
 */
void*	BotEntryPoint(void*);

void	process_incoming_packet(THREAD_DATA *td, uint8_t *buf, int len);

/*
 * Schedule a packet for queueing.
 */
void	queue_packet(PACKET *p, int priority);
void	queue_packet_reliable(PACKET *p, int priority);
void	queue_packet_large(PACKET *p, int priority); /* large packets are always sent reliably */

/*
 * Gets a file from the server.
 */
int		queue_get_file(THREAD_DATA *td, const char *filename, const char *initiator);
void	try_get_next_file(THREAD_DATA *td);

/*
 * Tries to send a file to the server.
 */
int		queue_send_file(THREAD_DATA *td, const char *filename, const char *initiator);
void	try_send_next_file(THREAD_DATA *td);

/*
 * Disconnects fro the server.
 */
void	disconnect_from_server(THREAD_DATA *td);

/*
 * Called when the arena changes.
 */
void	arena_changed(THREAD_DATA *td, char *new_arena);

void	do_send_file(THREAD_DATA *td);

/* change arena */
void	go(THREAD_DATA *td, SHIP ship, char *arena);
#endif

