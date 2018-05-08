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

#ifndef INET_H
#define INET_H

#include "includes.h"

/// \defgroup inet Network address parsing and formatting

/** IPv4/IPv6 address with port
 *
 * The `any.af` and `any.port` should always be sensible to use if the structure
 * is initialized.
 *
 * When `any.af` is set to `AF_INET` then the `ipv4` field can be used.
 *
 * When `any.af` is set to `AF_INET6` then the `ipv6` field can be used.
 */
typedef union sa_in46 {
	struct {
		sa_family_t af;
		in_port_t port;
	} any;
	struct sockaddr_in ipv4;
	struct sockaddr_in6 ipv6;
} sa_in46;

/** IPv4/IPv6 address
 *
 * When the `af` field is set to AF_INET then the `in.addr4` field is to be used.
 *
 * When the `af` field is set to AF_INET6 then the `in.addr6` field is to be used.
 */
struct in_addr46 {
	sa_family_t af;
	union {
		struct in_addr addr4;
		struct in6_addr addr6;
	} in;
};

/** Get an string representation of the IPv4/IPv6 address without port information
 *
 * \note The returned string should not be freed
 * \note This function uses a statically allocated string buffer and the value
 *          of that buffer will change after each invocation.
 *
 * \warning This function is not thread safe.
 *
 * \param sa The address information to get the string respresentation of
 * \returns A pointer to a statically allocated string buffer of at most `INET6_ADDRSTRLEN` length
 * \ingroup inet
 */
extern char* str_addr(sa_in46* sa);

/** Get an string representation of the IPv4/IPv6 address with port information
 *
 * \note The returned string should not be freed
 * \note This function uses a statically allocated string buffer and the value
 *          of that buffer will change after each invocation.
 *
 * \warning This function is not thread safe.
 *
 * \param sa The address information to get the string respresentation of
 * \returns A pointer to a statically allocated string buffer of at most `INET6_ADDRSTRLEN`+8 length
 * \ingroup inet
 */
extern char* str_addr_port(sa_in46* sa);

/** Get an string representation of the IPv4/IPv6 address
 *
 * \note The returned string should not be freed
 * \note This function uses a statically allocated string buffer and the value
 *          of that buffer will change after each invocation.
 *
 * \warning This function is not thread safe.
 *
 * \param sa The address information to get the string respresentation of
 * \returns A pointer to a statically allocated string buffer of at most `INET6_ADDRSTRLEN` length
 * \ingroup inet
 */
extern char* str_in_addr(struct in_addr46* addr);

/** Parse an IPv4 address
 *
 * \param text      The text to parse
 * \param ia        The structure to update with the IP address
 * \param allow_any Whether to allow INADDR_ANY address (0.0.0.0) to be parsed
 * \returns 0 on success or -1 on error
 * \ingroup inet
 */
extern int parse_ipv4(char* text, struct in_addr* ia, int allow_any);

/** Parse an IPv4 cidr
 *
 * \param text      The text to parse
 * \param ia        The structure to update with the IP address part
 * \param netmask   The structure to update with the netmask part
 * \param allow_any Whether to allow INADDR_ANY address (0.0.0.0) to be parsed
 * \returns The cidr mask (range [0..32]) or -1 on error
 * \ingroup inet
 */
extern int parse_ipv4_cidr(char* text, struct in_addr* ia, struct in_addr* netmask, int allow_any);

/** Parse an IPv6 cidr
 *
 * \param text      The text to parse
 * \param ia        The structure to update with the IP address part
 * \param allow_any Whether to allow "any" address (::) to be parsed
 * \returns 0 on success or -1 on error
 * \ingroup inet
 */
extern int parse_ipv6(char* text, struct in6_addr* ia, int allow_any);

/** Parse an IPv6 cidr
 *
 * \param text      The text to parse
 * \param ia        The structure to update with the IP address part
 * \param netmask   The structure to update with the netmask part
 * \param allow_any Whether to allow "any" address (::) to be parsed
 * \returns The cidr mask (range [0..128]) or -1 on error
 * \ingroup inet
 */
extern int parse_ipv6_cidr(char* text, struct in6_addr* ia, struct in6_addr* netmask, int allow_any);

/** Parse an IPv4 or IPv6 address
 *
 * \param text      The text to parse
 * \param sa        The structure to update with the address
 * \param allow_any Whether to allow "any" address (::) to be parsed
 * \returns 0 on success or -1 on error
 * \ingroup inet
 */
extern int parse_ip(char* text, union sa_in46* sa, int allow_any);

/** Parse an IPv4 or IPv6 address with port number
 *
 * \param text      The text to parse
 * \param sa        The structure to update with the address and port
 * \param allow_any Whether to allow "any" address (::) to be parsed
 * \returns 0 on success or -1 on error
 * \ingroup inet
 */
extern int parse_ip_port(char* text, union sa_in46* sa, int allow_any);

#endif /* INET_H */
