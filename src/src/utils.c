/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

char  *
strndup(const char *s, int n)
{
    char	*r;

    if (!s || !*s || n <= 0) {
	r = MALLOC(char, 1);
	*r = '\0';
    } else {
	r = MALLOC(char, n + 1);
	strncpy(r, s, n);
	r[n] = '\0';
    }
    return  r;
}

char  *
str_dup(const char *s)
{
    char	*r;

    if (!s || !*s) {
	r = MALLOC(char, 1);
	*r = '\0';
    } else {
	r = MALLOC(char, strlen(s) + 1);
	strcpy(r, s);
    }
    return  r;
}

int
hasparent(Object *o, Objid pid)
{
    int		 i, r = 0;
    Object	*p;
    List	*parents = list_dup(o->parents);

    for (i = 0; i < parents->len; i++) {
	if (parents->el[i].v.obj.id == pid.id) {
	    r = 1; break;
	} else if ((p = retrieve(parents->el[i].v.obj))
	       && hasparent(p, pid)) {
	    r = 1; break;
	}
    }
    list_free(parents);
    return r;
}

Error
check_parents(Objid player, Object *o, List *newparents)
{
    int		 i, j;
    Objid	 parent;
    Object	*p;

#ifdef ROOT_OBJ
    if (newparents->len == 0) {
	if (o->id.id != ROOT_OBJ) {
	    return E_RANGE;
	}
    }
#endif /* ROOT_OBJ */
    for (i = 0; i < newparents->len; i++) {
	parent = newparents->el[i].v.obj;
	if (parent.server) {
	  /* parent is not local */
	    return E_INVIND;
	} else if (!(p = retrieve(parent))) {
	    return E_OBJNF;
	} else if (p->id.id == o->id.id) {
	      /* parent is this! */
	    return E_MAXREC;
	} else if (!can_clone(player, parent)) {
	    return E_PERM;
	}
	for (j = 0; j < newparents->len; j++) {
	    if (hasparent(p, newparents->el[j].v.obj)) {
		  /* parents form a loop */
		return E_MAXREC;
	    }
	}
    }
    return E_NONE;
}

/*
 * NB: These versions of strcasecmp() and strncasecmp() depend on ASCII.
 */

static char  cmap[257] = "\
\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\
\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\
\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057\
\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077\
\100\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157\
\160\161\162\163\164\165\166\167\170\171\172\133\134\135\136\137\
\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157\
\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177\
\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\
\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\
\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\
\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\
\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\
\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\
\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\
\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377";

#if !HAVE_STRCASECMP

int
cool_strcasecmp(register const char *s, register const char *t)
{
    register char	*c = cmap;

    while (c[*s] == c[*t++]) {
	if (!*s++)
	    return 0;
    }
    return(c[*s] - c[*--t]);
}

int
cool_strncasecmp(register const char *s, register const char *t,
		   register int n)
{
    register char	*c = cmap;
    
    if (!n) return 0;
    while (c[*s] == c[*t++]) {
	if (!*s++ || !--n)
	    return 0;
    }
    return(c[*s] - c[*--t]);
}
#endif

#if !HAVE_GETTIMEOFDAY
int
cool_gettimeofday(struct timeval *tv, struct timezone *tzp)
{
    tv->tv_sec = time((long *) 0);
    tv->tv_usec = 0;
    /* leave tzp alone, since we don't use it */
    return 0;
}
#endif /* !HAVE_GETTIMEOFDAY */

int
match (register const char *template, register const char *tomatch,
	    char marker)
{
    register const char	*t;
    register int	 l;

    while (*tomatch == marker) {	/* skip leading markers in string */
	tomatch++;
    }
    if ((t = index(tomatch, marker))) {
	l = t - tomatch;
    } else {
	l = strlen(tomatch);
    }
    while (template && *template) 
    {
/*
 * skip leading markers in template
 */
	while (*template == marker) {	/* skip tokens in template */
	    template++;
	}
/*
 * if substring of this word in template matches word, return 1
 */
	while( *template != marker && *template )
	{
	    if (!cool_strncasecmp(template, tomatch, l)) 
	    {
		return 1;
	    }
	    template++;
	}
/*
 * skip to next marker
 */
	template = index(template, marker);
    }
    return 0;
}

int
match_full (register const char *template, register const char *tomatch,
	    char marker)
{
    register const char	*t;
    register int	 l1, l2;

    while (*tomatch == marker) {	/* skip leading markers in string */
	tomatch++;
    }
    l1 = strlen(tomatch);
    while (template && *template) {
	while (*template == marker) {	/* skip leading markers in template */
	    template++;
	}
	if ((t = index(template, marker))) {
	    l2 = t - template;
	} else {
	    l2 = strlen(template);
	}
	if (l1 == l2 &&
	    !cool_strncasecmp(template, tomatch, l2)) {	/* got a match */
	    return 1;
	}
	template = t;	/* skip to next marker */
    }
    return 0;
}
int
str_in(const char *s1, const char *s2)
{
    const char	*t2;
    int		 len = strlen(s1);

    for(t2 = s2; *t2; t2++) {
	if (!cool_strncasecmp(s1, t2, len)) {
	    return t2 - s2 + 1;
	}
    }
    return 0;
}

