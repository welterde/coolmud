/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "db_setup.h"

#define PACK(N, f)	fwrite( (char *) &(N), sizeof(N), 1, f)

static void	pack_methods(HashT *methods, FILE *f);
static void	pack_verbs(Verbdef *vbhead, FILE *f);
static void	pack_gvars(HashT *vars, FILE *f);
static void	pack_vars(Vardef *vhead, FILE *f);
static void	pack_symbols(Object *o, FILE *f);
static void	pack_locks(Lock *lhead, FILE *f);
static void	pack_var(Var v, FILE *f);
static void	pack_string(String *s, FILE *f);
static void	pack_list(List *list, FILE *f);
static void	pack_map(Map *map, FILE *f);

int
pack_object(Object *o, FILE *f)
{
    PACK(o->id.id, f);
    PACK(o->id.server, f);
    pack_list(o->parents, f);
    pack_symbols(o, f);
    pack_methods(o->methods, f);
    pack_verbs(o->verbs, f);
    pack_gvars(o->vars, f);
    pack_locks(o->locks, f);
    return 0;
}

static void
pack_methods(HashT *methods, FILE *f)
{
    int		 intzero = 0;
    int		 hval;
    Method	*m;
    unsigned char		 flag;
    
    if (methods) {
	PACK(methods->num, f);
	for (hval = 0; hval < methods->size; hval++) {
	    for (m = methods->table[hval]; m; m = m->next) {
		PACK(m->name, f);
		flag = m->blocked;
		PACK(flag, f);
		PACK(m->ninst, f);
		fwrite((char *) m->ehandler, sizeof(m->ehandler[0]), NERRS, f);
		pack_vars(m->vars, f);
		fwrite((char *) m->code, sizeof(Inst), m->ninst, f);
	    }
	}
    } else {
	PACK(intzero, f);
    }
}

static void
pack_verbs(Verbdef *vbhead, FILE *f)
{
    int		nverbs = 0;
    Verbdef	*vb;

    for (vb = vbhead; vb; vb = vb->next) {
	nverbs++;
    }
    PACK(nverbs, f);
    for (vb = vbhead; vb; vb = vb->next) {
	PACK(vb->verb, f);
	PACK(vb->prep, f);
	PACK(vb->method, f);
    }
}

static void
pack_gvars(HashT *vars, FILE *f)
{
    int		hval, intzero = 0;
    Vardef     *v;

    if (vars) {
	PACK(vars->num, f);
	for (hval = 0; hval < vars->size; hval++) {
	    for (v = vars->table[hval]; v; v = v->next) {
		PACK(v->name, f);
		pack_var(v->value, f);
	    }
	}
    } else {
	PACK(intzero, f);
    }
}

static void
pack_vars(Vardef *vhead, FILE *f)
{
    int		 nvars = 0;
    Vardef	*v;

    for (v = vhead; v; v = v->next) {
	nvars++;
    }
    PACK(nvars, f);
    for (v = vhead; v; v = v->next) {
	PACK(v->name, f);
	pack_var(v->value, f);
    }
}

static void
pack_var(Var v, FILE *f)
{
    fputc((char) v.type, f);
    switch (v.type) {
      case STR:
	pack_string(v.v.str, f);
	break;
      case NUM:
	PACK(v.v.num, f);
	break;
      case OBJ:
	PACK(v.v.obj.id, f);
	PACK(v.v.obj.server, f);
	break;
      case LIST:
	pack_list(v.v.list, f);
	break;
      case MAP:
	pack_map(v.v.map, f);
	break;
      case ERR:
	PACK(v.v.err, f);
	break;
      case PC:				/* should never happen */
	break;
    }
}

static void
pack_string(String *s, FILE *f)
{
    PACK(s->len, f);
    fwrite(s->str, sizeof(char), s->len, f);
}

static void
pack_list(List *list, FILE *f)
{
    int		i;

    PACK(list->len, f);
    for (i = 0; i < list->len; i++) {
	pack_var(list->el[i], f);
    }
}

static void
pack_map(Map *map, FILE *f)
{
    int		 i;
    MapPair	*pair;

    PACK(map->num, f);
    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    pack_var(pair->from, f);
	    pack_var(pair->to, f);
	}
    }
}

static void
pack_symbols(Object *o, FILE *f)
{
    int		i;

    PACK(o->nsymb, f);
    PACK(o->st_size, f);
    for (i = 0; i < o->nsymb; i++) {
	PACK(o->symbols[i].ref, f);
	if (o->symbols[i].ref) {
	    pack_string(o->symbols[i].s, f);
	}
    }
}

static void
pack_locks(Lock *lhead, FILE *f)
{
    Lock	*l;
    int		 nlocks = 0;

    for (l = lhead; l; l = l->next) {
	nlocks++;
    }
    PACK(nlocks, f);
    for (l = lhead; l; l = l->next) {
	pack_string(l->name, f);
	PACK(l->added_by, f);
    }
}
