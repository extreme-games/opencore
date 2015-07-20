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

/* FIXME: create a net module instead of having it part of this file */

#include <pthread.h>	/* for pthreads, must be included first */

#include <sys/socket.h>	/* for socket() */
#include <sys/time.h>	/* for gettimeofday() */
#include <sys/types.h>	/* for standard types */
#include <sys/stat.h>	/* for stat() */
#include <sys/utsname.h>/* for uname() */
#include <sys/wait.h>	/* for waitpid() */
#include <sys/param.h>	/* for MIN */

#include <arpa/inet.h>  /* for inet_ntoa() */
#include <netdb.h>	/* for getaddrinfo */

#include <assert.h>	/* for assert macro */
#include <errno.h>	/* for errno */
#include <stdarg.h>	/* for vararg macros */
#include <stdio.h>	/* for io functions */
#include <string.h>	/* for strcpy */
#include <stdlib.h>	/* for rand_r() */
#include <unistd.h>	/* for sleep */

#include <list>		/* for std::list */

#include <Python.h>

#include "opencore.hpp" /* opencore */

#include "bsd/queue.h"	/* for queue/list definitions */

#include "core.hpp"	/* bot definitions */
#include "botman.hpp"	/* for bot management */
#include "cmd.hpp"	/* command related functions */
#include "cmdexec.hpp"	/* for cmd_exec */
#include "config.hpp"	/* config-file functions */
#include "db.hpp"
#include "encrypt.hpp"	/* encryption facilities */
#include "lib.hpp"	/* library functions */
#include "log.hpp"	/* logging */
#include "player.hpp"	/* player ops */
#include "msg.hpp"	/* message functions */
#include "ops.hpp"	/* op functions */
#include "parsers.hpp"	/* parser functions */
#include "phand.hpp"	/* packet handlers */
#include "psend.hpp"	/* packet senders */
#include "util.hpp"	/* delim_args */

#include "opencore_swig.hpp"

#define STEP_INTERVAL 100	/* main loop runs every x ms, same with EVENT_TICK */
#define RELIABLE_RETRANSMIT_INTERVAL	1000
#define DEFERRED_PACKET_SEND_INTERVAL	15	/* 1 deferred packet gets sent per this interval , at 480byte blocks, this is 16000 bytes per second */
#define CONFIG_MTIME_POLL_INTERVAL 5000

/* as of OpenBSD 4.0 the implementation of getaddrinfo isnt reentrant */
#ifdef LOCK_GETADDRINFO
pthread_mutex_t g_getaddrinfo_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void cmd_getfile(CORE_DATA *cd);

/* local prototypes */
static void	empty_pkt_queue(packet_list_t *l);
static void	empty_rpkt_queue(rpacket_list_t *l);
static void	empty_chunk_queue(chunk_list_t *l);
static void	empty_upload_list(upload_list_t *l);
static void	free_thread_data(THREAD_DATA *td);
static void	init_thread_data(THREAD_DATA *td);
static int	open_socket(THREAD_DATA::net_t *n);
static int	pull_packets(packet_list_t *l, uint8_t *buf, int len, bool cluster);
static void	send_outgoing_packets(THREAD_DATA *td);
static void	mainloop(THREAD_DATA *td);
static void	write_packet(THREAD_DATA *td, uint8_t *pkt, int pktl);
static uint32_t	gen_valid_mid(uint32_t d);

/* global data */
pthread_key_t	g_tls_key;	/* the global tls data key */
pid_list_t	g_pid_list;	/* all pids of existing processes + mutex */
pthread_mutex_t	g_pid_list_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t	g_permissionid = 0;;
uint32_t	g_machineid = 0;

/* array of packet handler callbacks */
static pkt_handler	g_pkt_core_handlers[256];
static pkt_handler	g_pkt_game_handlers[256];


static
uint64_t
hash_buf(void *buf, int sz)
{
	uint8_t *p = (uint8_t*)buf;
	uint64_t res = 0;
	for (int i = 0; i < sz; ++i) {
		res = 31 * res + p[i];
	}
	return res;
}


static
uint32_t
gen_valid_mid(uint32_t d)
{

	uint8_t *b = (uint8_t*)&d;

	b[0] = b[0] % 73;
	b[1] = b[1] % 129;
	b[2] = b[2];	/* full range acceptable */
	b[3] = (b[3] % 24) + 7;

	return d;
}

/*
 * Spawns the initial bot process and waits for them all to exit.
 */
