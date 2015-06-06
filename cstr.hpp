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

#ifndef _CSTR_HPP
#define _CSTR_HPP

#include <stdarg.h>
#include <string.h>

#include "util.hpp"

#define CMPFNC strcmp
#define ICMPFNC strcasecmp

template <int N, int (*cmpfnc)(const char*, const char*) = ICMPFNC>
class Cstr {
public:
	Cstr() {
		clear();
		terminate();
	}

	Cstr(const char *str) {
		strncpy(m_data, str, N-1);
		terminate();
	}

	Cstr(const Cstr &val) {
		strlcpy(m_data, val.c_str(), N);
	}

	Cstr& SetFmt(const char *str, ...) {
		va_list ap;
		va_start(ap, str);
		vsnprintf(m_data, N-1, str, ap);
		va_end(ap);
		return *this;
	}

	Cstr& AppendFmt(const char *str, ...) {
		int len = length();
		if(len < N-1) {
			va_list ap;
			va_start(ap, str);
			vsnprintf(m_data+len, N-1-len, str, ap);
			va_end(ap);
		}
		return *this;
	}

	Cstr& operator=(const char *str) {
		strncpy(m_data, str, N-1);
		return *this;
	}

	Cstr& operator=(const Cstr &val) {
		strlcpy(m_data, val.c_str(), N);
		return *this;
	}

	Cstr& operator+=(const char *str) {
		int len = length();
		if(len < N-1) {
			strncpy(m_data+len, str, N-1-len);
		}
		return *this;
	}

	Cstr& operator+=(const Cstr &val) {
		return *this += val.c_str();
	}

	inline
	char* operator+(int i) {
		return m_data + i;
	}

	Cstr operator+(const char *str) {
		Cstr<N,cmpfnc> result = *this;
		result += str;
		return result;
	}

	Cstr operator+(const Cstr &val) {
		Cstr<N,cmpfnc> result = *this;
		result += val;
		return *this;
	}

	inline
	char& operator[](int i) {
		return m_data[i];
	}

	inline
	char* operator*() {
		return (char*)m_data;
	}

	inline
	bool operator<(const Cstr &rhs) const {
		return cmpfnc(c_str(), rhs.c_str()) < 0;
	}

	inline
	bool operator<=(const Cstr &rhs) const {
		return cmpfnc(c_str(), rhs.c_str()) <= 0;
	}

	inline
	bool operator>(const Cstr &rhs) const {
		return cmpfnc(c_str(), rhs.c_str()) > 0;
	}

	inline
	bool operator>=(const Cstr &rhs) const {
		return cmpfnc(c_str(), rhs.c_str()) >= 0;
	}

	inline
	bool operator==(const Cstr &rhs) const {
		return cmpfnc(c_str(), rhs.c_str()) == 0;
	}

	inline 
	const char* c_str() const {
		return (const char*)m_data;
	}
	
	inline
	int size() const {
		return N;
	}

	inline
	int length() const {
		return strlen(m_data);
	}

	inline
	void clear() {
		*m_data='\0';
	}

	inline
	void terminate() {
		m_data[N-1]='\0';
	}
	
protected:
	char m_data[N];
};

#endif

/* EOF */

