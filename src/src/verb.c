/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

void
verb_add(Object *o, int verb, int prep, int method)
{
    Verbdef *v, *newv, *prev;

    if (!o) {
	return;
    }
    newv = MALLOC(Verbdef, 1);
    newv->verb = verb;
    newv->prep = prep;
    newv->method = method;
    for (v = o->verbs, prev = 0; v; prev = v, v = v->next) {
	if (prep >= 0 && v->verb == verb) {
	    break;
	}
    }
    if (prev) {
	newv->next = prev->next;
	prev->next = newv;
    } else {
	newv->next = o->verbs;
	o->verbs = newv;
    }
}

int
verb_rm(Object *o, const char *verb)
{
    Verbdef	*prev = 0, *v;
    int		 i, verbno = -1;

    if (!o) {
	return 0;
    }
    if (isdigit(verb[0])) {
	verbno = atoi(verb);
    }
    for (v = o->verbs, i = 0; v; prev = v, v = v->next, i++) {
	if (verb_match(sym_get(o, v->verb)->str, verb) || i == verbno) {
	    if (prev) {
		prev->next = v->next;
	    } else {
		o->verbs = v->next;
	    }
	    sym_free(o, v->verb);
	    if (v->prep >= 0) {
		sym_free(o, v->prep);
	    }
	    sym_free(o, v->method);
	    FREE(v);
	    return 1;
	}
    }
    return 0;
}