int
main(int argc, char *argv[])
{
	char *pythonpath = getenv("PYTHONPATH");
	if (pythonpath) {
		char *str = (char*)xzmalloc(strlen(pythonpath) + strlen(":") + strlen("./libs") + 1);
		sprintf(str, "%s:%s", "./libs", pythonpath);
		setenv("PYTHONPATH", str, 1);
		free(str);
	} else {
		setenv("PYTHONPATH", "./libs", 1);
	}
	Py_Initialize();
	init_opencore();
	PyObject *sysPath = PySys_GetObject("path");
	PyObject *libDir = PyString_FromString(".");
	PyList_Append(sysPath, libDir);
	Py_DECREF(libDir);

	if (pthread_key_create(&g_tls_key, NULL) != 0) {
		Log(OP_SMOD, "Error creating thread-specific storage");
		exit(-1);
	}
	pthread_setspecific(g_tls_key, NULL);

	/* make directories */
	mkdir("files", 0700);

	/* setup packet handlers */
	for (int i = 0; i < 256; ++i) {
		g_pkt_core_handlers[i] = null_handler;
		g_pkt_game_handlers[i] = null_handler;
	}

	g_pkt_core_handlers[0x02] = pkt_handle_core_0x02;
	g_pkt_core_handlers[0x03] = pkt_handle_core_0x03;
	g_pkt_core_handlers[0x04] = pkt_handle_core_0x04;
	g_pkt_core_handlers[0x05] = pkt_handle_core_0x05;
	g_pkt_core_handlers[0x06] = pkt_handle_core_0x06;
	g_pkt_core_handlers[0x07] = pkt_handle_core_0x07;
	g_pkt_core_handlers[0x08] = pkt_handle_core_0x08_0x09;
	g_pkt_core_handlers[0x09] = pkt_handle_core_0x08_0x09;
	g_pkt_core_handlers[0x0A] = pkt_handle_core_0x0A;
	g_pkt_core_handlers[0x0E] = pkt_handle_core_0x0E;

	g_pkt_game_handlers[0x02] = pkt_handle_game_0x02;
	g_pkt_game_handlers[0x03] = pkt_handle_game_0x03;
	g_pkt_game_handlers[0x04] = pkt_handle_game_0x04;
	g_pkt_game_handlers[0x07] = pkt_handle_game_0x07;
	g_pkt_game_handlers[0x06] = pkt_handle_game_0x06;
	g_pkt_game_handlers[0x0A] = pkt_handle_game_0x0A;
	g_pkt_game_handlers[0x0D] = pkt_handle_game_0x0D;
	g_pkt_game_handlers[0x19] = pkt_handle_game_0x19;
	g_pkt_game_handlers[0x1C] = pkt_handle_game_0x1C;
	g_pkt_game_handlers[0x1D] = pkt_handle_game_0x1D;
	g_pkt_game_handlers[0x27] = pkt_handle_game_0x27;
	g_pkt_game_handlers[0x28] = pkt_handle_game_0x28;
	g_pkt_game_handlers[0x29] = pkt_handle_game_0x29;
	g_pkt_game_handlers[0x2E] = pkt_handle_game_0x2E;
	g_pkt_game_handlers[0x2F] = pkt_handle_game_0x2F;
	g_pkt_game_handlers[0x31] = pkt_handle_game_0x31;

	struct utsname uts;
	bzero(&uts, sizeof(struct utsname));
	uname(&uts);
	uint64_t hash = hash_buf(&uts, sizeof(struct utsname));
	g_machineid = gen_valid_mid(hash & 0xFFFFFFFF);
	g_permissionid = hash >> 32;

	load_op_file();

	log_init();
	db_init();
	botman_init();

	/* run the master bot */
	Log(OP_MOD, "Starting master into #master");
	char *err = StartBot("master", "#master", NULL);
	if (err) {
		LogFmt(OP_MOD, "Error starting master bot: %s", err);
		return -1;
	}

	/* become the bot management thread and loop */
	botman_mainloop();

	botman_shutdown();
	db_shutdown();
	log_shutdown();

	pthread_key_delete(g_tls_key);

	Py_Finalize();

	return 0;
}

void
disconnect_from_server(THREAD_DATA *td)
{
	player_simulate_player_leaves(td);
	player_free_absent_players(td, (ticks_ms_t)0);

	pkt_send_disconnect();
	send_outgoing_packets(td);

	close(td->net->fd);

	td->net->ticks->disconnected = get_ticks_ms();
	td->net->state = NS_DISCONNECTED;

	libman_export_event(td, EVENT_DISCONNECT, NULL);
}

void
cmd_getfile(CORE_DATA *cd)
{
	if (cd->cmd_argc != 2) {
		Reply("Usage: !getfile <filename>");
		return;
	}

	THREAD_DATA *td = get_thread_data();
	if (queue_get_file(td, cd->cmd_argv[1], cd->cmd_name)) {
		ReplyFmt("Queued file download: %s", cd->cmd_argv[1]);
	} else {
		ReplyFmt("Failed to queue file for download: %s", cd->cmd_argv[1]);
	}
}

void
cmd_putfile(CORE_DATA *cd)
{
	if (cd->cmd_argc != 2) {
		Reply("Usage: !putfile <filename>");
		return;
	}

	if (queue_send_file(get_thread_data(), cd->cmd_argv[1], cd->cmd_name)) {
		ReplyFmt("Queued file upload: %s", cd->cmd_argv[1]);
	} else {
		ReplyFmt("Failed to queue file for upload: %s", cd->cmd_argv[1]);
	}
}

int
QueueGetFile(const char *filename, const char *initiator)
{
	if (strstr(filename, "..") || strstr(filename, "/") || strstr(filename, "\\")) {
		return 0;
	}

	THREAD_DATA *td = get_thread_data();

	return queue_get_file(td, filename, initiator);
}

int
QueueSendFile(const char *filename, const char *initiator)
{
	if (strstr(filename, "..") || strstr(filename, "/") || strstr(filename, "\\")) {
		return 0;
	}

	THREAD_DATA *td = get_thread_data();

	return queue_send_file(td, filename, initiator);
}

int
queue_get_file(THREAD_DATA *td, const char *filename, const char *initiator)
{
	THREAD_DATA::net_t::chunk_in_t *ci = td->net->chunk_i;

	if (strlen(filename) > 15) return 0;

	DOWNLOAD_ENTRY f;
	strlcpy(f.name, filename, 16);
	strlwr(f.name);
	strlcpy(f.initiator, initiator ? initiator : "", 24);
	ci->file_list->push_back(f);

	LogFmt(OP_SMOD, "Queued file download: %s", filename);

	try_get_next_file(td);

	return 1;
}


/*
 * The format of streams is:
 *
 * [0x00][0x0A][content length][data chunk ...]
 * Conforming to format string "AACZ"
 * Content length is the size of all data chunks combined (not
 * including headers)
 *
 * It is repeated for the entire size of the file.
 *
 * The format of file transfers is:
 * [0x16][char filename[16][data]
 * Format is "AZ16Z"
 * It is written as one giant packet encoded in a stream.
 * This can transfer up to 20mb files.
 */
