/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "servers.h"

    String *
string_new(int len)
{
    String	*new;
    int		 newlen = MAX(len, STRING_INIT_SIZE);

    new = (String *) cool_malloc(sizeof(String) + newlen + 1);
    new->mem = newlen;
    new->len = 0;
    new->ref = 1;
    new->str = (char *) new + sizeof(String);
    new->str[0] = '\0';
    return new;
}

#ifndef INLINE

    String *
string_dup(String *s)
{
    s->ref++;
    return s;
}

    void
string_free(String *s)
{
    s->ref--;
    if (s->ref == 0) {
	FREE(s);
    }
}

#endif

/*
 * string_cpy() - copy a char * into a String struct
 */

    String *
string_cpy(const char *s)
{
    String	*new;

    new = string_new(strlen(s));
    new = string_cat(new, s);
    return new;
}

    String *
string_ncpy(const char *s, int len)
{
    String	*new;

    new = string_new(len + 1);
    strncpy(new->str, s, len);
    new->len = len;
    new->str[len] = '\0';
    return new;
}

    static String *
string_extend_to(String *str, int mem)
{
    String	*new;

    if (!str) return 0;
    if (!str->str) return 0;

    if (mem <= str->mem) {
	return str;
    }
#ifdef STRING_DOUBLING
    while (mem > str->mem) {
	str->mem = (str->mem + sizeof(String)) * 2 - sizeof(String);
    }
#else
    str->mem = (mem / STRING_GROW_BY + 1) * STRING_GROW_BY;
#endif
    new = string_new(str->mem);
    new->str[str->len] = '\0';
    strcpy(new->str, str->str);
    new->len = strlen(new->str);
    new->str[new->len] = '\0';
    FREE(str);
    return new;
}

    String *
string_catc(String *str, char c)
{
    if (!str) return 0;
    if (str->len + 2 > str->mem) {
	str = string_extend_to(str, str->len + 2);
    }
    str->str[str->len++] = c;
    str->str[str->len] = '\0';
    return str;
}

    String *
string_cat(String *str, const char *s)
{
    int		 len;

    if (!str) return 0;
    if (!str->str) return 0;
    if (!s) return 0;

    len = str->len + strlen(s);
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    strcat(str->str, s);
    str->len = len;
    str->str[len] = '\0';
    return str;
}

    String *
string_catnum(String *str, int num)
{
    int		len;

    if (!str) return 0;
    if (!str->str) return 0;

    len = str->len + INT_SIZE;
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    sprintf(str->str + str->len, "%d\0", num);
    str->len = strlen(str->str);
    return str;
}

    String *
string_catobj(String *str, Objid obj, int full)
{
    const char	*servername;
    int		 len;

    if (!str) return 0;
    len = str->len + INT_SIZE + 2 +
	(full ? strlen(servername = serv_id2name(obj.server)) : 0);
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    if (full) {
	sprintf(str->str + str->len, "#%d@%s", obj.id, servername);
    } else {
	sprintf(str->str + str->len, "#%d", obj.id);
    }
    str->len = strlen(str->str);
    return str;
}

    String *
string_indent_cat(String *str, int indent, const char *s)
{
    int		len;
    char	*c;

    if (!str) return 0;
    len = str->len + indent + strlen(s);
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }

    c = str->str + str->len;
    for (; indent; indent--) {
	*c++ = ' ';
    }
    strcpy(c, s);
    str->len = len;
    return str;
}

    String *
string_backslash(String *str, const char *s)
{
    int		len;
    const char	*t;
    char	*r;

    if (!str) return 0;
    /*
     * calculate required length, including backslashes
     * NOTE:  CR-LF turns into \n, both of which are two chars, so no
     *        length adjustment is needed
     */
    len = str->len;
    for (t = s; *t; t++, len++) {
	if ( *t == '"' || *t == '\t') {
	    len++;
	}
    }

    /*
     * extend string, if we have to
     */
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    r = str->str + str->len;	 	/* skip to end of string */
    while( *s )
    {
	switch(*s) {
	    case '\r':
		s++;			/* ignore \r; handle \n instead */
	    case '\n':
		*r++ = '\\';
		*r++ = 'n';
		break;
	    case '\t':
		*r++ = '\\';
		*r++ = 't';
		break;
	    case '\\':
		*r++ = *s;
		*r++ = *s;
		break;
	    case '"':
		*r++ = '\\';
	    default:
		*r++ = *s;
		break;
	}
	s++;
    }
    *r = '\0';
    str->len = strlen(str->str);
    return str;
}

