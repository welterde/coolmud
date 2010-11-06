/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

static List *	doinsert(List *list, Var value, int pos);
static int	sort_up(const void *a, const void *b);
static int	sort_down(const void *a, const void *b);

List *
list_new(int size)
{
    List	*new;

    new = MALLOC(List, 1);
    new->len = size;
    new->ref = 1;
    if (size) {
	new->el = MALLOC(Var, size);
    } else {
	new->el = 0;
    }
    new->mem = size;
    return new;
}

List *
list_setadd(List *list, Var value)
{
    List		*ret;

    ret = list;
    if (list_ismember(value, list)) {
	var_free(value);
	return ret;
    }
    return list_append(list, value, 0);
}
	
List *
list_setremove(List *list, Var value)
{
    List	*ret;
    int		 t;

    ret = list;
    if ((t = list_ismember(value, list))) {
	ret = list_delete(list, t);
    }
    return ret;
}

int
list_ismember(Var element, List *list)
{
    int		i;

    for (i = 0; i < list->len; i++) {
	if (var_eq(element, list->el[i])) {
	    return i + 1;
	}
    }
    return 0;
}

List *
list_assign(List *list, Var value, int pos)
{
    List	*new;
    int		 i;

    if (list->ref == 1) {
	var_free(list->el[pos - 1]);
	list->el[pos - 1] = value;
	return list;
    }

    new = list_new(list->len);
    for (i = 0; i < list->len; i++) {
	if (i == pos - 1) {
	    new->el[i] = value;
	} else { 
	    new->el[i] = var_dup(list->el[i]);
	}
    }

    list_free(list);
    return new;
}

static List *
doinsert(List *list, Var value, int pos)
{
    List	*new;
    int		 i;

    new = list_new(list->len + 1);
    for (i = 0; i < pos - 1; i++) {
	new->el[i] = var_dup(list->el[i]);
    }
    new->el[pos - 1] = value;
    for (i = pos; i < new->len; i++) {
	new->el[i] = var_dup(list->el[i - 1]);
    }
    list_free(list);
    return new;
}

List *
list_insert(List *list, Var value, int pos)
{
    if (pos < 1 || pos > list->len) {
	pos = 1;
    }
    return(doinsert(list, value, pos));
}
    
List *
list_append(List *list, Var value, int pos)
{
    if (pos < 1 || pos > list->len) {
	pos = list->len;
    }
    return(doinsert(list, value, pos + 1));
}

List *
list_delete(List *list, int pos)
{
    List	*new;
    int		 i;

    new = list_new(list->len - 1);
    for (i = 0; i < pos - 1; i++) {
	new->el[i] = var_dup(list->el[i]);
    }
    for (i = pos - 1; i < new->len; i++) {
	new->el[i] = var_dup(list->el[i + 1]);
    }
    list_free(list);				/* free old list */
    return new;
}

List
*list_subset(List *list, int lower, int upper)
{
    List	*r;
    int		i;

    r = list_new(upper - lower + 1);
    for (i = lower - 1; i < upper; i++) {
	r->el[i - lower + 1] = var_dup(list->el[i]);
    }
    return r;
}

#ifdef INLINE
void
list__free(List *list)
{
    int		i;

    for (i = 0; i < list->len; i++) {
	var_free(list->el[i]);
    }
    if (list->len) {
	FREE(list->el);
    }
    FREE(list);
}

#else /* !INLINE */

void
list_free(List *list)
{
    int		i;

    if (!--list->ref) {
	for (i = 0; i < list->len; i++) {
	    var_free(list->el[i]);
	}
	if (list->len) {
	    FREE(list->el);
	}
	FREE(list);
    }
}

List *
list_dup(List *list)
{
    list->ref++;
    return list;
}
#endif /* !INLINE */

List *
explode(const char *str, const char *sep, int nwords)
{
    const char	*word_start;
    List	*list = list_new(nwords);
    int		 i;

    for (i = 0; i < nwords; i++) {
	while ( *str && strncmp( str, sep, strlen( sep ) ) == 0 )
	{
	    str += strlen( sep );
	}

	word_start = str;
	/* skip to next separator */
	while (*str && strncmp( str, sep, strlen( sep ) ) != 0 )
	{
	    str++;
	}
	list->el[i].type = STR;
	list->el[i].v.str = string_ncpy(word_start, str - word_start);
    }
    return list;
}

List *
list_sort(List *list, int reverse)
{
    List	*ret;
    int		i;

    if (list->ref == 1) {
	ret = list;
    } else {
	ret = list_new(list->len);
	for (i = 0; i < list->len; i++) {
	    ret->el[i] = var_dup(list->el[i]);
	}
	list_free(list);
    }
    qsort(ret->el, ret->len, sizeof(Var), (reverse ? sort_down : sort_up));
    return ret;
}

static int
sort_up(const void *a, const void *b)
{
    return var_compare(*((const Var *) a), *((const Var *) b));
}

static int
sort_down(const void *a, const void *b)
{
    return var_compare(*((const Var *) b), *((const Var *) a));
}
