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

/* FIXME: add handler for weapons packet and its EVENT_UPDATE */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libgen.h>
#include <stdio.h>
#include <unistd.h>

#include "opencore.hpp"

#include "phand.hpp"

#include "core.hpp"
#include "cmd.hpp"
#include "encrypt.hpp"
#include "lib.hpp"
#include "msg.hpp"
#include "parsers.hpp"
#include "player.hpp"
#include "psend.hpp"
#include "util.hpp"

static void export_msg(THREAD_DATA *td, MSG_TYPE msg_type, PLAYER *p, int chatid, char *name, char *msg);
static void simulate_file_received(THREAD_DATA *td, char *filename);

void
null_handler(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (td->debug->show_unhandled_packets) {
		if (buf[0] == 0x00 && len >= 2) {
			LogFmt(OP_HSMOD, "Unhandled core packet: %2.2X:%2.2X", buf[0], buf[1]);
			spew_packet(buf, len, DIR_INCOMING);
		} else if (buf[0] != 0x00) {
			LogFmt(OP_HSMOD, "Unhandled game packet: %2.2X", buf[0]);
			spew_packet(buf, len, DIR_INCOMING);
		} else {
			Log(OP_HSMOD, "Game packet with no ID received");
		}
	}
}

void
pkt_handle_core_0x02(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t *n = td->net;

	if (n->state == NS_KEYEXCHANGE) {
		if (len >= 6) {
			extract_packet(buf, "AAc", NULL, NULL, &n->encrypt->server_key);
			init_encryption(td);
			td->net->encrypt->use_encryption = 1;
			pkt_send_login(td->bot_name, td->login->password,
			    g_machineid, 0, g_permissionid);
			pkt_send_sync_request();
			n->state = NS_LOGGINGIN;
		}
	}
}

/* reliable message handler */
void
pkt_handle_core_0x03(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t *n = td->net;

	if (len >= 6) {
		uint32_t ack_id;
		extract_packet(buf, "AAC", NULL, NULL, &ack_id);
		pkt_send_ack(ack_id);

		int msg_len = len-6;
		uint8_t *msg_ptr = &buf[6];

		/* if this ack is the next one in succession */
		if (ack_id == n->rel_i->next_ack_id) {
			/* process the message, if there is one */
			if (msg_len > 0) {
				process_incoming_packet(td, msg_ptr, msg_len);
			}
			++n->rel_i->next_ack_id;

			/* process all reliable packets waiting in the receive queue */
			rpacket_list_t *l = n->rel_i->queue;
			rpacket_list_t::iterator iter = l->begin();

			while (iter != l->end()) {
				RPACKET *rp = *iter;

				if (rp->ack_id == n->rel_i->next_ack_id) {
					process_incoming_packet(td, rp->data, rp->len);
					free_rpacket(rp);

					/* only 1 pkt per ack id is in the queue at any time */
					l->erase(iter);
					iter = l->begin();

					++n->rel_i->next_ack_id;
				} else {
					++iter;
				}
			}

		}
		/* lesser ack has not been received, store packet in holding queue
		   for later processing */
		else if (ack_id > n->rel_i->next_ack_id) { 
			rpacket_list_t *l = n->rel_i->queue;
			rpacket_list_t::iterator iter = l->begin();

			/* if the ack isnt already in the rp list, add it */
			while(iter != l->end()) {
			RPACKET *rp = *iter;
				if (rp->ack_id == ack_id)
					break;

				++iter;
			}
			if (iter == l->end()) { /* ack_id wasnt found in the list */
				RPACKET *rp = allocate_rpacket(msg_len, get_ticks_ms(), ack_id);
				memcpy(rp->data, msg_ptr, msg_len);

				n->rel_i->queue->push_back(rp);
			}
		}
	}
}


/* received an ack */
void
pkt_handle_core_0x04(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 6) {
		uint32_t ack_id;
		extract_packet(buf, "AAC", NULL, NULL, &ack_id);

		rpacket_list_t *l = td->net->rel_o->queue;
		rpacket_list_t::iterator iter = l->begin();

		while(iter != l->end()) {
			RPACKET *rp = *iter;
			if (rp->ack_id == ack_id) {
				free_rpacket(rp);
				iter = l->erase(iter);
			} else {
				++iter;
			}
		}
	}
}