int
queue_send_file(THREAD_DATA *td, const char *filename, const char *initiator)
{
	THREAD_DATA::net_t::send_file_data_t *sfd = td->net->send_file_data;

	if (strlen(filename) > 15) return 0;

	UPLOAD_DATA *ud = (UPLOAD_DATA*)xmalloc(sizeof(UPLOAD_DATA));
		
	strlcpy(ud->filename, filename, 64);
	strlcpy(ud->initiator, initiator ? initiator : "", 24);

	LogFmt(OP_SMOD, "Queued file upload: %s", filename);

	sfd->upload_list->push_back(ud);
	try_send_next_file(td);

	return 1;
}

void
try_send_next_file(THREAD_DATA *td)
{
	THREAD_DATA::net_t::send_file_data_t *sfd = td->net->send_file_data;

	if (sfd->in_use == 0 && !sfd->upload_list->empty()) {
		UPLOAD_DATA *ud = *sfd->upload_list->begin();

		sfd->in_use = 1;

		strlcpy(sfd->cur_initiator, ud->initiator, 64);
		strlcpy(sfd->cur_filename, ud->filename, 24);

		PubMessageFmt("*putfile %s", sfd->cur_filename);

		LogFmt(OP_SMOD, "Initiated file upload: %s", sfd->cur_filename);
		if (strlen(sfd->cur_initiator) > 0) {
			RmtMessageFmt(sfd->cur_initiator, "Initiated file upload: %s", sfd->cur_filename);
		}

		sfd->upload_list->erase(sfd->upload_list->begin());
		free(ud);
	} 
}

void
do_send_file(THREAD_DATA *td)
{
	THREAD_DATA::net_t::send_file_data_t *sfd = td->net->send_file_data;
	char *filename = sfd->cur_filename;
	const char *initiator = (strlen(sfd->cur_initiator) > 0) ? sfd->cur_initiator : NULL;

	char full_filename[64];

	snprintf(full_filename, 64, "files/%s", filename);

	initiator = initiator ? initiator : "";

	FILE *f = fopen(full_filename, "rb");
	if (f == NULL) {
		LogFmt(OP_SMOD, "Couldn't open file for sending: %s", filename);
		if (initiator) {
			RmtMessageFmt(initiator, "Couldn't open file for sending: %s", filename);
		}
		sfd->in_use = 0;
		return;
	}

	struct stat s;
	memset(&s, 0, sizeof(s));
	fstat(fileno(f), &s);
	int bufl = s.st_size + 17;
	uint8_t *buf = (uint8_t*)xmalloc(bufl);
	if (fread(&buf[17], bufl - 17, 1, f) != 1) {
		LogFmt(OP_SMOD, "Couldn't read file for sending: %s", filename);
		if (initiator) {
			RmtMessageFmt(initiator, "Couldn't read file for sending: %s", filename);
		}
		fclose(f);
		free(buf);
		sfd->in_use = 0;
		return;
	}

	/* add file transfer header */
	buf[0] = 0x16;
	strlcpy((char*)&buf[1], filename, 16);

	int bytes_left = bufl;

	/* send initial stream start */
	while (bytes_left > 0) {
		int nout = MIN(bytes_left, 220); // must fit into a cluster header
		PACKET *p = allocate_packet(6 + nout);
		build_packet(p->data, "AACZ",
		    0x00,
		    0x0A,
		    bufl,
		    &buf[bufl - bytes_left], nout);
		bytes_left -= nout;

		queue_packet_reliable(p, SP_DEFERRED);
	}

	free(buf);
}

/*
 * Check the get_file() queue and if there are entries, initiate
 * a file transfer.
 */
void
try_get_next_file(THREAD_DATA *td)
{
	THREAD_DATA::net_t *n = td->net;
	THREAD_DATA::net_t::chunk_in_t *ci = n->chunk_i;

	if (ci->in_use == 0 && ci->file_list->empty() == false) {
		DOWNLOAD_ENTRY *fe = &*ci->file_list->begin();
		LogFmt(OP_SMOD, "Initiating file download: %s", fe->name);
		if (strlen(fe->initiator) > 0) {
			RmtMessageFmt(fe->initiator, "Initiating file download: %s", fe->name);
		}
		PubMessageFmt("*getfile %s", fe->name);
		strlcpy(ci->initiator, fe->initiator, 24);
		ci->file_list->erase(ci->file_list->begin());
		ci->in_use = 1;
	}
}

/*
 * Connect to the server.
 */
int
connect_to_server(THREAD_DATA *td)
{
	Log(OP_MOD, "Connecting to server");

	if (open_socket(td->net) != 0) {
		return -1;
	}

	ticks_ms_t ticks = get_ticks_ms();

	td->net->state = NS_CONNECTING;
	pkt_send_client_key(td->net->encrypt->client_key);
	td->net->ticks->last_connection_request = ticks;
	td->net->ticks->last_pkt_received = ticks;

	return 0;
}

void
Go(char *arena)
{
	THREAD_DATA *td = get_thread_data();

	go(td, SHIP_SPECTATOR, arena);
}

void
go(THREAD_DATA *td, SHIP ship, char *arena)
{
	strlcpy(td->arena_change_request, arena ? arena : td->login->arenaname, 16);

	LogFmt(OP_REF, "Changing arenas: %s", td->arena_change_request);
	pkt_send_arena_login(ship, td->arena_change_request);

	player_simulate_player_leaves(td);
}

void
arena_changed(THREAD_DATA *td, char *new_arena)
{
	CORE_DATA *cd = libman_get_core_data(td);

	/* export the change event */

	strlcpy(cd->ac_old_arena, td->arena->name, 16);
	strlcpy(td->arena->name, new_arena, 16);
	cd->bot_arena = td->arena->name;
	libman_export_event(td, EVENT_ARENA_CHANGE, cd);
}

