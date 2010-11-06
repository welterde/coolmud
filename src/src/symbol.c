/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

List	*empty_list, *zero, *one;

static struct st {
    Sys_sym		 sym;
    const char		*name;
} system_table_names[] = {
    { BLANK,			"" },
    { INIT,			"init" },
    { PARSE,			"parse" },
    { BOOT_SERVER,		"boot_server" },
    { CONNECT_SERVER,		"connect_server" },
    { DISCONNECT_SERVER,	"disconnect_server" },
    { SYM_CONNECT,		"connect" },
    { SYM_DISCONNECT,		"disconnect" },
    { RAISE,			"raise" },
    { RETURN,			"return" },
};

#define SYS_SIZE Arraysize(system_table_names)

String		*system_table[SYS_SIZE];

int
sym_add(Object *o, String  *name)
{
    int		i;
    Symbol     *new_symbols;

    for (i = 0; i < o->nsymb; i++) {
	if (o->symbols[i].ref && !strcmp(o->symbols[i].s->str, name->str)) {
	    o->symbols[i].ref++;
	    string_free(name);
	    return i;
	}
    }
    for (i = 0; i < SYS_SIZE; i++) {
	if (!strcmp(system_table[i]->str, name->str)) {
	    string_free(name);
	    return -i - 1;
	}
    }
    for (i = 0; i < o->nsymb; i++) {
	if (!o->symbols[i].ref) {	/* no refs to symbol; blank entry */
	    o->symbols[i].s = name;	/* fill it in */
	    o->symbols[i].ref = 1;
	    return i;
	}
    }
    if (o->nsymb >= o->st_size) {	/* symbol table full, double it */
	new_symbols = MALLOC(Symbol, o->st_size * 2);
	for (i = 0; i < o->nsymb; i++) {
	    new_symbols[i] = o->symbols[i];
	}
	o->st_size *= 2;
	FREE(o->symbols);
	o->symbols = new_symbols;
    }
    o->symbols[o->nsymb].s = name;
    o->symbols[o->nsymb++].ref = 1;
    return o->nsymb - 1;
}

void
sym_init_sys(void)
{
    int		i;
    struct st	j;

    for (i = 0; i < Arraysize(system_table_names); i++) {
	j = system_table_names[i];
	system_table[j.sym] = string_cpy(j.name);
    }
    empty_list = list_new(0);
    zero = list_new(1);
    zero->el[0].type = NUM;
    zero->el[0].v.num = 0;
    one = list_new(1);
    one->el[0].type = NUM;
    one->el[0].v.num = 1;
}

void
sym_free(Object *o, int symno)
{
    if (symno < 0) {		/* it's a system symbol */
	return;			/* abort */
    }
    if (!--o->symbols[symno].ref) {
	string_free(o->symbols[symno].s);
	o->symbols[symno].s = 0;
    }
}

void
sym_init(Object *o)
{
    o->st_size = SYM_INIT_SIZE;
    o->symbols = MALLOC(Symbol, o->st_size);
    o->nsymb = 0;
}
