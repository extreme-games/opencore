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


#include <stdio.h>

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <fmt_string>\n", argv[0]);
		return -1;
	}

	char *fmt = argv[1];
	int s = 0;
	while (*fmt) {
		char c = *fmt++;
		switch (c) {
			case 'a':
			case 'A':
				s+=1;
				break;
			case 'b':
			case 'B':
				s+=2;
				break;
			case 'c':
			case 'C':
				s+=4;
				break;
			default:
				printf("'%c': ignored\n", c);
				break;
		}
	}

	printf("\"%s\": %d bytes\n", argv[1], s);

	return 0;
}

