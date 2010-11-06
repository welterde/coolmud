/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

static int	size_methods(HashT *methods);
static int	size_verbs(Verbdef *vb);
static int	size_gvars(HashT *vars);
static int	size_vars(Vardef *vd);
static int	size_var(Var v);
static int	size_list(List *list);
static int	size_map(Map *map);
static int	size_symbols(Object *o);
static int	size_locks(Lock *l);

#define size_string(S) (sizeof((S)->len) + (S)->len)

int
size_object(Object *o)
{
    int		sz = 0;

    sz += sizeof(o->id.id) + sizeof(o->id.server);
    sz += size_list(o->parents);
    sz += size_symbols(o);
    sz += size_methods(o->methods);
    sz += size_gvars(o->vars);
    sz += size_verbs(o->verbs);
    sz += size_locks(o->locks);
    return sz;
}

static int
size_methods(HashT *methods)
{
    int		hval, sz = sizeof(int);		/* for nmethods */
    Method	*m;

    if (methods) {
	for (hval = 0; hval < methods->size; hval++) {
	    for (m = (Method *) methods->table[hval]; m; m = m->next) {
		sz += sizeof(m->name);
		sz += sizeof(unsigned char);	/* for blocked flag */
		sz += sizeof(m->ninst) + m->ninst * sizeof(Inst);
		sz += sizeof(int);	/* for nvars */
		sz += NERRS * sizeof(enum eh) + size_vars(m->vars);
	    }
	}
    }
    return sz;
}

static int
size_verbs(Verbdef *vb)
{
    int		sz = sizeof(int);	/* for nverbs */

    for (; vb; vb = vb->next) {
	sz += 3 * sizeof(int);
    }
    return sz;
}

static int
size_vars(Vardef *vd)
{
    int		sz = 0;

    for (; vd; vd = vd->next) {
	sz += sizeof(vd->name) + size_var(vd->value);
    }
    return sz;
}

static int
size_gvars(HashT *vars)
{
    int		sz = sizeof(int);	/* for nvars */
    int		hval;

    if (vars) {
	for (hval = 0; hval < vars->size; hval++) {
	    sz += size_vars(vars->table[hval]);
	}
    }
    return sz;
}
    
static int
size_var(Var v)
{
    switch (v.type) {
      case STR:
	return size_string(v.v.str) + 1;
      case NUM:
	return sizeof(v.v.num) + 1;
      case OBJ:
	return sizeof(v.v.obj.id) + sizeof(v.v.obj.server) + 1;
      case LIST:
	return size_list(v.v.list) + 1;
      case MAP:
	return size_map(v.v.map) + 1;
      case ERR:
	return sizeof(v.v.err) + 1;
      case PC:				/* should never happen */
	return 0;
    }
    return 0;		/* should never happen */
}

static int
size_list(List *list)
{
    int		i, sz = sizeof(list->len);	/* size of nels */

    for (i = 0; i < list->len; i++) {
	sz += size_var(list->el[i]);
    }
    return sz;
}

static int
size_map(Map *map)
{
    int		 i, sz = sizeof(map->num);	/* size of nels */
    MapPair	*pair;

    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    sz += size_var(pair->from);
	    sz += size_var(pair->to);
	}
    }
    return sz;
}

static int
size_symbols(Object *o)
{
    int		i, sz = sizeof(o->nsymb) + sizeof(o->st_size);

    for (i = 0; i < o->nsymb; i++) {
	sz += sizeof(o->symbols[i].ref);
	if (o->symbols[i].ref) {
	    sz += size_string(o->symbols[i].s);
	}
    }
    return sz;
}

static int
size_locks(Lock *l)
{
    int		sz = sizeof(int);	/* for nlocks */

    for (; l; l = l->next) {
	sz += size_string(l->name);
	sz += sizeof(l->added_by);
    }
    return sz;
}
