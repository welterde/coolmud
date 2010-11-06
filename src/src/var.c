/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "servers.h"

#ifndef INLINE
void
var_free(Var  v)
{
    switch (v.type) {
      case NUM:
      case OBJ:
      case ERR:
      case PC:
	break;
      case LIST:
	list_free(v.v.list);
	break;
      case MAP:
	map_free(v.v.map);
	break;
      case STR:
	string_free(v.v.str);
	break;
    }
}

Var
var_dup(Var v)
{
    switch (v.type) {
      case NUM:
      case OBJ:
      case ERR:
      case PC:
	break;
      case LIST:
	v.v.list = list_dup(v.v.list);
	break;
      case MAP:
	v.v.map = map_dup(v.v.map);
	break;
      case STR:
	v.v.str = string_dup(v.v.str);
	break;
    }    
    return v;
}

#endif

int
var_compare(Var  v1, Var  v2)
{
    switch(v1.type) {
      case NUM:
	return v1.v.num - v2.v.num;
      case STR:
	return cool_strcasecmp(v1.v.str->str, v2.v.str->str);
      case OBJ:
	return v1.v.obj.id - v2.v.obj.id;
      case ERR:  case LIST:  case PC:  case MAP:
	return -1;	/* cannot compare error, list or map values */
    }
    return -1;		/* should never reach */
}

int
var_eq(Var  v1,  Var v2)
{
    int		 i;
    MapPair	*pair;
    Var		 to;

    if(v1.type != v2.type) {
	return 0;	/* values of different types are unequal */
    }
    switch(v1.type) {
      case NUM:  case PC:
	return v1.v.num == v2.v.num;
      case OBJ:
	return v1.v.obj.id == v2.v.obj.id && v1.v.obj.server == v2.v.obj.server;
      case ERR:
	return v1.v.err == v2.v.err;
      case STR:
	return !cool_strcasecmp(v1.v.str->str, v2.v.str->str);
      case LIST:
	if (v1.v.list == v2.v.list) {		/* if they're the same list, */
	    return 1;				/* we're done */
	} else if (v1.v.list->len != v2.v.list->len) {
	    return 0;
	}
	for (i = 0; i < v1.v.list->len; i++) {
	      /* recursively compare all elements of list */
	    if (!var_eq(v1.v.list->el[i], v2.v.list->el[i])) {
		return 0;
	    }
	}
	return 1;
      case MAP:
	if (v1.v.map == v2.v.map) {
	    return 1;
	} else if (v1.v.map->num != v2.v.map->num) {
	    return 0;
	}
	for (i = 0; i < v1.v.map->size; i++) {
	    for (pair = v1.v.map->table[i]; pair; pair = pair->next) {
		if (!map_find(v2.v.map, pair->from, &to)) {
		    return 0;
		} else if (!var_eq(pair->to, to)) {
		    return 0;
		}
	    }
	}
	return 1;
    }
    return 0;
}

Var
var_init(int type)
{
    Var		v;

    switch(v.type = type) {
      case STR:
	v.v.str = string_new(1);
	break;
      case NUM:  case PC:
	v.v.num = 0;
	break;
      case OBJ:
	v.v.obj.id = -1;
	v.v.obj.server = 0;
	break;
      case LIST:
	v.v.list = list_dup(empty_list);
	break;
      case MAP:
	v.v.map = map_new(0);
	break;
      case ERR:
	v.v.err = E_NONE;
	break;
    }
    return v;
}
    
int
var_add_local(Method *m, int name, int *varno)
{
    Vardef 	*prev = 0, *v;

    for (v = m->vars, *varno = 0; v; v = v->next, (*varno)++) {
	prev = v;
	if (v->name == name) {
	    return 1;
	}
    }
    v = MALLOC(Vardef, 1);
    v->name = name;
    v->value.type = NUM;
    v->value.v.num = 0;
    v->next = 0;
    if (prev) {
	prev->next = v;
    } else {
	m->vars = v;
    }
    return 0;
}

int
var_add_global(Object *o, int name, Var init_value)
{
    Vardef	*v;
    int		 hval, dummy;

    if (!o->vars) {
	o->vars = hash_new(HASH_INIT_SIZE);
    }
    hval = hash(sym_get(o, name)->str) % o->vars->size;
    if (var_find(o->vars->table[hval], name, &dummy)) {
	return 1;
    } else {
	v = MALLOC(Vardef, 1);
	v->name = name;
	v->value = init_value;
	v->next = o->vars->table[hval];
	o->vars->table[hval] = v;
	o->vars->num++;
	return 0;
    }
}

Error
var_rm_global(Object *o, const char *name)
{
    Vardef	*v, *prev = 0;
    int		 hval;

    if (!o->vars) { return E_VARNF; }
    hval = hash(name) % o->vars->size;
    for (v = o->vars->table[hval]; v; prev = v, v = v->next) {
	if (!cool_strcasecmp(sym_get(o, v->name)->str, name)) {
	    if (prev) {
		prev->next = v->next;
	    } else {
		o->vars->table[hval] = v->next;
	    }
	    o->vars->num--;
	    sym_free(o, v->name);
	    var_free(v->value);
	    FREE(v);
	    return E_NONE;
	}
    }
    return E_VARNF;
}