static
void
init_thread_data(THREAD_DATA *td)
{
	THREAD_DATA::net_t *n = td->net;

	memset(td->net->ticks, 0, sizeof(THREAD_DATA::net_t::ticks_t));
	memset(td->net->encrypt, 0, sizeof(THREAD_DATA::net_t::encrypt_t));
	memset(td->net->rel_o, 0, sizeof(THREAD_DATA::net_t::rel_sent_t));
	memset(td->net->rel_i, 0, sizeof(THREAD_DATA::net_t::rel_read_t));
	memset(td->net->stats, 0, sizeof(THREAD_DATA::net_t::stats_t));

	td->net->state = NS_DISCONNECTED;

	n->queues->h_prio = new packet_list_t;
	n->queues->n_prio = new packet_list_t;
	n->queues->d_prio = new packet_list_t;
	n->rel_o->queue = new rpacket_list_t;
	n->rel_i->queue = new rpacket_list_t;
	n->chunk_i->queue = new chunk_list_t;
	n->chunk_i->file_list = new file_list_t;
	n->chunk_i->in_use = 0;

	n->send_file_data->upload_list = new upload_list_t;
	n->send_file_data->in_use = 0;
	strcpy(n->send_file_data->cur_initiator, "");
	strcpy(n->send_file_data->cur_filename, "");

	ticks_ms_t ticks = get_ticks_ms();

	td->rand_seed = ticks;

	n->encrypt->client_key = -rand_r(&td->rand_seed);
	n->encrypt->server_key = 0;
	n->encrypt->use_encryption = 0;
	
	memset(n->stream->data, 0, sizeof(n->stream->data));
	n->stream->offset = 0;

	n->ticks->last_deferred_flush = ticks;
	n->ticks->last_pkt_received = ticks;
	n->ticks->last_sync_received = ticks;
	n->ticks->last_pos_update_sent = ticks;

	td->parser->info_line = 1;

	td->periodic->last_info = ticks;
	td->periodic->last_einfo = ticks;

	td->arena->ticks->last_player_flush = ticks;
	strlcpy(td->arena->name, "", 16);
	strlcpy(td->arena_change_request, "", 16);

	td->bot_pos->x = 512 * 16;
	td->bot_pos->y = 512 * 16;
}

static
void
free_thread_data(THREAD_DATA *td)
{
	THREAD_DATA::net_t *n = td->net;

	empty_pkt_queue(n->queues->h_prio);
	delete n->queues->h_prio;

	empty_pkt_queue(n->queues->n_prio);
	delete n->queues->n_prio;

	empty_pkt_queue(n->queues->d_prio);
	delete n->queues->d_prio;

	empty_rpkt_queue(n->rel_o->queue);
	delete n->rel_o->queue;

	empty_rpkt_queue(n->rel_i->queue);
	delete n->rel_i->queue;

	empty_chunk_queue(n->chunk_i->queue);
	delete n->chunk_i->queue;

	delete n->chunk_i->file_list;	/* entries are not dynamically allocated */

	empty_upload_list(n->send_file_data->upload_list);
	delete n->send_file_data->upload_list;
}


void
cmd_startbot(CORE_DATA *cd)
{
	if (cd->cmd_argc != 3) {
		RmtMessageFmt(cd->cmd_name, "Usage: %s <type> <arena>", cd->cmd_argv[0]);
		return;
	}

	char *err = StartBot(cd->cmd_argv[1], cd->cmd_argv[2], cd->cmd_name);
	if (err) {
		RmtMessageFmt(cd->cmd_name, "Couldn't start bot: %s", err);
	} else {
		RmtMessageFmt(cd->cmd_name, "Started %s in %s", cd->cmd_argv[1], cd->cmd_argv[2]);
	}
}

void
cmd_go(CORE_DATA *cd)
{
	if (cd->cmd_argc != 2) {
		Reply("Usage: !go <arena");
		return;
	}

	ReplyFmt("Changing to arena: %s", cd->cmd_argv[1]);
	Go(cd->cmd_argv[1]);
}

#define CMD_NONE 0
#define CMD_DIE 1
#define CMD_STOPBOT 2
#define CMD_GETFILE 3
#define CMD_PUTFILE 4
#define CMD_LISTBOTS 5
#define CMD_TYPES 6
#define CMD_LOADTYPES 7
#define CMD_LOG 8
#define CMD_CMDLOG 9
#define CMD_ABOUT 10
#define CMD_INSLIB 11
#define CMD_RMLIB 12
#define CMD_HELP 13
#define CMD_SYSINFO 14
#define CMD_LOADOPS 15
#define CMD_LISTOPS 16
#define CMD_EXEC 17
#define CMD_STARTBOT 18
#define CMD_GO 19


