/*
 * Copyright (C) 2013-2014 Jo-Philipp Wich <jo@mein.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __JSONPATH_H_
#define __JSONPATH_H_

#include <stddef.h>

#include "lemon/parser.h"
#include "lexer.h"
#include "matcher.h"

struct jp_opcode {
	int type;
	struct jp_opcode* next;
	struct jp_opcode* sibling;
    struct jp_opcode* down;
	char* str;
	int   num;
};

struct jp_state {
	struct jp_opcode *pool;
	struct jp_opcode *path;
	int error_pos;
	int error_code;
	int off;
};

struct jp_opcode* jp_append_op(struct jp_opcode *a, struct jp_opcode *b);
struct jp_opcode* jp_alloc_op(struct jp_state *s, int type, int num, char *str, ...);
struct jp_state*  jp_parse(const char *expr);
void              jp_free(struct jp_state *s);

void* ParseAlloc(void *(*mfunc)(size_t));
void  Parse(void *pParser, int type, struct jp_opcode *op, struct jp_state *s);
void  ParseFree(void *pParser, void (*ffunc)(void *));

#endif /* __JSONPATH_H_ */