/* received a sync request */
void
pkt_handle_core_0x05(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t *n = td->net;
	if (n->state == NS_CONNECTING) {
		if (len >= 6) {
			int32_t temp;
			extract_packet(buf, "AAc", NULL, NULL, &temp);	
			pkt_send_server_key(temp);
			n->state = NS_KEYEXCHANGE;
		}
	} else {
		pkt_send_sync_response();
	}
}


/* handle a sync response */
void
pkt_handle_core_0x06(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t *n = td->net;
	if (len >= 10) {
		extract_packet(buf, "AACC", NULL, NULL, NULL, NULL);
		n->ticks->last_sync_received = get_ticks_ms();
	}
}

void
pkt_handle_core_0x07(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 2) {
		Log(OP_MOD, "Received core disconnect from server");
		disconnect_from_server(td);
	}
}

void
pkt_handle_core_0x08_0x09(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t::stream_t *s = td->net->stream;
	if (len >= 2) {
		int stream_sz = len - 2;
		if (s->offset + stream_sz <= (int)sizeof(s->data)) {
			memcpy(&s->data[s->offset], buf, stream_sz);
			s->offset += stream_sz;
		} else {
			Log(OP_SMOD, "Ignoring stream too large and truncating content");
		}

		// 0x00 0x09 packet received, process the stream
		if (buf[1] == 0x09) {
			process_incoming_packet(td, s->data, s->offset);
			s->offset = 0;
		}
	}
}

/* disconnect packet */
void
pkt_handle_core_0x0d(THREAD_DATA *td, uint8_t *buf, int len)
{
	char reason[256];

	if (len >= 5) {
		strlcpy(reason, (char*)&buf[4], MIN(len - 4, 256));
	} else {
		strcpy(reason, "No reason");
	}

	LogFmt(OP_MOD, "Received disconnect from server (%s)", reason);
	disconnect_from_server(td);
}

/* cluster packet handling */
void
pkt_handle_core_0x0E(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 3) {
		int coffset = 2;
		int clen;
		while(coffset < len) {
			clen = MIN(buf[coffset], len-coffset);
			//spew_packet(&buf[coffset+1], clen, DIR_INCOMING);
			process_incoming_packet(td, &buf[coffset+1], clen);
			coffset += clen+1;
		}
	}	
}

/* file transfer packets */
void
pkt_handle_core_0x0A(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t::chunk_in_t *ci = td->net->chunk_i;

	if (ci->queue->empty()) {
		/* new transfer being initiated */
		if (len > 23) {
			uint32_t content_size;
			uint8_t content_type;
			char filename[16];
			extract_packet(buf, "AACAZ",
			    NULL,
			    NULL,
			    &content_size,
			    &content_type,
			    filename, 16);
			filename[sizeof(filename) - 1] = '\0';

			CHUNK *c = (CHUNK*)allocate_chunk(len - 23);
			memcpy(c->data, &buf[23], len - 23);

			/* initialize incoming chunk data */
			ci->in_use = 1;
			strlcpy(ci->filename, filename, 16);
			ci->content_size = content_size;
			ci->read_size = len - 6;
			ci->content_type = content_type;
			ci->queue->push_back(c);
		}	
	} else if (len > 6) {
		uint32_t content_size;
		extract_packet(buf, "AAC",
		    NULL,
		    NULL,
		    &content_size);

		CHUNK *c = (CHUNK*)allocate_chunk(len - 6);
		memcpy(c->data, &buf[6], len - 6);

		ci->queue->push_back(c);
		ci->read_size += len - 6;
	}

	if (ci->read_size == ci->content_size) {
		/* file transfer complete */

		char *base = basename(ci->filename);
		char outfile[64];
		snprintf(outfile, sizeof(outfile), "files/%s", base);
		strlwr(outfile);
		FILE *f = fopen(outfile, "wb");

		/* loop through the list, writing it to disk */
		chunk_list_t *l = ci->queue;
		while (l->begin() != l->end()) {
			CHUNK *c = *l->begin();
			
			if (f) {
				uint32_t bytes_written = fwrite(c->data, c->len, 1, f);
				if (bytes_written != 1) {
					fclose(f);
					f = NULL;
				}
			}

			free_chunk(c);
			l->erase(l->begin());
		}

		ci->in_use = 0;

		CORE_DATA *cd = libman_get_core_data(td);
		cd->transfer_filename = base;
		cd->transfer_direction = TRANSFER_S2C;
		if (f) {
			fclose(f);
			LogFmt(OP_SMOD, "Received file: %s", base);
			cd->transfer_success = 1;

			if (strlen(ci->initiator) > 0) {
				RmtMessageFmt(ci->initiator, "File received: %s", base);
			}
		} else {
			LogFmt(OP_SMOD, "Couldn't open file for writing: %s", base);
			cd->transfer_success = 0;

			if (strlen(ci->initiator) > 0) {
				RmtMessageFmt(ci->initiator, "File receipt failed: %s", base);
			}

		}

		libman_export_event(td, EVENT_TRANSFER, cd);

		try_get_next_file(td);
	}
}

