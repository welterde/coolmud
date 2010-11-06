/* Copyright (c) 1993 Stephen F. White */

#include "config.h"
#include "cool.h"
#include "proto.h"

/*
 * map.c - functions for manipulating mappings (associative arrays)
 *
 * All functions except map_copy() "eat" their arguments.  That is, the
 * caller should bump the refcount if necessary.
 */

void
map_free(Map *map)
{
    MapPair	*pair, *pnext;
    int		 i;

    if (!map) return;
    if (!--map->ref) {
	for (i = 0; i < map->size; i++) {
	    for (pair = map->table[i]; pair; pair = pnext) {
		var_free(pair->from);
		var_free(pair->to);
		pnext = pair->next;
		FREE(pair);
	    }
	}
	FREE(map);
    }
}

Map *
map_copy(Map *map)
{
    Map		*new = map_new(map->num);
    int		 i;
    MapPair	*pair;

    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    map_add(new, var_dup(pair->from), var_dup(pair->to));
	}
    }
    return new;
}

int
map_find(Map *map, Var key, Var *returnval)
{
    int		 hval = hash_var(key) % map->size;
    MapPair	*pair;

    if (!map || !returnval) {
	return -1;
    }
    for (pair = map->table[hval]; pair; pair = pair->next) {
	if (var_eq(key, pair->from)) {
	    *returnval = pair->to;
	    return 0;
	}
    }
    return -1;
}
    
Map *
map_add(Map *map, Var key, Var value)
{
    int		hval = hash_var(key) % map->size;
    MapPair	*pair, *prev;
    Map		*new;

    if (map->ref > 1) {
	/* copy old map to new map */
	new = map_copy(map);
	map->ref--;
	map = new;
    }
    for (prev = pair = map->table[hval]; pair; prev = pair, pair = pair->next) {
	if (var_eq(key, pair->from)) {
	    var_free(pair->to);
	    pair->to = value;
	    var_free(key);
	    return map;
	}
    }
    pair = MALLOC(MapPair, 1);
    pair->from = key;
    pair->to = value;
    pair->next = 0;
    if (prev) {
	prev->next = pair;
    } else {
	map->table[hval] = pair;
    }
    map->num++;
    return map;
}

/***********
 * map_keys
 * By Robin Powell
 *
 * Return a list of all the keys used by a mapping, used in op_for.
 ***********/
List *
map_keys(Map *map)
{
    List	*new = list_new(0);
    int		 i;
    MapPair	*pair;

    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    new = list_append(new, var_dup(pair->from), 0 );
	}
    }
    return new;
}

