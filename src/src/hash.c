/* Copyright (C) 1993 Stephen F. White */

/*
 * This hashing function was ripped from Aho & Ullman, who ripped it
 * from someone else.
 */

#include "config.h"
#include "cool.h"
#include "proto.h"

int
hash(const char *s)
{
    const char	*p;
    unsigned	 h = 0, g;
    for (p = s; *p; p++) {
	h = (h << 4) + (*p);
	g = h & 0xf0000000;
	if (g) {
	    h = h ^ (g >> 24);
	    h = h ^ g;
	}
    }
    return h;
}

int
hash_var(Var v)
{
    switch (v.type) {
      case STR:
	return hash(v.v.str->str);
      case NUM:
	return v.v.num;
      case OBJ:
	return v.v.obj.id;	/* these */
      case LIST:
	return v.v.list->len;	/* are */
      case MAP:
	return v.v.map->num;	/* totally */
      case ERR:
	return (int) v.v.err;	/* bogus */
      case PC:
	return 0;
    }
    return 0;
}

HashT *
hash_new(int  htsize)
{
    HashT	*new;
    int		 i;

    htsize = MAX(htsize, HASH_INIT_SIZE);
    new = cool_malloc(sizeof(HashT) + htsize * sizeof(void *));
    new->size = htsize;
    new->num = 0;
    new->table = (void *) (new + 1);
    new->ref = 1;
    for (i = 0; i < htsize; i++) {
	new->table[i] = 0;
    }
    return new;
}