void
pkt_handle_game_0x02(THREAD_DATA *td, uint8_t *buf, int len)
{
	//XXX with these commented out the arena_change_request may be unnecessary
	//if (strlen(td->arena_change_request) > 0) {
		arena_changed(td, td->arena_change_request);
		strlcpy(td->arena_change_request, "", 16);
	//}
}

/* enter arena */
void
pkt_handle_game_0x03(THREAD_DATA *td, uint8_t *buf, int len)
{
	int offset = 0;
	/* enter events are clustered together, even though the packet
	   isnt marked as a cluster */
	while (len-offset >= 64) {
		char name[20];
		char squad[20];
		PLAYER_ID pid;
		uint16_t freq;
		uint8_t ship;

		extract_packet(&buf[offset], "AAAzzCCBBBBBBA",
		    NULL,
		    &ship,
		    NULL,
		    name, 20,
		    squad, 20,
		    NULL,
		    NULL,
		    &pid,
		    &freq,
		    NULL,
		    NULL,
		    NULL,
		    NULL,
		    NULL
		    );
												
		name[20-1] = '\0';
		squad[20-1] = '\0';

		PLAYER *p;
		if ((p = player_player_entered(td, name, squad, pid, freq, ship)) != NULL) {
			if (strcasecmp(name, td->bot_name) == 0)
				PrivMessage(p, "*bandwidth 1000000");
			if (td->enter->send_watch_damage) PrivMessage(p, "*watchdamage");
			if (td->enter->send_einfo) PrivMessage(p, "*einfo");
			if (td->enter->send_info) PrivMessage(p, "*info");
			
		} else {
			Log(OP_HSMOD, "Received enter event for non-existent player");
		}

		offset += 64;
	}
}

/* handle player leaving */
void
pkt_handle_game_0x04(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 3) {
		PLAYER_ID pid;
		extract_packet(buf, "AB", NULL, &pid);

		player_player_left(td, pid);
	}
}

/* handle player kill */
void
pkt_handle_game_0x06(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 10) {
		PLAYER_ID killer, killed;
		extract_packet(buf, "AABBBB",
		    NULL,
		    NULL,
		    &killer,
		    &killed,
		    NULL,
		    NULL
		    );

		PLAYER *pkiller = player_find_by_pid(td, killer, MATCH_HERE);
		PLAYER *pkilled = player_find_by_pid(td, killed, MATCH_HERE);

		if (pkiller && pkilled) {
			CORE_DATA *cd = libman_get_core_data(td);

			cd->p1 = pkiller;
			cd->killer = pkiller;
			cd->p2 = pkilled;
			cd->killed = pkilled;

			libman_export_event(td, EVENT_KILL, cd);
		}

	}
}



/* message packet */
/* event_message */
/* TODO: this function is a quagmire and should be broken down in to 2+ functions,
 * a pkt handler, and a message helper handler
 */
