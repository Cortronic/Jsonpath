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

%token_type {struct jp_opcode *}
%extra_argument {struct jp_state *s}

%left T_AND.
%left T_OR.
%left T_UNION.
%nonassoc T_EQ T_NE T_GT T_GE T_LT T_LE.
%right T_NOT.

%include {
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "jsonpath.h"
#include "parser.h"
}

%syntax_error {
    int i;

    for (i = 0; i < sizeof(tokennames) / sizeof(tokennames[0]); i++)
        if (yy_find_shift_action(yypParser, (YYCODETYPE)i) < YYNSTATE + YYNRULE)
            s->error_code |= (1 << i);

    s->error_pos = s->off;
}

input ::= expr(A).                                  { s->path = A->down; }

expr(A) ::= T_LABEL(B) T_EQ path(C).                { A = B; A->down = C; }
expr(A) ::= path(B).                                { A = B; }

path(A) ::= T_ROOT|T_THIS(B) segments(C).           { A = B; A->down = C; }
path(A) ::= T_ROOT|T_THIS(B).                       { A = B; }

segments(A) ::= segments(B) segment(C).             { A = jp_append_op(B, C); }
segments(A) ::= segment(B).                         { A = B; }

segment(A) ::= T_DEEP(B) segment(C).                { A = jp_append_op(B, C); }
segment(A) ::= T_DEEP(B) T_LABEL|T_WILDCARD(C).     { A = jp_append_op(B, C); }
segment(A) ::= T_DOT T_LABEL|T_WILDCARD(B).         { A = B; }
// segment(A) ::= T_BROPEN T_NUMBER(B) T_BRCLOSE.      { A = B; }
segment(A) ::= T_BROPEN union_exps(B) T_BRCLOSE.    { A = B; }

// Array slices
segment(A) ::= T_BROPEN slice_step(B) T_BRCLOSE.    { A = B; }
slice_step(A) ::= slice_stop(B) T_SLICE T_NUMBER(C).{ A = B; A->down->sibling->sibling = C; }
slice_step(A) ::= slice_stop(B) T_SLICE.            { A = B; }
slice_step(A) ::= slice_stop(B).                    { A = B; }
slice_stop(A) ::= slice_sta(B) T_NUMBER(C).         { A = B; A->down->sibling = C; }
slice_stop(A) ::= slice_sta(B).                     { A = B; A->down->sibling = jp_alloc_op(s, T_NUMBER, INT_MAX, NULL, NULL); }
slice_sta(A) ::= T_NUMBER(B) T_SLICE(C).            { A = C; A->down = B; }
slice_sta(A) ::= T_SLICE(B).                        { A = B; A->down = jp_alloc_op(s, T_NUMBER, 0, NULL, NULL); }

// Array unions
segment(A) ::= T_BROPEN union_end(B) T_BRCLOSE.     { A = B; }
union(A) ::= union(B) T_NUMBER(C) T_UNION.          { A = B; jp_append_op(A->down, C); }
union_end(A) ::= union(B) T_NUMBER(C).              { A = B; jp_append_op(A->down, C); }
union_end(A) ::= union(B).                          { A = B; }
union(A) ::= T_NUMBER(B) T_UNION(C).                { A = C; A->down = B; }

// ++
union_exps(A) ::= T_FILTER or_exps(B).              { A = B; }
union_exps(A) ::= or_exps(B).                       { A = B; }

//--
//union_exps(A) ::= T_FILTER union_exps(B).           { A = B; }
//union_exps(A) ::= union_exp(B).                     { A = B; A = B->sibling ? jp_alloc_op(s, T_UNION, 0, NULL, B, NULL) : B; }

//union_exp(A) ::= union_exp(B) T_UNION or_exps(C).   { A = jp_append_op(B, C); }
//union_exp(A) ::= or_exps(B).                        { A = B; }

or_exps(A) ::= or_exp(B).                           { A = B->sibling ? jp_alloc_op(s, T_OR, 0, NULL, B, NULL) : B; }

or_exp(A) ::= or_exp(B) T_OR and_exps(C).           { A = jp_append_op(B, C); }
or_exp(A) ::= and_exps(B).                          { A = B; }

and_exps(A) ::= and_exp(B).                         { A = B->sibling ? jp_alloc_op(s, T_AND, 0, NULL, B, NULL) : B; }

and_exp(A) ::= and_exp(B) T_AND cmp_exp(C).         { A = jp_append_op(B, C); }
and_exp(A) ::= cmp_exp(B).                          { A = B; }

cmp_exp(A) ::= unary_exp(B) T_LT|T_LE|T_GT|T_GE|T_EQ|T_NE(C) unary_exp(D). { 
    A = C; 
    A->down = B; 
    while (B->sibling) B = B->sibling; 
    B->sibling = D; 
}
cmp_exp(A) ::= unary_exp(B).                    { A = B; }

unary_exp(A) ::= T_BOOL|T_STRING|T_WILDCARD(B). { A = B; }
unary_exp(A) ::= T_NUMBER(B).                   { A = B; }
unary_exp(A) ::= T_POPEN or_exps(B) T_PCLOSE.   { A = B; }
unary_exp(A) ::= T_NOT(B) unary_exp(C).         { A = B; A->down = C; }
unary_exp(A) ::= path(B).                       { A = B; }
