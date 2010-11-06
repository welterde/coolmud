/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "execute.h"
#include "netio.h"
#include "x.tab.h"

/*
 * builtins.c
 *
 * This file handles looking up and argument checking for the
 * built-in functions, through bf_lookup().  The actual opcodes for
 * the functions are contained in opcodes2.c.
 */

/*
 * bf_table - Table of built-in functions
 *
 * "num" is the token which the parser should assign.  "name" is
 * the name of the function (case is irrelevant, as always). 
 * "minargs" is the minimum number of arguments the function will
 * accept, "maxargs" is the maximum.  If "maxargs" is negative,  an
 * unlimited number of arguments will be accepted.
 */

struct bf_entry bf_table[] = {
    { CLONE,	"clone",	0,	0 }, 	/* functions which modify */
    { DESTROY,	"destroy",	0,	0 },	/* the current object */
    { CHPARENTS,	"chparents",	1,	1 },
    { LOCK,	"lock",		1,	1 },
    { RMVERB,	"rm_verb",	1,	1 },
    { RMMETHOD,	"rm_method",	1,	1 },
    { RMVAR,	"rm_var",	1,	1 },
    { UNLOCK,	"unlock",	1,	1 },
    { VERB,	"add_verb",	3,	3 },
    { ASGNGVAREXPR,"setvar",	2,	2 },
    { CONNECT,	"connect",	2,	2 },

    { VERBS,	"verbs",	0,	0 },	/* functions which get info */
    { VARS,	"vars",		0,	0 },	/* about current object */
    { GETGVAREXPR,	"getvar",	1,	1 },
    { METHODS,	"methods",	0,	0 },
    { HASPARENT,	"hasparent",	1,	1 },
    { OBJSIZE,	"objsize",	0,	0 },
    { FIND_METHOD, "find_method",	1,	1 },
    { SPEW_METHOD,"spew_method",	1,	1 },
    { LIST_METHOD, "list_method",	1,	4 },
    { DECOMPILE, "decompile",	0,	0 },
    { PSUB,	"psub",		1,	1 },

    { ECHO,	"echo",		1,	2 },	/* player functions */
    { ECHON,	"echon",		1,	2 },
    { ECHO_FILE,	"echo_file",	1,	2 },
    { DISCONNECT,	"disconnect",	0,	1 },
    { PROGRAM,	"program",	0,	2 },
    { COMPILE,	"compile",	1,	3 },

    { SETADD,	"setadd",	2,	2 },	/* list functions */
    { SETREMOVE,	"setremove",	2,	2 },
    { LISTINSERT,	"listinsert",	2,	3 },
    { LISTAPPEND,	"listappend",	2,	3 },
    { LISTDELETE, "listdelete",	2,	2 },
    { LISTASSIGN,	"listassign",	3,	3 },
    { SORT,		"sort",		1,	2 },

    { EXPLODE,	"explode",	1,	2 },	/* string functions */
    { STRSUB,	"strsub",	3,	4 },
    { PAD,	"pad",		2,	3 },
    { MATCH,	"match",	2,	3 },
    { MATCH_FULL,	"match_full",	2,	3 },
    { STRCMP,	"strcmp",	2,	2 },
    { F_INDEX,	"index",	2,	3 },
    { RINDEX,	"rindex",	2,	3 },
    { TOLOWER,	"tolower",	1,	1 },
    { TOUPPER,	"toupper",	1,	1 },
    { CRYPT,	"crypt",	1,	2 },

    { TONUM,	"tonum",	1,	1 },	/* conversion functions */
    { TOOBJ,	"toobj",	1,	1 },
    { TOSTR,	"tostr",	1,	1 },
    { TOERR,	"toerr",	1,	1 },

    { TYPEOF,	"typeof",	1,	1 },	/* utility functions */
    { LENGTHOF,	"lengthof",	1,	1 },
    { SERVEROF,	"serverof",	1,	1 },
    { SERVERNAME,	"servername",	1,	1 },
    { SERVERS,	"servers",	0,	0 },
    { RANDOM,	"random",	1,	1 },
    { TIME,	"time",		0,	0 },
    { CTIME,	"ctime",	0,	1 },
    { SLEEP,	"sleep",	1,	1 },
    { KILL,	"kill",		1,	1 },
    { PS,	"ps",		0,	0 },

    { SHUTDOWN,	"shutdown",	0,	0 },	/* wizard functions */
    { SYNC,	"sync",		0,	0 },
    { WRITELOG,	"writelog",	1,	1 },
    { CHECKMEM,	"checkmem",	0,	0 },
    { CACHE_STATS,	"cache_stats",	0,	0 },
    { SET_PARSE,	"set_parse",	2,	2 },
};

/*
 * bf_lookup() - Look up a built-in function by name
 *
 * "name" is the function to look up.  The return value, if positive,
 * is the token which the parser should assign.  Negative return value
 * indicates the function wasn't found.
 */

int
bf_lookup(const char *name)
{
    int		i;

    for (i = 0; i < Arraysize(bf_table); i++) {
	if (!cool_strcasecmp(bf_table[i].name, name)) {
	    return bf_table[i].num;
	}
    }
    return -1;
}

const char *
bf_id2name(int id)
{
    int		i;

    for (i = 0; i < Arraysize(bf_table); i++) {
	if (bf_table[i].num == id) {
	    return bf_table[i].name;
	}
    }
    return 0;
}

struct bf_entry *
bf_id2entry(int id)
{
    int               i;

    for (i = 0; i < Arraysize(bf_table); i++) {
      if (bf_table[i].num == id) {
          return &(bf_table[i]);
      }
    }
    return 0;
}