void
pkt_handle_game_0x07(THREAD_DATA *td, uint8_t *buf, int len)
{
	/* do not process messages with no message length (len <= 5).
	   even though the message could contain a sound, a bot would
	   only process text. so even though a 5-byte packet is valid,
	   there is no reason for it to be processed by this core */
	if (len >= 6) {
		uint8_t msg_type, sound;
		uint16_t pid;
		char raw_msg[256];
		extract_packet(buf, "AAAB", NULL, &msg_type, &sound, &pid);
		strlcpy(raw_msg, (char*)&buf[5], MIN(len-5,256));

		PLAYER *p = NULL;
		einfo_t einfo;
		info_t	*info = &td->parser->info;
		int	*info_line = &td->parser->info_line;

		switch (msg_type) {
		case MSG_ARENA: /* arena message */
			/* file upload completed handler */
			if (strncmp(raw_msg, "File received: ", strlen("File received: ")) == 0) {
				THREAD_DATA::net_t::send_file_data_t *sfd = td->net->send_file_data;
				/* TODO: this should probably happen when the ack for the final chunk packet is sent, instead of being triggered
				 * by an arena message */
				/* 15 char max:      	cycad> *putfile #abcdefghijk.lvl
										Sending file: #abcdefghijk.lvl
										File received: #abcdefghijk.lv
				*/
				if (strncasecmp(&raw_msg[strlen("File received: ")], sfd->cur_filename, 15) == 0) {
					sfd->in_use = 0;

					/* notify initiator */
					LogFmt(OP_SMOD, "File uploaded: %s", sfd->cur_filename);
					if (strlen(sfd->cur_initiator) > 0) {
						RmtMessageFmt(sfd->cur_initiator, "File uploaded: %s", sfd->cur_filename);
					}

					/* this is copied to prevent cur_filename from changing during the exported event */
					char oldfilename[64];
					strlcpy(oldfilename, sfd->cur_filename, 64);

					CORE_DATA *cd = libman_get_core_data(td);
					cd->transfer_direction = TRANSFER_C2S;
					cd->transfer_filename = oldfilename;
					cd->transfer_success = 1;

					libman_export_event(td, EVENT_TRANSFER, cd);

					try_send_next_file(td);
				}
			}

			if (parse_einfo(raw_msg, &einfo) == true) {
				PLAYER *p = player_find_by_name(td, einfo.name, MATCH_HERE);
				if (p) {
					p->einfo_valid = 1;
					*p->einfo = einfo;

					CORE_DATA *cd = libman_get_core_data(td);
					cd->p1 = p;
					libman_export_event(td, EVENT_EINFO, cd);
				}
			} else if (parse_info(raw_msg, *info_line, info) == true) {
				*info_line += 1;	
				if (*info_line == 9) {
					*info_line = 1;
					/* export info event */
					PLAYER *p = player_find_by_name(td, info->name, MATCH_HERE);
					if (p) {
						/* store info data */
						p->info_valid = 1;
						*p->info = *info;

						CORE_DATA *cd = libman_get_core_data(td);
						cd->p1 = p;
						libman_export_event(td, EVENT_INFO, cd);
					}
				}
#if 0
			/* this can handle slightly interleaved infos if its uncommented, but is it really necessary? */
			} else if (parse_info(raw_msg, 1, &td->parser->info) == true) {
				*info_line = 2;	
#endif
			} else {
				*info_line = 1;
			}

			export_msg(td, msg_type, NULL, 0, NULL, raw_msg);
			break;
		case MSG_PUBLIC_MACRO: /* public macro */
		case MSG_PUBLIC: /* public message */
		case MSG_TEAM: /* team message, ignored */
		case MSG_FREQ: /* freq message, ignored */
		case MSG_PRIVATE: /* priv message */
			/* all of the above have similar processing */
			p = player_find_by_pid(td, pid, MATCH_HERE);
			if (p) {
				export_msg(td, msg_type, p, 0, p->name, raw_msg);
				if (raw_msg[0] == CMD_CHAR) {
					/* event_command */
					if (msg_type == MSG_PUBLIC ||
					    msg_type == MSG_PUBLIC_MACRO) {
						handle_command(td, p, p->name, CMD_PUBLIC, raw_msg);
					} else if (msg_type == MSG_PRIVATE) {
						handle_command(td, p, p->name, CMD_PRIVATE, raw_msg);
					}
				}
			}
			break;
		case MSG_WARNING: /* warning */
			export_msg(td, msg_type, NULL, 0, NULL, raw_msg);
			break;
		case MSG_REMOTE: /* remote message */
			/* pid == PID_NONE , unsourced (?alert) */
			/* pid == 0, rmt priv message */
			if (pid == PID_NONE) {
				alert_t alert;
				if (parse_rmt_alert(raw_msg, &alert) == true) {
					CORE_DATA *cd = libman_get_core_data(td);

					cd->alert_name = alert.name;
					cd->alert_type = alert.type;
					cd->alert_msg = alert.msg;
					cd->alert_arena = alert.arena;

					libman_export_event(td, EVENT_ALERT, cd);
				} else {
					Log(OP_HSMOD, "Alert parsing failed");
				}

			} else if (pid == 0) {
				rmt_msg_t rm;
				if (parse_rmt_message(raw_msg, &rm) == true) {
					export_msg(td, msg_type, NULL, 0, rm.name, rm.msg);
					if (rm.msg[0] == CMD_CHAR) {
						/* event_command */
						handle_command(td, NULL, rm.name, CMD_REMOTE, rm.msg);
					}	
				}
			}
			break;
		case MSG_SYSOP: /* sysop error */
			export_msg(td, msg_type, NULL, 0, NULL, raw_msg);
			break;
		case MSG_CHAT: /* chat message */
			{
				int chat_num = Atoi(raw_msg);
				if (chat_num >= 1 && chat_num <= MAX_CHATS) {
					char *name_start = strstr(raw_msg, ":");
					if (name_start) {
						name_start+=1;
						char *name_end = strstr(raw_msg, "> ");
						int name_len = name_end-name_start;
						if (name_end && name_len >=1
						    && name_len < 20) {
							char name[20];
							strncpy(name, name_start, name_len);
							name[name_len]='\0';

							char *msg = name_end+2;
							export_msg(td, msg_type, NULL, chat_num, name, msg);
							if (msg[0] == CMD_CHAR) {
								handle_command(td, NULL,
								    name, CMD_CHAT, msg);
							}
						}
					}	
				}
			}
			break;
		default:
			Log(OP_HSMOD, "Unhandled message type");
		}
	}
}