static
void
register_commands()
{
	bool is_master = strcmp(get_thread_data()->bot_type, "master") == 0;

	if (is_master) {
		RegisterCommand(CMD_DIE, "!die", CORE_NAME, 9, CMD_PRIVATE | CMD_REMOTE, NULL, "Shut down the bot core", "This shuts down the bot core and all running bots.");
		RegisterCommand(CMD_STARTBOT, "!startbot", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, "<type> <arena>", "Start a bot", "Start a bot of <type> into <arena>");
		RegisterCommand(CMD_GETFILE, "!getfile", CORE_NAME, 7, CMD_PRIVATE | CMD_REMOTE, "<filename>", "Force the bot to download <filename>", "Force the bot to download <filename>");
		RegisterCommand(CMD_PUTFILE, "!putfile", CORE_NAME, 7, CMD_PRIVATE | CMD_REMOTE, "<filename>", "Force the bot to upload <filename>", "Force the bot to upload <filename>");
		RegisterCommand(CMD_LISTBOTS, "!listbots", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, NULL, "List the running bots", "List the running bots");
		RegisterCommand(CMD_TYPES, "!types", CORE_NAME, 1, CMD_PRIVATE | CMD_REMOTE, NULL, "List types of bots tha can be started", "List types of bots tha can be started");
		RegisterCommand(CMD_LOADTYPES, "!loadtypes", CORE_NAME, OP_HSMOD, CMD_PRIVATE | CMD_REMOTE, NULL, "Load the bot types file", "Load the bot types file");
		RegisterCommand(CMD_SYSINFO, "!sysinfo", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, NULL, "Show system information", "Show system information, including load averages. Load averages are the average number of processes in the system process queue over the last 1, 5, and 15 minutes.");
		RegisterCommand(CMD_LOADOPS, "!loadops", CORE_NAME, 5, CMD_PRIVATE | CMD_REMOTE, NULL, "Load op file", "Reload the op file. This affects all bots.");
		RegisterCommand(CMD_EXEC, "!exec", CORE_NAME, 9, CMD_PRIVATE | CMD_REMOTE, NULL, "Execute a shell command", "Execute a shell command and spew the output");
	} else {
		RegisterCommand(CMD_STOPBOT, "!stopbot", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, NULL, "Stops this bot", "Stops this bot");
		RegisterCommand(CMD_GO, "!go", CORE_NAME, 7, CMD_PRIVATE | CMD_REMOTE, "<arena>", "Change the bot's arena", "Change the bot's arena");
	}

	RegisterCommand(CMD_LOG, "!log", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, NULL, "Show the bot's log", "Show the bot's log");
	RegisterCommand(CMD_CMDLOG, "!cmdlog", CORE_NAME, 2, CMD_PRIVATE | CMD_REMOTE, NULL, "Show the bot's command log", "Show the bot's command log");
	RegisterCommand(CMD_ABOUT, "!about", CORE_NAME, 0, CMD_PRIVATE | CMD_REMOTE, NULL, "Show core and library information", "Shows information about the core and libraries.");
	RegisterCommand(CMD_INSLIB, "!inslib", CORE_NAME, 9, CMD_PRIVATE | CMD_REMOTE, "<lib name>", "Load a module into the bot", "Load a module into the bot");
	RegisterCommand(CMD_RMLIB, "!rmlib", CORE_NAME, 9, CMD_PRIVATE | CMD_REMOTE, "<lib name>", "Unload a library from the bot", "Unload a library from the bot");
	RegisterCommand(CMD_HELP, "!help", CORE_NAME, 0, CMD_PRIVATE | CMD_REMOTE, "[class | command]", "Show available commands", "This displays all commands. If a class is specified, all commands in that class are shown. If a command is specified, its usage and long description are shown.");
	RegisterCommand(CMD_LISTOPS, "!listops", CORE_NAME, 1, CMD_PRIVATE | CMD_REMOTE, NULL, "List bot ops", "List bot ops at or below your access level");
}


void
GameEvent(CORE_DATA *cd)
{
	switch (cd->event) {
	case EVENT_START:
		RegisterPlugin(CORE_VERSION, CORE_NAME, "cycad", CORE_VERSION, __DATE__, __TIME__, "core handler", 0);
		register_commands();
		break;
	case EVENT_COMMAND:
		switch (cd->cmd_id) {
		case CMD_DIE: cmd_die(cd); break;
		case CMD_STOPBOT: cmd_stopbot(cd); break;
		case CMD_GETFILE: cmd_getfile(cd); break;
		case CMD_PUTFILE: cmd_putfile(cd); break;
		case CMD_LISTBOTS: cmd_listbots(cd); break;
		case CMD_TYPES: cmd_types(cd); break;
		case CMD_LOADTYPES: cmd_loadtypes(cd); break;
		case CMD_LOG: cmd_log(cd); break;
		case CMD_CMDLOG: cmd_cmdlog(cd); break;
		case CMD_ABOUT: cmd_about(cd); break;
		case CMD_INSLIB: cmd_inslib(cd); break;
		case CMD_RMLIB: cmd_rmlib(cd); break;
		case CMD_HELP: cmd_help(cd); break;
		case CMD_SYSINFO: cmd_sysinfo(cd); break;
		case CMD_LOADOPS: cmd_loadops(cd); break;
		case CMD_LISTOPS: cmd_listops(cd); break;
		case CMD_EXEC: cmd_exec(cd); break;
		case CMD_STARTBOT: cmd_startbot(cd); break;
		case CMD_GO: cmd_go(cd); break;
		default: break;
		}
		break;
	default:
		break;
	}
}


/*
 * Each bot's entry point. 'arg' must be a dynamically allocated THREAD_ARG
 * pointer. The pointer is freed by the new thread.
 */
void*
BotEntryPoint(void *arg)
{
	THREAD_DATA *td = (THREAD_DATA*)arg;

	/* set the tls key */
	pthread_setspecific(g_tls_key, td);

	init_thread_data(td);

	db_instance_init();
	cmd_instance_init(td);
	player_instance_init(td);
	libman_instance_init(td);

	/* load the core pseudo-plugin */
	libman_load_library(td, NULL);

	/* load the bots libraries */
	int num_plugins = DelimCount(td->libstring, ' ') + 1;
	for (int i = 0; i < num_plugins; ++i) {
		char libname[64];
		DelimArgs(libname, 64, td->libstring, i, ' ', false);
		if (strlen(libname) > 0) {
			libman_load_library(td, libname);
		}
	}

	/* mainloop */
	mainloop(td);

	disconnect_from_server(td);

	player_instance_shutdown(td);
	libman_instance_shutdown(td);
	cmd_instance_shutdown(td);
	db_instance_shutdown();

	botman_bot_exiting(td->botman_handle);

	free_thread_data(td);

	free(td);

	return NULL;
}


