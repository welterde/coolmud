/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "db_setup.h"

static void	unpack_methods(Object *o, FILE *f);
static Verbdef *unpack_verbs(FILE *f);
static void	unpack_gvars(Object *o, FILE *f);
static Vardef  *unpack_vars(FILE *f);
static void	unpack_symbols(Object *o, FILE *f);
static Lock    *unpack_locks(FILE *f);
static Var	unpack_var(FILE *f);
static String  *unpack_string(FILE *f);
static List    *unpack_list(FILE *f);
static Map   *unpack_map(FILE *f);

#define UNPACK(N, f)	fread( (void *) &(N), sizeof(N), 1, f)

Object *
unpack_object(FILE *f)
{
    Object	*o = new_object();

    UNPACK(o->id.id, f);
    UNPACK(o->id.server, f);
    o->parents = unpack_list(f);
    unpack_symbols(o, f);
    unpack_methods(o, f);
    o->verbs = unpack_verbs(f);
    unpack_gvars(o, f);
    o->locks = unpack_locks(f);
    return o;
}

static void
unpack_methods(Object *o, FILE *f)
{
    int		 i, nmethods;
    Method	*m;
    unsigned char	flag;
   
    UNPACK(nmethods, f);
    o->methods = hash_new(nmethods / HASH_INIT_LOAD);
    for (i = 0; i < nmethods; i++) {
	m = MALLOC(Method, 1);
	UNPACK(m->name, f);
	UNPACK(flag, f);
	m->blocked = flag;
	UNPACK(m->ninst, f);
	fread((void *) m->ehandler, sizeof(m->ehandler[0]), NERRS, f);
	m->vars = unpack_vars(f);
	if (m->ninst > 0) {
	    m->code = MALLOC(Inst, m->ninst);
	    fread( (void *) m->code, sizeof(Inst), m->ninst, f);
	} /* if */
	m->ref = 1;
	m->next = 0;
	add_method(o, m);
    } /* for */
}

static Verbdef *
unpack_verbs(FILE *f)
{
    int		 nverbs;
    Verbdef	*vb, *prev = 0, *head = 0;

    UNPACK(nverbs, f);
    while (nverbs--) {
	vb = MALLOC(Verbdef, 1);
	UNPACK(vb->verb, f);
	UNPACK(vb->prep, f);
	UNPACK(vb->method, f);
	vb->next = 0;
	if (prev) {
	    prev->next = vb;
	} else {
	    head = vb;
	}
	prev = vb;
    }
    return head;
}

static void
unpack_gvars(Object *o, FILE *f)
{
    int		 nvars, name;

    UNPACK(nvars, f);
    o->vars = hash_new(nvars / HASH_INIT_LOAD);
    while (nvars--) {
	UNPACK(name, f);
	var_add_global(o, name, unpack_var(f));
    }
}

static Vardef *
unpack_vars(FILE *f)
{
    int		 nvars = 0;
    Vardef	*v, *prev = 0, *head = 0;

    UNPACK(nvars, f);
    while(nvars--) {
	v = MALLOC(Vardef, 1);
	UNPACK(v->name, f);
	v->value = unpack_var(f);
	v->next = 0;
	if (prev) {
	    prev->next = v;
	} else {
	    head = v;
	}
	prev = v;
    }
    return head;
}

static Var
unpack_var(FILE *f)
{
    Var		v;

    v.type = (Type_spec) fgetc(f);
    switch (v.type) {
      case STR:
	v.v.str = unpack_string(f);
	break;
      case NUM:
	UNPACK(v.v.num, f);
	break;
      case OBJ:
	UNPACK(v.v.obj.id, f);
	UNPACK(v.v.obj.server, f);
	break;
      case LIST:
	v.v.list = unpack_list(f);
	break;
      case MAP:
	v.v.map = unpack_map(f);
	break;
      case ERR:
	UNPACK(v.v.err, f);
	break;
      case PC:				/* should never happen */
	break;
    }
    return v;
}

static String *
unpack_string(FILE *f)
{
    String	*s;
    int		 len;

    UNPACK(len, f);
    s = string_new(len + 1);
    fread(s->str, sizeof(char), len, f);
    s->str[len] = '\0';
    s->len = len;
    return s;
}

static List *
unpack_list(FILE *f)
{
    int		i;
    List       *list = MALLOC(List, 1);

    UNPACK(list->len, f);
    list->mem = list->len;
    if (list->len > 0) {
	list->el = MALLOC(Var, list->len);
    }
    for (i = 0; i < list->len; i++) {
	list->el[i] = unpack_var(f);
    }
    list->ref = 1;
    return list;
}

static Map *
unpack_map(FILE *f)
{
    int		 i, nels;
    Map		*map;
    Var		 from, to;

    UNPACK(nels, f);
    map = map_new(nels);
    for (i = 0; i < nels; i++) {
	from = unpack_var(f);
	to = unpack_var(f);
	map = map_add(map, from, to);
    }
    return map;
}

static void
unpack_symbols(Object *o, FILE *f)
{
    int		i;

    UNPACK(o->nsymb, f);
    UNPACK(o->st_size, f);
    if (o->st_size > 0) {
	o->symbols = MALLOC(Symbol, o->st_size);
    }
    for (i = 0; i < o->nsymb; i++) {
	UNPACK(o->symbols[i].ref, f);
	if (o->symbols[i].ref) {
	    o->symbols[i].s = unpack_string(f);
	}
    }
}

static Lock *
unpack_locks(FILE *f)
{
    Lock	*l, *prev = 0, *head = 0;
    int		 nlocks = 0;

    UNPACK(nlocks, f);
    while (nlocks--) {
	l = MALLOC(Lock, 1);
	l->name = unpack_string(f);
	UNPACK(l->added_by, f);
	l->next = 0;
	if (prev) {
	    prev->next = l;
	} else {
	    head = l;
	}
	prev = l;
    }
    return head;
}
