/*
 * Copyright (c) 2017, Quarantainenet Holding B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the company nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "inet.h"
#include "utils.h"

char* str_addr(sa_in46* sa)
{
	static char buf[INET6_ADDRSTRLEN]; // max size for ipv6 (incl. 0-char)

	if (sa->any.af == AF_INET)
		inet_ntop(AF_INET, &(sa->ipv4.sin_addr), buf, sizeof(buf));
	else if (sa->any.af == AF_INET6)
		inet_ntop(AF_INET6, &(sa->ipv6.sin6_addr), buf, sizeof(buf));
	else
		strncpy(buf, "[unknown address family]", INET6_ADDRSTRLEN - 1);

	return buf;
}

char* str_addr_port(sa_in46* sa)
{
	static char buf[INET6_ADDRSTRLEN + 8]; // '[' <ipv6> ']:' <n> \0

	if (sa->any.af == AF_INET)
		snprintf(buf, sizeof(buf), "%s:%u", str_addr(sa), ntohs(sa->any.port));
	else if (sa->any.af == AF_INET6)
		snprintf(buf, sizeof(buf), "[%s]:%u", str_addr(sa), ntohs(sa->any.port));
	else
		strncpy(buf, "[unknown address family]", INET6_ADDRSTRLEN + 7);

	return buf;
}

char* str_in_addr(struct in_addr46* addr)
{
	static char buf[INET6_ADDRSTRLEN]; // max size for ipv6 (incl. 0-char)

	if (addr->af == AF_INET)
		inet_ntop(AF_INET, &(addr->in.addr4), buf, sizeof(buf));
	else if (addr->af == AF_INET6)
		inet_ntop(AF_INET6, &(addr->in.addr6), buf, sizeof(buf));
	else
		strncpy(buf, "[unknown address family]", INET6_ADDRSTRLEN - 1);

	return buf;
}

int parse_ipv4(char* text, struct in_addr* ia, int allow_any)
{
	sa_in46 sa;
	sa.any.af = AF_INET;
	int i = parse_ip(text, &sa, allow_any);
	if (i == -1)
		return -1;
	memcpy(ia, &(sa.ipv4.sin_addr), sizeof(struct in_addr));
	return 0;
}

int parse_ipv4_cidr(char* text, struct in_addr* ia, struct in_addr* netmask, int allow_any)
{
	char* idx = strchr(text, '/');
	if (idx == NULL) {
		if ((allow_any) && (strcmp(text, "*") == 0)) {
			ia->s_addr = 0x00000000;
			netmask->s_addr = 0x00000000;
			return 0;
		}

		netmask->s_addr = 0xffffffff;
		return parse_ipv4(text, ia, 0);
	}

	int i;
	if (my_strtol(&idx[1], &i))
		return -1;

	if (i < 0 || i > 32)
		return -1; // should be in [0..32]

	if (i == 0)
		netmask->s_addr = 0x00000000;
	else
		netmask->s_addr = htonl(~((1 << (32 - i)) - 1));

	*idx = 0;
	i = parse_ipv4(text, ia, 0);
	*idx = '/';

	return i;
}

int parse_ipv6(char* text, struct in6_addr* ia, int allow_any)
{
	sa_in46 sa;
	sa.any.af = AF_INET6;
	int i = parse_ip(text, &sa, allow_any);
	if (i == -1)
		return -1;
	memcpy(ia, &(sa.ipv6.sin6_addr), sizeof(struct in6_addr));
	return 0;
}

int parse_ipv6_cidr(char* text, struct in6_addr* ia, struct in6_addr* netmask, int allow_any)
{
	char* idx = strchr(text, '/');
	if (idx == NULL) {
		if ((allow_any) && (strcmp(text, "*") == 0)) {
			memset(ia, 0, sizeof(struct in6_addr));
			memset(netmask, 0, sizeof(struct in6_addr));
			return 0;
		}

		memset(netmask, 0xff, sizeof(struct in6_addr));
		return parse_ipv6(text, ia, 0);
	}

	int i;
	if (my_strtol(&idx[1], &i))
		return -1;

	if (i < 0 || i > 128)
		return -1; // should be in [0..128]

	memset(netmask, 0, sizeof(struct in6_addr));
	int nm_pos = 0;
	while (i >= 8) {
		netmask->s6_addr[nm_pos++] = 0xff;
		i -= 8;
	}
	if (i > 0)
		netmask->s6_addr[nm_pos] = (0xff00 >> i) & 0xff;

	*idx = 0;
	i = parse_ipv6(text, ia, 0);
	*idx = '/';

	return 0;
}

int parse_ip(char* text, union sa_in46* sa, int allow_any)
{
	if ((allow_any) && (strcmp(text, "*") == 0)) {
		if (sa->any.af == AF_UNSPEC)
			sa->any.af = AF_INET6;

		if (sa->any.af == AF_INET)
			sa->ipv4.sin_addr.s_addr = INADDR_ANY;
		else if (sa->any.af == AF_INET6)
			sa->ipv6.sin6_addr = in6addr_any;

		return 0;
	}

	if (sa->any.af == AF_UNSPEC) {
		char* idx = strchr(text, ':');
		sa->any.af = (idx == NULL) ? AF_INET : AF_INET6;
	}

	if (sa->any.af == AF_INET)
		return (inet_pton(AF_INET, text, &(sa->ipv4.sin_addr)) == 1) ? 0 : -1;
	if (sa->any.af == AF_INET6)
		return (inet_pton(AF_INET6, text, &(sa->ipv6.sin6_addr)) == 1) ? 0 : -1;

	return -1;
}

int parse_ip_port(char* text, union sa_in46* sa, int allow_any)
{
	sa->any.af = AF_UNSPEC;

	if (text[0] == '[') { // '[' <ip46-addr> ']' [ ':' <port> ]
		char* idx = strchr(text, ']');
		if (idx == NULL)
			return -1; // '[' mismatch

		*idx = 0;
		int i = parse_ip(&text[1], sa, allow_any);
		*idx = ']';
		if (i == -1)
			return -1;

		if (idx[1] == 0) { // '[' <ip46-addr> ']'
			return 0;
		}

		if (idx[1] == ':') { // '[' <ip46-addr> ']:' <port>
			int i;
			if (my_strtol(&idx[2], &i))
				return -1;
			if (i <= 0 || i >= 65536)
				return -1; // should be in [1..65535]
			sa->any.port = htons(i);
			return 0;
		}

		return -1;
	}

	char* idx = strchr(text, ':');
	if (idx == NULL) { // <ip4-addr>
		sa->any.af = AF_INET;
		return parse_ip(text, sa, allow_any);
	}

	char* idx2 = strchr(&idx[1], ':');
	if (idx2 != NULL) { // <ip6-addr>
		sa->any.af = AF_INET6;
		return parse_ip(text, sa, allow_any);
	}

	// <ip4-addr> ':' <port>
	sa->any.af = AF_INET;
	*idx = 0;
	int i = parse_ip(text, sa, allow_any);
	*idx = ':';
	if (i == -1)
		return -1;

	if (my_strtol(&idx[1], &i))
		return -1;
	if (i <= 0 || i >= 65536)
		return -1; // should be in [1..65535]

	sa->any.port = htons(i);
	return 0;
}