int
verb_match(register const char *verb, register const char *word)
{
    register const char	*w;
    register short	star;
    register char	*c = cmap;

    while(*verb) {
	w = word;
	star = 0;
	while (1) {
	    if (*verb == '*') {
		verb++;
		star = 1;
	    }
	    if (!*verb || isspace(*verb) || !*w || c[*w] != c[*verb]) break;
	    w++; verb++;
	}
	if (star && !*w) return 1;
	if (!*w && (!*verb || isspace(*verb)))
	    return 1;
	while (*verb && !isspace(*verb))
	    verb++;
	while (isspace(*verb))
	    verb++;
    }
    return 0;
}

int
prep_match(const char *preplist, const char *argstr,
	   const char **dobj, const char **prep, const char **iobj,
	   int *dobjlen, int *preplen)
{
    register const char	*c = cmap;
    register const char	*p;
    register const char	*a = argstr;
    register const char	*dend = a, *pstart;
    int			 dobj_quoted = 0;

    while (isspace(*a)) {
	a++;
    }
    if (*a == '"') {
	*dobj = ++a;
	while (*a && *a != '"') {	/* skip quoted dobj */
	    if( *a == '\\' )	/* Quote " with \ */
	    {
		switch( a[1] )
		{
		    case '"':
			a++;
			break;
		    default:
			break;
		}
	    }
	    a++;
	}
	dend = a;
	dobj_quoted = 1;
    } else {
	*dobj = a;
    }


    while(*a) 
    {
	if (!dobj_quoted) 
	{
	    dend = a;
	}
	while (isspace(*a))		/* skip whitespace before prep */
	    a++;
	pstart = a;
	p = preplist;
	while (*p) {
	    a = pstart;
	    while (isspace(*p))		/* skip whitespace */
	    {
		p++;
	    }
	    while (c[*p++] == c[*a++]) 
	    {
		if (*p == '_') {
		    while (isspace(*a))
			a++;
		    p++;
		}
		if (isspace(*p) || (!*p && isspace(*a)) || !*a) 
		{
		    if( !*p || !*a || c[*p] == c[*a] )
		    {
			*prep = pstart;
			*preplen = a - pstart;
			*dobjlen = dend - *dobj;
			while (isspace(*a))
			    a++;
			*iobj = a;
			return 1;		/* found a match, quit */
		    } else
			break;
		}
	    }
	    while (*p && !isspace(*p))	/* skip to next preposition */
	    {
		p++;
	    }
	}
	while (*a && !isspace(*a))	/* no match, skip to next word */
	    a++;
    }
    return 0;
}

int
valid_ident(const char *s)
{
    if (!isalpha(s[0]) && *s != '_') {
	return 0;
    }
    for (++s; *s; s++) {
	if (!isalnum(*s) && *s != '_') {
	    return 0;
	}
    }
    return 1;
}

    String *
strsub(const char *source, const char *what, const char *with, int case_matters)
{
    register const char	*s;
    String	*r;
    int		lwhat = strlen(what);

    if (lwhat == 0) {
	return string_cpy(source);
    } /* if */

    r = string_new(0);
    s = source;

    while (*s) {
	if (case_matters ? !strncmp(s, what, lwhat)
		: !cool_strncasecmp(s, what, lwhat)) {
	    r = string_cat(r, with);
	    s += lwhat;
	} else {
	    r = string_catc(r, *s);
	    s++;
	}
    }
    return r;
}

    const char *
addr_htoa(unsigned long l)
{
    static char	buf[32];

    sprintf(buf, "%ld.%ld.%ld.%ld",
	    (l >> 24) & 0xFF, (l >> 16) & 0xFF, (l >> 8) & 0xFF, l & 0xFF);
    return buf;
}

    int
count_words(const char *s, const char *sep)
{
    int		nwords = 0;

    while (*s) {
	/* skip leading separators */
	while ( *s && strncmp( s, sep, strlen( sep ) ) == 0 )
	    s++;
	if (!*s)		/* if that's it, bug out */
	    return nwords;
	/* skip to next separator */
	while (*s && strncmp( s, sep, strlen( sep ) ) != 0 )
	    s++;
	nwords++;
    }
    return nwords;
}


/* simple substring search routines, courtesy of clay luther */

    int
strindex(const char *source, const char *what, int case_counts)
{
    const char  *s, *e;
    int         lwhat = strlen(what);

    for (s = source, e = source + strlen(source) - lwhat; s <= e; s++) {
	if (! (case_counts ? strncmp(s, what, lwhat)
		: cool_strncasecmp(s, what, lwhat))) {
	    return s - source + 1;
	}
    }
    return 0;
}

    int
strrindex(const char *source, const char *what, int case_counts)
{
    const char  *s;
    int         lwhat = strlen(what);

    for (s = source + strlen(source) - lwhat; s >= source; s--) {
	if (! (case_counts ? strncmp(s, what, lwhat)
		: cool_strncasecmp(s, what, lwhat))) {
	    return s - source + 1;
	}
    }
    return 0;
}
