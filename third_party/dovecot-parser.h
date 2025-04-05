/*
 * Copyright (c) 2002-2018 Dovecot authors
 * Copyright (c) 2015-2018 Pali <pali@cpan.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef DOVECOT_PARSER_H
#define DOVECOT_PARSER_H

#include <stdbool.h>
#include <stddef.h>

enum message_address_parse_flags {
	/* If enabled, missing mailbox and domain are set to MISSING_MAILBOX
	   and MISSING_DOMAIN strings. Otherwise they're set to "". */
	MESSAGE_ADDRESS_PARSE_FLAG_FILL_MISSING = (1 << 0),
	/* Require local-part to strictly adhere to RFC5322 when parsing dots.
	   For example ".user", "us..ser" and "user." will be invalid. This
	   isn't enabled by default, because these kind of invalid addresses
	   are commonly used in Japan. */
	MESSAGE_ADDRESS_PARSE_FLAG_STRICT_DOTS = (1 << 1),
	/* Same as MESSAGE_ADDRESS_PARSE_FLAG_STRICT_DOTS, but accept also
	   non-strict input. Flag invalid_syntax will be set to true. */
	MESSAGE_ADDRESS_PARSE_FLAG_NON_STRICT_DOTS_AS_INVALID = (1 << 2),
};

/* group: ... ; will be stored like:
   {name = NULL, NULL, "group", NULL}, ..., {NULL, NULL, NULL, NULL}
*/
struct message_address {
	struct message_address *next;

	/* display-name */
	char *name;
	size_t name_len;
	/* route string contains the @ prefix */
	char *route;
	size_t route_len;
	/* local-part */
	char *mailbox;
	size_t mailbox_len;
	char *domain;
	size_t domain_len;
	char *comment;
	size_t comment_len;
	char *original;
	size_t original_len;
	/* there were errors when parsing this address */
	bool invalid_syntax;
};

/* Parse message addresses from given data. Note that giving an empty string
   will return NULL since there are no addresses. */
struct message_address *
message_address_parse(const char *str, size_t len, unsigned int max_addresses, enum message_address_parse_flags flags);

void message_address_add(struct message_address **first, struct message_address **last,
			 const char *name, size_t name_len, const char *route, size_t route_len,
			 const char *mailbox, size_t mailbox_len, const char *domain, size_t domain_len,
			 const char *comment, size_t comment_len);

void message_address_free(struct message_address **addr);

void message_address_write(char **str, size_t *len, const struct message_address *addr);

void compose_address(char **output, size_t *output_len, const char *mailbox, size_t mailbox_len, const char *domain, size_t domain_len);
void split_address(const char *input, size_t input_len, char **mailbox, size_t *mailbox_len, char **domain, size_t *domain_len);

void string_free(char *string);

#endif