void
pkt_handle_game_0x0A(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 36) {
		if (td->net->state == NS_LOGGINGIN) {
			uint8_t login_response;
			uint32_t server_version;
			uint8_t send_namereg;
			extract_packet(buf, "AACCCCAACCCC",
				NULL,
			    &login_response,
			    &server_version,
			    NULL,
			    NULL,
			    NULL,
			    NULL,
			    &send_namereg,
			    NULL,
			    NULL,
			    NULL,
			    NULL
			    );

			switch (login_response) {
				case 0x01: /* unregistered name */
					pkt_send_login(td->bot_name, td->login->password, g_machineid, 1, g_permissionid);
					break;
				case 0x00: /* login ok */
					if (send_namereg) {
						pkt_send_namereg(td->bot_name, td->bot_email);
						LogFmt(OP_MOD, "Registering new name: %s", td->bot_name);
					}
				case 0x05: /* permission only */
				case 0x06: /* spectate only */
					Log(OP_MOD, "Logged in to server");
					td->net->state = NS_CONNECTED;
					break;
				default:
					StopBotFmt("Unable to log in to server, " \
					    "error code %d", login_response);
			}
		}
	}
}

/* event_change */
void
pkt_handle_game_0x0D(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 6) {
		uint16_t freq;
		uint16_t pid;

		extract_packet(buf, "ABBA",
		    NULL,
		    &pid,
		    &freq,
		    NULL
		    );

		PLAYER *p = player_find_by_pid(td, pid, MATCH_HERE);

		if (p) {
			player_player_change(td, p, freq, p->ship);
		} else {
			Log(OP_HSMOD, "EVENT_CHANGE for nonexistent pid!");
		}
	}
}


/* event_attach/event_detach */
void
pkt_handle_game_0x0E(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 5) {
		uint16_t attacher;
		uint16_t target;

		extract_packet(buf, "ABB",
		    NULL,
		    &attacher,
		    &target
		    );

		PLAYER *p1 = player_find_by_pid(td, attacher, MATCH_HERE);

		CORE_DATA *cd = libman_get_core_data(td);
		if (p1) {
			if (target == PID_NONE) {
				cd->p1 = p1;
				cd->p2 = NULL;		
				libman_export_event(td, EVENT_DETACH, cd);
			} else {
				PLAYER *p2 = player_find_by_pid(td, target, MATCH_HERE);
				if (p2) {
					cd->p1 = p1;
					cd->p2 = p2;
					libman_export_event(td, EVENT_ATTACH, cd);
				} else {
					Log(OP_HSMOD, "Turret packet for nonexistent target pid!");
				}
			}
		} else {
			Log(OP_HSMOD, "Turret packet for nonexistent attacher pid!");
		}
	}
}


