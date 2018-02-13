/*
 * Copyright (C) 2017-2018 Cor Loef        <cortronic@googlemail.com>
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

#include "json/json.h"
#include "jsonpath.h"

Json::Value*
jp_match_next( struct jp_opcode* ptr,
               Json::Value* root,
               Json::Value* cur,
               Json::Value* result,
               bool deep = false );

//---------------------------------------------------------------------------

Json::Value*
jp_match( struct jp_opcode* path,
          Json::Value* jsobj,
          Json::Value* result ) {

    jp_match_next( path, jsobj, jsobj, result );

    if ( result->size() )
        return &((*result)[0]);

    return NULL;
}
//---------------------------------------------------------------------------

static bool
jp_json_to_op( Json::Value* obj, struct jp_opcode* opcode ) {

    switch ( obj->type() ) {

    case Json::booleanValue:
        opcode->type = T_BOOL;
        opcode->num = obj->asBool();
        return true;

    case Json::intValue:
    case Json::uintValue:
        opcode->type = T_NUMBER;
        opcode->num  = obj->asInt();
        return true;

    case Json::stringValue:
        opcode->type = T_STRING;
        opcode->str  = (char*)obj->asCString();
        return true;

    default:
        return false;
    }
}
//---------------------------------------------------------------------------

static bool
jp_resolve( Json::Value* cursor,
            struct jp_opcode* opcode,
            struct jp_opcode* result ) {
				
    Json::Value* val = jp_match_next( opcode->down, cursor, cursor, NULL );

    if ( val && val->type() != Json::nullValue ) {
        return jp_json_to_op( val, result );
    }
    return false;
}
//---------------------------------------------------------------------------

static bool
jp_resolve( Json::Value* root,
            Json::Value* cursor,
            struct jp_opcode* opcode,
            struct jp_opcode* result ) {

    switch ( opcode->type ) {

    case T_THIS:
        return jp_resolve( cursor, opcode, result );

    case T_ROOT:
        return jp_resolve( root, opcode, result );

    default:
        *result = *opcode;
        return true;
    }
}
//---------------------------------------------------------------------------

static bool
jp_cmp( struct jp_opcode* op,
        Json::Value* root,
        Json::Value* cur ) {
        
    int delta;
    struct jp_opcode left, right;

    if ( !jp_resolve(root, cur, op->down, &left ) ||
            !jp_resolve(root, cur, op->down->sibling, &right ) )
        return false;

    if ( right.type == T_WILDCARD ) {
        return true;
    } else if ( left.type != right.type ) {
        return false;
    }

    switch ( left.type ) {

    case T_BOOL:
    case T_NUMBER:
        delta = left.num - right.num;
        break;

    case T_STRING:
        delta = strcmp(left.str, right.str);
        break;

    default:
        return false;
    }

    switch ( op->type ) {

    case T_EQ:
        return (delta == 0);

    case T_LT:
        return (delta < 0);

    case T_LE:
        return (delta <= 0);

    case T_GT:
        return (delta > 0);

    case T_GE:
        return (delta >= 0);

    case T_NE:
        return (delta != 0);

    default:
        return false;
    }
}
//---------------------------------------------------------------------------

static bool
jp_expr( struct jp_opcode* op,
         Json::Value* root,
         Json::Value* cur,
         const char* key,
         int idx ) {
    struct jp_opcode* sop;

    switch ( op->type ) {

    case T_WILDCARD:
        return true;

    case T_EQ:
    case T_NE:
    case T_LT:
    case T_LE:
    case T_GT:
    case T_GE:
        return jp_cmp(op, root, cur );

    case T_ROOT:
        return !!jp_match_next(op->down, root, root, NULL);

    case T_THIS:
        return !!jp_match_next(op->down, cur, cur, NULL);

    case T_NOT:
        return !jp_expr(op->down, root, cur, key, idx);

    case T_AND:
        for (sop = op->down; sop; sop = sop->sibling)
            if (!jp_expr(sop, root, cur, key, idx))
                return false;
        return true;

    case T_OR:
    case T_UNION:
        for (sop = op->down; sop; sop = sop->sibling)
            if (jp_expr(sop, root, cur, key, idx) )
                return true;
        return false;

    case T_LABEL:
    case T_STRING:
        return (key && !strcmp(op->str, key));

    case T_NUMBER:
        return (idx == op->num);

    default:
        return false;
    }
}
//---------------------------------------------------------------------------

static Json::Value*
jp_match_expr( struct jp_opcode *ptr,
               Json::Value* root,
               Json::Value* cur,
               Json::Value* result,
               bool deep ) {
    int idx;
    Json::ValueIterator it;
    Json::Value *tmp = NULL, *res = NULL;

    switch ( cur->type() ) {

    case Json::objectValue:

        for ( it = cur->begin(); it != cur->end(); it++ ) {

            if ( it->isObject() || it->isArray() ) {
                if ( !res && jp_expr( ptr, root, &*it, it.memberName(), -1 ) ) {
                    tmp = jp_match_next( ptr->sibling, root, &*it, result );
                }
            }
            if ( tmp && !res ) {
               res = tmp;
            }

        }
        break;

    case Json::arrayValue:

        for (idx = 0, it = cur->begin(); it != cur->end(); idx++, it++) {

            if ( jp_expr(ptr, root, &*it, NULL, idx)) {
                tmp = jp_match_next(ptr->sibling, root, &*it, result);
            }
            if ( tmp && !res ) {
                res = tmp;
            }
        }
        break;

    default:
        break;
    }

    if (deep) {
        for (it = cur->begin(); it != cur->end(); it++) {
            if (it->isObject() || it->isArray()) {
                jp_match_expr(ptr, root, &*it, result, deep);
            }
        }
    }

    return res;
}
//---------------------------------------------------------------------------

static Json::Value*
jp_match_string( struct jp_opcode *ptr,
                 Json::Value* root,
                 Json::Value* cursor,
                 Json::Value* result,
                 bool deep ) {
    Json::Value *res = NULL;

    if (cursor) {

        if (cursor->isObject()) {
            Json::Value* next = &((*cursor)[ptr->str]);

            if (!next->empty()) {
                res = jp_match_next(ptr->sibling, root, next, result);
            }
        }

        if (deep && cursor->isObject() || cursor->isArray()) {
            Json::ValueIterator it;

            for (it = cursor->begin(); it != cursor->end(); it++) {
                if ( it->isObject() || it->isArray()) {
                    jp_match_string(ptr, root, &*it, result, deep);
                }
            }
        }
    }
    return res;
}
//---------------------------------------------------------------------------

static Json::Value*
jp_match_union( struct jp_opcode *ptr,
                Json::Value* root,
                Json::Value* cursor,
                Json::Value* result,
                bool deep ) {
    Json::Value *res = NULL;

    if (cursor && cursor->isArray()) {
        Json::Value* tmp;
        struct jp_opcode* sibling = ptr->down;

        while (sibling) {
            if (sibling->num >= 0 && sibling->num < (int)cursor->size()) {
                tmp = jp_match_next(ptr->sibling, root, &(*cursor)[sibling->num], result);
                if (tmp && !res) {
                    res = tmp;
                }
            }
            sibling = sibling->sibling;
        }
    }

    if (deep && (cursor->isObject() || cursor->isArray())) {
        Json::ValueIterator it;

        for (it = cursor->begin(); it != cursor->end(); it++) {
            if (it->isObject() || it->isArray()) {
                 jp_match_union(ptr, root, &*it, result, deep);
            }
        }
    }
    return res;
}
//--------------------------------------------------------------------------

static Json::Value*
jp_match_slice( struct jp_opcode *ptr,
                Json::Value* root,
                Json::Value* cursor,
                Json::Value* result,
                bool deep ) {
    Json::Value *res = NULL;

    if (cursor && cursor->isArray()) {
        Json::Value* tmp;
        int start = ptr->down->num;
        int stop  = ptr->down->sibling->num;
        int step  = 0;

        if (ptr->down->sibling->sibling && ptr->down->sibling->sibling->num) {
            step = ptr->down->sibling->sibling->num;
        }

        if (step == 0) {
            if (start <= stop) {
                step = 1;
            } else {
                step = -1;
            }
        }

        if (stop == INT_MAX) {
            if (step > 0) {
                stop = cursor->size();
            } else {
                stop = -1;
            }
        }

        if ((start >= 0 || stop >= 0) &&
                ((start < stop && step > 0) ||
                 (start > stop && step < 0))) {

            for (int i = start;
                    (step > 0 && i < stop) || (step < 0 && i > stop);
                    i +=  step ) {
                if (i >= 0 && i < (int)cursor->size()) {
                    tmp = jp_match_next(ptr->sibling, root, &(*cursor)[i], result);
                    if (tmp && !res){
                        res = tmp;
                    }
                }
            }
        }
    }

    if (deep && (cursor->isObject() || cursor->isArray())) {
        Json::ValueIterator it;

        for (it = cursor->begin(); it != cursor->end(); it++) {
            if (it->isObject() || it->isArray()) {
                 jp_match_slice(ptr, root, &*it, result, deep);
            }
        }
    }
    return res;
}
//--------------------------------------------------------------------------

static Json::Value*
jp_match_number( struct jp_opcode *ptr,
                 Json::Value* root,
                 Json::Value* cursor,
                 Json::Value* result,
                 bool deep ) {
    Json::Value *res = NULL;

    if (cursor) {

        if (cursor->isArray()) {
            int idx = ptr->num;
            Json::Value* next = NULL;

            if ( idx < 0 )
                idx += cursor->size();
            if ( idx >= 0 )
                next = &((*cursor)[idx]);
            if ( next )
                res = jp_match_next(ptr->sibling, root, next, result );
        }

        if (deep && ( cursor->isObject() || cursor->isArray())) {
            Json::ValueIterator it;

            for (it = cursor->begin(); it != cursor->end(); it++) {
                if (it->isObject() || it->isArray()) {
                    jp_match_number(ptr, root, &*it, result, deep);
                }
            }
        }
    }
    return res;
}
//--------------------------------------------------------------------------

static Json::Value*
jp_match_any( struct jp_opcode *ptr,
              Json::Value* root,
              Json::Value* cursor,
              Json::Value* result,
              bool deep ) {

    if (ptr->sibling == NULL) {
         return jp_match_next(ptr->sibling, root, cursor, result, deep );

    } else if (cursor) {
        if (cursor->isObject() || cursor->isArray()) {
            Json::ValueIterator it;

            for (it = cursor->begin(); it != cursor->end(); it++) {
                jp_match_next(ptr->sibling, root, &*it, result, deep);
            }
        }
    }
    return NULL;
}
//---------------------------------------------------------------------------

static Json::Value*
jp_match_next( struct jp_opcode* ptr,
               Json::Value* root,
               Json::Value* cursor,
               Json::Value* result,
               bool deep ) {

    if (!ptr) {
        if (result && result->isArray()) {
            result->append(*cursor);
        }
        return cursor;
    }

    switch ( ptr->type ) {

    case T_WILDCARD:
        return jp_match_any(ptr, root, cursor, result, deep );

    case T_STRING:
    case T_LABEL:
         return jp_match_string(ptr, root, cursor, result, deep);

    case T_NUMBER:
         return jp_match_number(ptr, root, cursor, result, deep);

    case T_UNION:
         return jp_match_union(ptr, root, cursor, result, deep);

    case T_SLICE:
         return jp_match_slice(ptr, root, cursor, result, deep);

    case T_DEEP:
        return jp_match_next(ptr->sibling, root, cursor, result, true);

    default:
        return jp_match_expr(ptr, root, cursor, result, deep);
    }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------











