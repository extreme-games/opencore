/*
 * These functions were ported from twcore.
 */

/* FIXME: the memcpy() calls here are unnecessary, but i could not cast
 * and then use the assignment operators. doing this gave an undesired
 * result. maybe an amd64 issue.
 */
/*
 * NOTE: the memcpy() calls are removed, but i am not sure how
 * this would work on x64.
 */

#include "encrypt.hpp"

void
init_encryption(THREAD_DATA *td)
{
	THREAD_DATA::net_t::encrypt_t *e = td->net->encrypt;

	bzero(e->table, 520);

	int64_t t = e->server_key, o=0;
	for (int i = 0; i < 520/2; ++i) {
		o = t;
		t = ((o * 0x834E0B5FL) >> 48) & 0xFFFFFFFFL;
		t = (t + (t >> 31)) & 0xFFFFFFFFL;
		t = (((o % 0x1F31DL) * 16807) - (t * 2836) + 123) & 0xFFFFFFFFL;

		if (t > 0x7FFFFFFFL) {
			t = (t + 0x7FFFFFFFL) & 0xFFFFFFFFL;
		}

		*(int16_t*)&e->table[i*2] = t;
	}
}

void
encrypt_buffer(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t::encrypt_t *e = td->net->encrypt;

	int64_t tempInt;
	int64_t tempKey = e->server_key;

	int i;
	for (i = 0; i < len-3; i += 4) {
		tempInt = (*(int32_t*)&buf[i]) ^ (*(int32_t*)&e->table[i]) ^ tempKey;
		tempKey = tempInt;
		*(int32_t*)&buf[i] = tempInt & 0xFFFFFFFF;
	}

	if (i < len) {
		int64_t trailKey = (*(int32_t*)&buf[i]) ^ (*(int32_t*)&e->table[i]) ^ tempKey;
		//memcpy(&buf[i], &trailKey, len-i);
		for (int j = 0; j < len - i; ++j) {
			buf[i + j] = ((uint8_t*)&trailKey)[j];
		}
	}
}

void
decrypt_buffer(THREAD_DATA *td, uint8_t *buf, int len)
{
	THREAD_DATA::net_t::encrypt_t *e = td->net->encrypt;

	int64_t tempInt;
	int64_t tempKey = e->server_key;

	int i;
	for (i = 0; i < len-3; i += 4) {
		tempInt = (*(int32_t*)&e->table[i]) ^ tempKey ^ (*(int32_t*)&buf[i]);
		tempKey = (*(int32_t*)&buf[i]);
		*(int32_t*)&buf[i] = tempInt & 0xFFFFFFFF;
	}

	if (i < len) {
		int64_t trailKey = (*(int32_t*)&e->table[i]) ^ tempKey ^ (*(int32_t*)&buf[i]);
		//memcpy(&buf[i], &trailKey, len-i);
		for (int j = 0; j < len - i; ++j) {
			buf[i + j] = ((uint8_t*)&trailKey)[j];
		}
	}
}