void
pkt_handle_game_0x14(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 7) {
		uint16_t freq;
		uint32_t jackpot;

		extract_packet(buf, "ABC",
		    NULL,
		    &freq,
		    &jackpot
		    );

		CORE_DATA *cd = libman_get_core_data(td);

		cd->victory_freq = freq;
		cd->victory_jackpot = jackpot;
		libman_export_event(td, EVENT_FLAG_VICTORY, cd);
	}
}


void
pkt_handle_game_0x19(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 273) {
		char filename[16];
		extract_packet(buf, "AZ",
		    NULL,
		    &filename, 16
		    );
		filename[15] = '\0';

		/* make sure the file to be uploaded was intended to be sent... */
		if (strcmp(filename, td->net->send_file_data->cur_filename) == 0) {
			do_send_file(td);
		} else {
			LogFmt(OP_SMOD, "Ignoring file upload request from server (Expected:%s  Received:)", td->net->send_file_data->cur_filename, filename);
		}
	}
}

/* event_change : XXX: does this ever happen? */
void
pkt_handle_game_0x1C(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 6) {
		uint8_t ship;
		uint16_t pid;
		uint16_t freq;

		extract_packet(buf, "AABB",
		    NULL,
		    &ship,
		    &pid,
		    &freq
		    );

		PLAYER *p = player_find_by_pid(td, pid, MATCH_HERE);
		if (p) {
			player_player_change(td, p, freq, ship);
		} else {
			Log(OP_HSMOD, "EVENT_CHANGE for nonexistent player!");
		}
	}
}

/* event_change */
void
pkt_handle_game_0x1D(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 6) {
		uint16_t freq;
		uint16_t pid;
		uint8_t ship;

		extract_packet(buf, "AABB",
		    NULL,
		    &ship,
		    &pid,
		    &freq
		    );

		PLAYER *p = player_find_by_pid(td, pid, MATCH_HERE);

		if (p) {
			player_player_change(td, p, freq, ship);
		} else {
			Log(OP_HSMOD, "EVENT_CHANGE for nonexistent pid!");
		}
	}
}

void
pkt_handle_game_0x27(THREAD_DATA *td, uint8_t *buf, int len)
{
	pkt_send_position_update(td->bot_pos->x, td->bot_pos->y,
	    td->bot_vel->x, td->bot_vel->y);
	td->net->ticks->last_pos_update_sent = get_ticks_ms();
}

/* handle position update */
/* XXX: why is pid 1 byte? */
void
pkt_handle_game_0x28(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 16) {
		uint16_t x, y, xv, yv;
		PLAYER_ID pid;
		uint8_t status;

		extract_packet(buf, "AABBAAAABBB",
		    NULL,
		    NULL,	/* rotation */
		    NULL,	/* timestamp */
		    &x,		/* x pixels */
		    NULL,
		    NULL,
		    &pid,	/* player id , why is this 8 bytes? */
		    &status,	/* player status */
		    &yv,	/* y velocity */
		    &y,		/* y pixels */
		    &xv		/* x velocity */
		    );

		PLAYER *p = player_find_by_pid(td, pid, MATCH_HERE);
		if (p) {
			p->pos->x = x;
			p->pos->y = y;
			p->x_vel = xv;
			p->y_vel = yv;
			p->status = status;

			CORE_DATA *cd = libman_get_core_data(td);
			cd->p1 = p;
			libman_export_event(td, EVENT_UPDATE, cd);
		} else {
			/* XXX: figure out why this is */
			//Log(OP_HSMOD, "Game packet 0x28 for non-existent pid");
		}

	}
}

void
pkt_handle_game_0x2E(THREAD_DATA *td, uint8_t *buf, int len)
{
	/* XXX: this is not implemented, and is only here
	   to prevent the bot spamming 2E messages every
	   second when 'spew unhandled packets' is set */
}