static
void
mainloop(THREAD_DATA *td)
{
	THREAD_DATA::net_t *n = td->net;

	ticks_ms_t acc, ticks, lticks;	/* accumulator, current ticks, last iteration ticks */

	int	pktl;			/* packet length */
	uint8_t pkt[MAX_PACKET];	/* buffer space for a packet */

	if (connect_to_server(td) != 0) {
		free_thread_data(td);
		LogFmt(OP_MOD, "Error performing initial connect");
		return;
	}

	acc = 0;
	ticks = get_ticks_ms();
	lticks = ticks;
	ticks_ms_t last_botman_checkin = ticks;
	ticks_ms_t last_botman_stopcheck = ticks;
	ticks_ms_t last_config_mtime_check = ticks;
	while (td->running >= 0) {
		ticks = get_ticks_ms();
		acc += ticks - lticks;
		lticks = ticks;

		/* check in with the bot manager */
		if (ticks - last_botman_checkin >= BOTMAN_CHECKIN_INTERVAL) {
			botman_bot_checkin(td->botman_handle);
			last_botman_checkin = ticks;
		}
		if (ticks - last_botman_stopcheck >= BOTMAN_STOPCHECK_INTERVAL) {
			if (botman_bot_shouldstop(td->botman_handle)) {
				td->running = -1;
			}
			last_botman_stopcheck = ticks;
		}

		/* flush out tick events to bots */
		if (acc >= STEP_INTERVAL) { 
			libman_expire_timers(td);
			while(acc >= STEP_INTERVAL) {
				/* event_tick */
				libman_export_event(td, EVENT_TICK, NULL);
				acc -= STEP_INTERVAL;
			}
		}

		/* if the bot is disconnected, see if it is time to reconnect */
		if (n->state == NS_DISCONNECTED) {
			if (ticks - n->ticks->disconnected > 60000) {
				free_thread_data(td);
				init_thread_data(td);
				connect_to_server(td);
			} else {
				usleep(50000);	/* 50ms */
				continue;
			}
		}

		/* see if the config file has been modified and if so send a reread event */
		if (ticks - last_config_mtime_check >= CONFIG_MTIME_POLL_INTERVAL) {
			struct stat attr;
			memset(&attr, 0, sizeof(struct stat));
			if (stat(td->config->filename, &attr) == 0) {
				if (td->config->last_modified_time != attr.st_mtime) {
					libman_export_event(td, EVENT_CONFIG_CHANGE, NULL);
					td->config->last_modified_time = attr.st_mtime;
				}
			}
			last_config_mtime_check = ticks;
		}

		/* use up to STEP_INTERVAL ms for the db thread */
		ticks_ms_t ticks_taken = get_ticks_ms() - ticks;
		ticks_ms_t db_ticks = ticks_taken > STEP_INTERVAL ? STEP_INTERVAL : STEP_INTERVAL - ticks_taken;
		db_instance_export_events(db_ticks);

		/* read a packet or wait for a timeout */
		ticks_taken = get_ticks_ms() - ticks;
		int timeout = ticks_taken > STEP_INTERVAL ? 0 : STEP_INTERVAL - ticks_taken;
		while (poll(n->pfd, 1, timeout) > 0) {
			/* process incoming packet, data is waiting */
			pktl = (int)read(n->fd, pkt, MAX_PACKET);
			if (pktl >= 0) {
				++n->stats->packets_read;
				n->ticks->last_pkt_received = get_ticks_ms();

				if (n->encrypt->use_encryption) {
					if (pkt[0] == 0x00) {
						if (pktl >= 2) {
							decrypt_buffer(td, &pkt[2], pktl-2);
						}
					} else {
						decrypt_buffer(td, &pkt[1], pktl-1);
					}
				}

				if (td->debug->spew_packets) {
					spew_packet(pkt, pktl, DIR_INCOMING);
				}

				process_incoming_packet(td, pkt, pktl);
			}

			ticks_taken = get_ticks_ms() - lticks;
		}

		/* update the tick count after potential sleeping in poll() */
		ticks = get_ticks_ms();

		/* network state specfic actions */
		if (n->state == NS_CONNECTING) {
			/* retransmit connection request if it was lost */
			if (ticks - n->ticks->last_connection_request > 15000) {
				pkt_send_client_key(n->encrypt->client_key);
				n->ticks->last_connection_request = ticks;
			}
		} else if (ticks - n->ticks->last_pkt_received > 30*1000) {
			/* disconnect if no packets have been received for 30 seconds */
			Log(OP_MOD, "No data received for 30 seconds, reconnecting...");
			disconnect_from_server(td);
			continue;
		}

		/* transmit player position update if necessary */
		if (n->state == NS_CONNECTED && td->in_arena) {
			if ((ticks - n->ticks->last_pos_update_sent > 100
			    && td->bot_ship != SHIP_SPECTATOR)
			    || (ticks - n->ticks->last_pos_update_sent > 1000
			    && td->bot_ship == SHIP_SPECTATOR)) {
				pkt_send_position_update(td->bot_pos->x, td->bot_pos->y,
				    td->bot_vel->x, td->bot_vel->y);
				n->ticks->last_pos_update_sent = ticks;
			}
		}

		/* send periodic info/einfo */
		if (n->state == NS_CONNECTED) {
			// subtract 10000 to offset this by 10 seconds from *einfo to avoid filling buffers with commands/responses
			if (td->periodic->info && ticks - (td->periodic->last_info - 10000U) >= td->periodic->info) {
				int nhere = player_get_phere(td);
				PLAYER *parray = player_get_parray(td);
				for (int i = 0; i < nhere; ++i) {
					if (parray[i].here && td->enter->send_info) {
						PrivMessage(&parray[i], "*info");
					}
				}

				td->periodic->last_info = ticks;
			}

			if (td->periodic->einfo && ticks - td->periodic->last_einfo >= td->periodic->einfo) {
				int nhere = player_get_phere(td);
				PLAYER *parray = player_get_parray(td);
				for (int i = 0; i < nhere; ++i) {
					if (parray[i].here && td->enter->send_einfo) {
						PrivMessage(&parray[i], "*einfo");
					}
				}

				td->periodic->last_einfo = ticks;
			}
		}

		/* retransmit reliable packets that have not been acked */
		rpacket_list_t *l = n->rel_o->queue;
		rpacket_list_t::iterator iter = l->begin();
		while (iter != l->end()) {
			RPACKET *rp = *iter;
			if (ticks - rp->ticks > RELIABLE_RETRANSMIT_INTERVAL) {
				PACKET *p = allocate_packet(rp->len);
				memcpy(p->data, rp->data, rp->len);

				/* update packets retransmit tick */
				rp->ticks = ticks;

				queue_packet(p, SP_HIGH);
			}
			
			++iter;
		}

		/* free absent players if its time */
		ticks_ms_t flush_check_interval = 60 * 60 * 1000;
		if (ticks - td->arena->ticks->last_player_flush > flush_check_interval) {
			player_free_absent_players(td, flush_check_interval);
			td->arena->ticks->last_player_flush = ticks;
		}

		/* write packets generated during loop iteration */
		send_outgoing_packets(td);
	} /* while td->running != 0 */
}