/***********
 * string_output
 * By Robin Powell
 *
 * Turn literal '\''n''s into \r\n
 ***********/

    String *
string_output( String *str )
{
    char *r,*s;

    if (!str) return 0;
    if (!str->str) return 0;

    s = str->str;
    r = str->str;
    while( *s )
    {
	switch(*s) {
	    case '\\':
		s++;
		switch(*s)
		{
		    case 'n':
			/* Replaces the two chars '\' and 'n' */
			*r = '\r';
			r++;
			*r = '\n';
			r++;
			break;
		    case '\\':
			*r = '\\';
			r++;
			break;
		    default:
			*r = '\\';
			r++;
			*r = *s;
			r++;
			break;
		}
		break;
	    default:
		*r++ = *s;
		break;
	}
	if( *s )
	    s++;
    }
    *r = '\0';
    str->len = strlen(str->str);
    return str;
}

/***********
 * string_fixquote
 * By Robin Powell
 *
 * Removes " pairs if the string starts with ".  Truncates at the second ".
 * If the string starts with ", also turn \" to ".
 * Turn \n to \r\n.
 ***********/

    String *
string_fixquote( String *str )
{
    char *r,*s;
    int in_quote = 0;
    int first_char = 0;		/* Set if we've seen a non-blank char */

    if (!str) return 0;
    if (!str->str) return 0;

    s = str->str;
    r = str->str;
    while( *s )
    {
	if( first_char == 0 && isspace(*s) )
	{
	    s++;
	    continue;
	}

	/* Not using a switch here so breaks could be used properly */
	if( *s == '"' )
	{
	    if( in_quote == 1 )
	    {
		/* Found matching quote */
		break;
	    } else {
		if( first_char == 0 )
		{
		    in_quote = 1;
		    s++;
		    continue;
		}
	    }
	}
	if( *s == '\\' )
	{
	    s++;
	    switch(*s)
	    {
		case '"':
		    if( !in_quote )
		    {
			*r = '\\';
			r++;
		    }
		    *r = '"';
		    r++;
		    s++;
		    break;
		case 'n':
		    /* Replaces the two chars '\' and 'n' */
		    *r = '\r';
		    r++;
		    *r = '\n';
		    r++;
		    s++;
		    break;
		default:
		    *r = '\\';
		    r++;
		    *r = *s;
		    r++;
		    s++;
		    break;
	    }
	    continue;
	}
	first_char = 1;
	*r++ = *s;
	s++;
    }
    *r = '\0';
    str->len = strlen(str->str);
    return str;
}

    String *
string_pad(String *str, int padlen, char tok)
{
    int		 len;
    char	*s;

    if (!str) return 0;
    len = str->len + padlen;
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    s = str->str + str->len;
    while (padlen--) {
	*s++ = tok;
    }
    *s = '\0';
    str->len = len;
    return str;
}

    String *
string_prepad(String *str, int padlen, char tok)
{
    int		 len;
    char	*s, *d;

    if (!str) return 0;
    len = str->len + padlen;
    if (len + 1 > str->mem) {
	str = string_extend_to(str, len + 1);
    }
    for (s = str->str + str->len, d = s + padlen; s >= str->str;) {
	*d-- = *s--;
    }
    s = str->str;
    while (padlen--) {
	*s++ = tok;
    }
    str->len = len;
    return str;
}

    String *
string_strip_cr(String *str)
{
    char       *s1, *s2;

    if (!str) return 0;
    for (s1 = s2 = str->str; *s1; s1++) {
	if (*s1 != '\r') {
	    *s2++ = *s1;
	}
    }
    *s2 = '\0'; /* terminate it */
    str->len = s2 - str->str;
    return str;
} /* string_strip_cr() */

/***********
 * string_list
 * By Robin Powell
 *
 * Return a list of all the letters in a string, used in op_for.
 ***********/
List *
string_list(String *str)
{
    List	*new = list_new(0);
    int		 i;
    MapPair	*pair;

    for (i = 0; i < str->len; i++) 
    {
	Var temp;

	temp.v.str = string_new(2);
	temp.type = STR;
	temp.v.str->str[0] = str->str[i];
	temp.v.str->str[1] = '\0';
	new = list_append(new, var_dup(temp), 0 );
    }
    return new;
}