// clearly this small parsing state machine is ugly but it works
// and is secure.
void
pkt_handle_game_0x2F(THREAD_DATA *td, uint8_t *buf, int len)
{
	// name'\0'<word players>
	int narenas = 0;
	int i = 1;
	int state = 0; // in name
	int namelen = 0;

	// count up the number of arenas
	while (i < len) {
		if (state == 0) {
			++namelen;
			if (namelen > 16) {
				break;
			}
			if (buf[i] == '\0') {
				// end of name
				state = 1;
			}
		} else if (state == 1) {
			state = 2;
		} else if (state == 2) {
			// second count byte
			state = 0;
			namelen = 0;
			++narenas;
		}

		++i;	
	}

	// copy the counted number out
	if (narenas > 0) {
		ARENA_LIST *arena_list = (ARENA_LIST*)xcalloc(narenas, sizeof(ARENA_LIST));
		i = 1;
		namelen = 0;
		narenas = 0;
		state = 0;
		while (i < len) {
			ARENA_LIST tmp;
			if (state == 0) {
				++namelen;
				if (namelen > 16) {
					break;
				}
				if (buf[i] == '\0') {
					// end of name
					state = 1;
				}
				tmp.name[namelen - 1] = buf[i];
			} else if (state == 1) {
				state = 2;
			} else if (state == 2) {
				// second count byte
				int16_t count = *(int16_t*)(&buf[i - 1]);
				tmp.count = count < 0 ? -count : count;
				tmp.current_arena = count < 0 ? 1 : 0;

				state = 0;
				namelen = 0;
				arena_list[narenas] = tmp;
				++narenas;
			}

			++i;	
		}

		/* export arena list event */
		CORE_DATA *cd = libman_get_core_data(td);
		cd->arena_list = arena_list;
		cd->arena_list_count = narenas;
		libman_export_event(td, EVENT_ARENA_LIST, cd);
		free(arena_list);
	}
}


/*
 * Map name packet.
 * Map download packet
 */
void
pkt_handle_game_0x29(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (len >= 16) {
		char map_name[16];
		uint32_t checksum;

		extract_packet(buf, "AzC",
		    NULL,
		    map_name, 16,
		    &checksum
		    );
		map_name[sizeof(map_name) - 1] = '\0';

		char *base = strdup(map_name);
		if (base == NULL) {
			perror("No memory");
			exit(-1);
		}
		basename(map_name);
		char outfile[64];
		snprintf(outfile, sizeof(outfile), "files/%s", base);
		FILE *f = fopen(outfile, "rb");
		if (f == NULL) {
			queue_get_file(td, map_name, NULL);
		} else {
			/* XXX: do checks here (checksum file & and amke sure the checksums match! */
			fclose(f);

			/* simulate the receive so plugins know the arena file is valid */
			simulate_file_received(td, base);
		}
		free(base);
	}

}

void
pkt_handle_game_0x31(THREAD_DATA *td, uint8_t *buf, int len)
{
	if (td->net->state == NS_CONNECTED) {
		go(td, SHIP_SPECTATOR, NULL);

		PubMessage("*relkills 1");

		/* execute auto-run */
		char line[256];
		int delims=DelimCount(td->login->autorun, '\\');
		for (int i=0; i <= delims; ++i) {
			DelimArgs(line, 256, td->login->autorun, i, '\\', false);
			if (strlen(line) > 0) {
				PubMessage(line);
			}
		}

		/* join chats */
		if (strlen(td->login->chats) > 0) {
			delims=DelimCount(td->login->chats, ',');
			for (int i=0; i <= delims && i < MAX_CHATS; ++i) {
				DelimArgs(td->chats->chat[i], 16,
				    td->login->chats, i, ',', false);
			}
			PubMessageFmt("?chat=%s", td->login->chats);
		}

		libman_export_event(td, EVENT_LOGIN, NULL);
	}
}

/*
 * Small helper function for event_message
 */
static
void
export_msg(THREAD_DATA *td, MSG_TYPE msg_type, PLAYER *p, int chatid, char *name, char *msg)
{
	CORE_DATA *cd = libman_get_core_data(td);

	cd->msg_type = msg_type;
	cd->p1 = p;
	cd->msg_name = name;
	cd->msg = msg;
	cd->msg_chatid = chatid;
	cd->msg_chatname = "";
	if (chatid > 0 && chatid <= MAX_CHATS) {
		cd->msg_chatname = td->chats->chat[chatid - 1];
	}

	libman_export_event(td, EVENT_MESSAGE, cd);
}

/*
 * Helper function that simulates a successful file download.
 */
static
void
simulate_file_received(THREAD_DATA *td, char *filename)
{
	CORE_DATA *cd = libman_get_core_data(td);

	cd->transfer_success = 1;
	cd->transfer_filename = filename;
	cd->transfer_direction = TRANSFER_S2C;

	libman_export_event(td, EVENT_TRANSFER, cd);
}