static
void
empty_pkt_queue(packet_list_t *l)
{
	while (l->begin() != l->end()) {
		free_packet(*l->begin());
		l->erase(l->begin());
	}
}

static
void
empty_rpkt_queue(rpacket_list_t *l)
{
	while (l->begin() != l->end()) {
		free_rpacket(*l->begin());
		l->erase(l->begin());
	}
}

static
void
empty_chunk_queue(chunk_list_t *l)
{
	while (l->begin() != l->end()) {
		free_chunk(*l->begin());
		l->erase(l->begin());
	}
}

static
void
empty_upload_list(upload_list_t *l)
{
	while (l->begin() != l->end()) {
		free(*l->begin());
		l->erase(l->begin());
	}
}

/* pull packets from packet list into buf, and erase/free them */
static
int
pull_packets(packet_list_t *l, uint8_t *buf, int len, bool cluster)
{
	int bytes_placed = 0;
	int offset = 0;

	int ch_size = 0; /* cluster header size */
	if (cluster)
		ch_size = 1;

	while(l->begin() != l->end()) {
		PACKET *p = *l->begin();

		/* if theres room in the packet for cluster+cluster len */
		if (p->len + ch_size <= len - offset) {
			if (cluster) {
				assert(p->len <= 0xFF);
				if (p->len > 0xFF) abort();
				bytes_placed = build_packet(
				    &buf[offset], "AZ", p->len, p->data, p->len);
			} else {
				bytes_placed = build_packet(
				    &buf[offset], "Z", p->data, p->len);
			}

			offset += bytes_placed;
			free_packet(p);
			l->erase(l->begin());
		} else {
			break;
		}
	}	

	return offset;
}

static
void
send_outgoing_packets(THREAD_DATA *td)
{
	THREAD_DATA::net_t *n = td->net;

	ticks_ms_t ticks = get_ticks_ms();

	/* write any outgoing packets */
	if (ticks - n->ticks->last_deferred_flush > DEFERRED_PACKET_SEND_INTERVAL) {
		/*
		 * if its time to flush deferred packets, grab one and place it into the SP_NORMAL queue
		 * so it gets sent out immediately
		 */

		/* TODO: This should be rate limited based on packet size, not interval */
		size_t ndeferred = 0;
		while (!n->queues->d_prio->empty() && ndeferred++ < 3) { // send 3 packets per interval
			PACKET *p = *n->queues->d_prio->begin();
			n->queues->d_prio->erase(n->queues->d_prio->begin());

			queue_packet_reliable(p, SP_NORMAL);
		}

		n->ticks->last_deferred_flush = ticks;
	}

	/* packet size + space for unencrypted type id */
	/* TODO: fix MAX_PACKET vs constants */
	uint8_t pkt[MAX_PACKET];
	int pkt_count;
	do {
		pkt_count = 0;

		pkt_count += n->queues->h_prio->size();
		pkt_count += n->queues->n_prio->size();

		if (pkt_count > 0) {
			bool cluster = pkt_count >= 2;
			int pktl = 0;

			if (cluster) {
				build_packet(pkt, "AA", 0x00, 0x0e);
				pktl += 2;
			}

			/* pull packets from all the various queues */
			pktl += pull_packets(n->queues->h_prio,
			    &pkt[pktl], MAX_PACKET - pktl, cluster);
			pktl += pull_packets(n->queues->n_prio,
			    &pkt[pktl], MAX_PACKET - pktl, cluster);

			write_packet(td, pkt, pktl);
		}
		
	} while (pkt_count > 0);
}

static
void
write_packet(THREAD_DATA *td, uint8_t *pkt, int pktl)
{
	if (td->debug->spew_packets) {
		spew_packet(pkt, pktl, DIR_OUTGOING);
	}

	if (td->net->encrypt->use_encryption) {
		if (pkt[0] == 0x00) {
			if (pktl >= 2) {
				encrypt_buffer(td,
				    &pkt[2], pktl - 2);
			}
		} else {
			encrypt_buffer(td, &pkt[1],
			    pktl - 1);
		}
	}

	send(td->net->fd, pkt, pktl, 0);
	td->net->stats->packets_sent += 1;
}

/*
 * Setup and initialize a network socket.
 */
