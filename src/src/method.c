/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"

static void	do_fmr(Object *o, const char *name);

/*
 * find_method() - find a method on an object
 * 
 * returns a pointer to the method if found, 0 if not.
 */

Method *
find_method(Object *o, const char *name)
{
    Method	*m;
    int		 hval;

    if (!o || !o->methods) { return 0; }
    hval = hash(name) % o->methods->size;
    for (m = o->methods->table[hval]; m; m = m->next) {
	if (!cool_strcasecmp(sym_get(o, m->name)->str, name)) {
	    return m;
	}
    }
    return 0;
}

/*
 * find_method_recursive() - find a method on an object or its ancestors
 *
 * Uses a post-order depth first search reversing the order of the
 * parents, and marks searched objects using a generational index. 
 * This actually results in a reverse topological search, with a left to
 * right ordering of branches.  Thus, an object is only searched after
 * all the objects that point to it are searched.  Thanks to greg hudson
 * for help with the algorithm.
 */

static Method	*found_m;
static Object	*found_o;
static int	 found_blocked_method;		/* flag */
static unsigned	 search_gen = 0;

Method *
find_method_recursive(Object *o, const char *name, Object **where)
{
    if (!o || !name) return 0;
    search_gen += 1;
    found_m = 0;
    found_o = 0;
    found_blocked_method = 0;
#ifdef METHOD_DEBUG
    printf("#%d.%s:  ", o->id.id, name);
#endif /* METHOD_DEBUG */
    do_fmr(o, name);
#ifdef METHOD_DEBUG
    if (found_m) {
	if (found_blocked_method) {
	    printf(", found on #%d, blocked\n", found_o->id.id);
	} else {
	    printf(", found on #%d\n", found_o->id.id);
	} /* if */
    } else {
	printf(", not found\n");
    } /* if */
#endif /* METHOD_DEBUG */
    *where = found_o;
    return found_m;
} /* find_method_recursive */

static void
do_fmr(Object *o, const char *name)
{
    Method	*m;
    int		 i;
    Object	*p;

      /* bail if we've already searched this one */
    if (!o || o->last_search == search_gen) return;

      /* bail if we already found a blocked method */
    if (found_blocked_method) return;

      /* search parents, in reverse order */
    for (i = o->parents->len - 1; i >= 0; i--) {
	p = retrieve(o->parents->el[i].v.obj);
	do_fmr(p, name);
	if (found_blocked_method) return;
    } /* for */
    o->last_search = search_gen;		/* mark this object searched */
    m = find_method(o, name);
#ifdef METHOD_DEBUG
    printf("#%d ", o->id.id);
#endif /* METHOD_DEBUG */
    if (m) {					/* if we hit one, */
	found_m = m;				/* save it */
	found_o = o;
	if (m->blocked) {			/* if it was blocked, */
	    found_blocked_method = 1;		/* set the flag */
	} /* if */
    } /* if */
} /* find_method_recursive */

void
add_method(Object *o, Method *new_method)
{
    Method	*m;
    int		 hval;
 
    if (!o || !new_method) return;
    new_method->next = 0;
    if (!o->methods) {
	o->methods = hash_new(HASH_INIT_SIZE);
    }
    hval = hash(sym_get(o, new_method->name)->str) % o->methods->size;
      /* skip to end of chain */
    for (m = o->methods->table[hval]; m && m->next; m = m->next)
	;
    if (m) {
	m->next = new_method;
    } else {
	o->methods->table[hval] = new_method;
    }
    o->methods->num++;
}

Error
rm_method(Object *o, const char *name)
{
    Method	*m, *prev;
    int		 hval;

    if (!o || !name) return E_INVIND;
    if (!o->methods) return E_METHODNF;
    hval = hash(name) % o->methods->size;
    for (m = o->methods->table[hval], prev = 0; m; prev = m, m = m->next) {
	if (!cool_strcasecmp(sym_get(o, m->name)->str, name)) {
	    if (prev) {
		prev->next = m->next;
	    } else {
		o->methods->table[hval] = m->next;
	    }
	    free_method(o, m);
	    o->methods->num--;
	    return E_NONE;
	}
    }
    return E_METHODNF;
}