Vardef *
var_find(Vardef *top, int name, int *varno)
{
    Vardef	*v;

    for (v = top, *varno = 0; v; v = v->next, (*varno)++) {
	if (v->name == name) {
	    return v;
	}
    }
    return 0;
}

Vardef *
var_find_local_by_name(Object *o, Method *m, const char *name, int namelen,
		 int *varno)
{
    Vardef	*v;
    const char	*vname;

    for (v = m->vars, *varno = 0; v; v = v->next, (*varno)++) {
	vname = sym_get(o, v->name)->str;
	if (strlen(vname) == namelen
	 && !cool_strncasecmp(vname, name, namelen)) {
	    return v;
	}
    }
    return 0;
}

Vardef *
var_find_local(Method *m, int varno)
{
    Vardef	*v;

    for (v = m->vars; v && varno; v = v->next, varno--)
	;
    return v;
}

void
var_assign_local(Var  *vars, int  varno, Var  value)
{
    var_free(vars[varno]);
    vars[varno] = value;
}

Error
var_assign_global(Object  *o, String *name, Var  value)
{
    Vardef	*v;
    Var		 oldvalue;
    int		 hval;

    if (var_get_global(o, name->str, &oldvalue) == E_NONE) {
	if (value.type != oldvalue.type) {
	    var_free(value);
	    return E_TYPE;
	}
    }

    if (o->vars) {
	hval = hash(name->str) % o->vars->size;
	for (v = o->vars->table[hval]; v; v = v->next) {
	    if (!cool_strcasecmp(sym_get(o, v->name)->str, name->str)) {
		var_free(v->value);
		v->value = value;
		cache_put(o, o->id.id);
		return E_NONE;
	    }
	}
    }
    name->ref++;
    var_add_global(o, sym_add(o, name), value);
    cache_put(o, o->id.id);
    return E_NONE;
}

void
var_get_local(Var  *vars, int  varno, Var *r)
{
    *r = vars[varno];
}

Error
var_get_global(Object *o, const char *name, Var *r)
{
    Vardef	*v;
    Object	*p;
    int		 i, hval;

    if (o->vars) {
	hval = hash(name) % o->vars->size;
	for (v = o->vars->table[hval]; v; v = v->next) {
	    if (!cool_strcasecmp(name, sym_get(o, v->name)->str)) {
		*r = v->value;
		return E_NONE;
	    }
	}
    }

/*
 * couldn't find locally, search on parents, recursively
 */
    for (i = 0; i < o->parents->len; i++) {
	if ((p = retrieve(o->parents->el[i].v.obj))) {
	    if (var_get_global(p, name, r) == E_NONE) {
		return E_NONE;			/* found it */
	    }
	}
    }
/*
 * no match
 */
    return E_VARNF;
}

String *
var_tostring(String *st, Var  v, int quotes)
{
    switch (v.type) {
      case NUM:  case PC:
	st = string_catnum(st, v.v.num);
	break;
      case OBJ:
	st = string_catobj(st, v.v.obj, v.v.obj.server);
	break;
      case STR:
        if (quotes) {
	    st = string_catc(st, '"');
	}
	st = string_backslash(st, v.v.str->str);
        if (quotes) {
	    st = string_catc(st, '"');
	}
	break;
      case LIST:
	st = list_tostring(st, v.v.list);
	break;
      case MAP:
	st = map_tostring(st, v.v.map);
	break;
      case ERR:
	st = string_cat(st, err_id2desc(v.v.err));
	break;
    }
    return st;
}

String *
list_tostring(String *st, List *l)
{
    int		i;

    st = string_catc(st, '{');
    for (i = 0; i < l->len; i++) {
	st = var_tostring(st, l->el[i], 1);
	if (i < l->len - 1) {
	    st = string_cat(st, ", ");
	}
    }
    st = string_catc(st, '}');
    return st;
}

String *
map_tostring(String *st, Map *map)
{
    int		 i, num = 0;
    MapPair	*pair;

    st = string_catc(st, '[');
    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    st = var_tostring(st, pair->from, 1);
	    st = string_cat(st, " => ");
	    st = var_tostring(st, pair->to, 1);
	    if (num < map->num - 1) {
		st = string_cat(st, ", ");
	    }
	    num++;
	}
    }
    st = string_catc(st, ']');
    return st;
}

int
var_count_local(Method *m)
{
    int		c;
    Vardef     *v;

    for (v = m->vars, c = 0; v; v = v->next, c++)
	;
    return c;
}

void
var_init_local(Object *on, Method *m, Var *vars)
{
    int		 i;
    Vardef	*v;

    for (v = m->vars, i = 0; v; v = v->next, i++) {
	vars[i] = var_dup(v->value);
    }
}

void
var_copy_vars(Object *source, Object *dest)
{
    Vardef	*v;
    String	*varname;
    int		 hval;

    if (!source->vars) { return; }
    for (hval = 0; hval < source->vars->size; hval++) {
	for (v = source->vars->table[hval]; v; v = v->next) {
	    varname = sym_get(source, v->name);
	    (void) var_assign_global(dest, varname, var_dup(v->value));
	}
    }
}