static
int
open_socket(THREAD_DATA::net_t *n) 
{
	struct addrinfo ai_hints, *ai;
	bzero(&ai_hints, sizeof(ai_hints));
	ai_hints.ai_socktype = SOCK_DGRAM;
	ai_hints.ai_family = AF_INET;
	ai_hints.ai_flags = AI_CANONNAME;

#ifdef LOCK_GETADDRINFO
	pthread_mutex_lock(&g_getaddrinfo_mutex);
#endif

	/* resolve the host name */
	int getaddrret = getaddrinfo(n->servername,
	    n->serverport, &ai_hints, &ai);

#ifdef LOCK_GETADDRINFO
	pthread_mutex_unlock(&g_getaddrinfo_mutex);
#endif

	if (getaddrret != 0) {
		StopBot("error resolving hostname");
		return -1;
	}
	
	/* copy resolved sockaddr_in struct to the bot's data and free it */
	memcpy(&n->serv_sin, ai->ai_addr, sizeof(struct sockaddr_in));
	/* ai->ai_canonname can be null on macs if the ip doesnt have rdns */
	strlcpy(n->servername, ai->ai_canonname ? ai->ai_canonname : n->servername, 128);
	snprintf(n->serverport, 8, "%hu", ntohs(n->serv_sin.sin_port)); 
	freeaddrinfo(ai);

	/* resolve the ip and store it for future reference */
	if (inet_ntop(n->serv_sin.sin_family, &n->serv_sin.sin_addr, 
	    n->serverip, INET_ADDRSTRLEN) == NULL)
		strlcpy(n->serverip, "unknown", INET_ADDRSTRLEN);

	/* create a udp socket */
	if ((n->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		StopBot("error creating network socket");
		return -1;
	}
	
	if (connect(n->fd, (struct sockaddr*)&n->serv_sin,
	    sizeof(struct sockaddr_in)) == -1) {
		StopBot("error connecting to network socket");
		return -1;
	}

	n->pfd[0].fd = n->fd;
	n->pfd[0].events = POLLRDNORM;

	return 0;
}

/*
 * Queue a packet 'p' with sending priority 'priority'. 'p' must point
 * to a packet allocated with allocate_packet().
 */
void
queue_packet(PACKET *p, int priority)
{
	assert(p->len <= 255);

	THREAD_DATA::net_t *n = get_thread_data()->net;

	if (priority == SP_HIGH)
		n->queues->h_prio->push_back(p);
	else if (priority == SP_NORMAL)
		n->queues->n_prio->push_back(p);	
	else if (priority == SP_DEFERRED) {
		LogFmt(OP_HSMOD, "Upgrading unreliable deferred packet to reliable");
		n->queues->d_prio->push_back(p);
	}
}


void
queue_packet_large(PACKET *p, int priority)
{
	// chunk packets need to be sent in succession
	if (priority == SP_DEFERRED) priority = SP_NORMAL;

	// break the packet down into chunks
	int offset = 0;
	while (offset < p->len) {
		// also must fit into a cluster packet
		int chunk_size = MIN(p->len - offset, 240); // min size must be < guard clauses in queue_packet and queue_packet_reliable
		PACKET *q = allocate_packet(chunk_size + 2);
		build_packet(q->data, "AAZ",
			0x00,
			(offset + chunk_size < p->len) ? 0x08 : 0x09,
			&p->data[offset],
			chunk_size
			);

		queue_packet_reliable(q, priority);

		offset += chunk_size;
	}

	free_packet(p);
}

/*
 * Queue a packet for reliable transmission.
 */
void
queue_packet_reliable(PACKET *p, int priority) 
{
	assert(p->len < 510);

	THREAD_DATA::net_t *n = get_thread_data()->net;

	if (priority == SP_DEFERRED) {
		n->queues->d_prio->push_back(p);
	} else if (priority == SP_HIGH || priority == SP_NORMAL) {
		uint32_t ack_id;
		PACKET *p2;

		/* packet with space for reliable header */
		p2 = allocate_packet(p->len + 6);

		ack_id = n->rel_o->next_ack_id++;

		/* add reliable header and free old (headerless) packet */
		build_packet(p2->data, "AACZ", 0x00, 0x03, ack_id, p->data, p->len);
		free_packet(p);

		/* store a copy for retransmission until its ack is received */
		RPACKET *rp = allocate_rpacket(p2->len, get_ticks_ms(), ack_id);
		memcpy(rp->data, p2->data, p2->len);
		n->rel_o->queue->push_back(rp);

		/* send p2 */
		queue_packet(p2, priority);
	} else {
		assert(0);
	}
}


/*
 * This function dispatches packets to their appropriate handlers.
 */
void
process_incoming_packet(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len > 0) {
		if (buf[0] == 0x00) {
			if (len >= 2)
				g_pkt_core_handlers[buf[1]](td, buf, len);
		} else
			g_pkt_game_handlers[buf[0]](td, buf, len);
	}
}


void
SetPosition(uint16_t x, uint16_t y, uint16_t xv, uint16_t yv)
{
	THREAD_DATA *td = get_thread_data();

	td->bot_pos->x = x;
	td->bot_pos->y = y;
	td->bot_vel->x = xv;
	td->bot_vel->y = yv;

	pkt_send_position_update(x, y, xv, yv);
	td->net->ticks->last_pos_update_sent = get_ticks_ms();
}


void
StopBot(char *reason) 
{
	THREAD_DATA *td = get_thread_data();

	LogFmt(OP_MOD, "Shutting down: %s", reason ? reason : "Unspecified");
	td->running = -1;
}


void
StopBotFmt(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	
	char buf[256];
	vsnprintf(buf, 256, fmt, ap);

	StopBot(buf);

	va_end(ap);
}


void
SendPlayer(PLAYER *p, const char *arena)
{
	THREAD_DATA *td = get_thread_data();
	
	if (p->here) PrivMessageFmt(p, "*sendto %s,%s,%.15s", td->net->serverip, td->net->serverport, td->arena ? arena : "");
}

