/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "execute.h"
#include "x.tab.h"

Op_entry  opcode_info[] = {
    { IF,	"IF",		op_if,		2, { NUM_ARG, NUM_ARG }, 0 },
    { ELSEIF, "ELSEIF",		op_elseif,	1, { NUM_ARG }, 0 },
    { ELSE, "ELSE",		op_null,	0, 0, 0 },
    { FOR, "FOR",		op_for,		2, { VARNO_ARG, NUM_ARG }, 0 },
    { FORRNG, "FORRNG",		op_forrng,	2, { VARNO_ARG, NUM_ARG }, 0 },
    { WHILE, "WHILE",		op_while,	1, { NUM_ARG }, 0 },
    { DO, "DO",			op_do,		1, { NUM_ARG }, 0 },
    { DOWHILE, "DOWHILE",	op_dowhile,	2, { NUM_ARG, NUM_ARG }, 0 },
    { AT, "AT",			op_at,		1, { NUM_ARG }, 0 },
    { PUSHPC, "PUSHPC",		op_pushpc,	0, 0, 0 },
    { SLEEP, "SLEEP",		op_sleep,	0, 0, 0 },
    { NUMPUSH, "NUMPUSH",	op_numpush,	1, { NUM_ARG }, 0 },
    { STRPUSH, "STRPUSH",	op_strpush,	1, { STR_ARG }, 0 },
    { OBJPUSH, "OBJPUSH",	op_objpush,	2, { NUM_ARG, NUM_ARG }, 0 },
    { LISTPUSH, "LISTPUSH",	op_listpush,	0, 0, 0 },
    { MAPPUSH, "MAPPUSH",	op_mappush,	1, { NUM_ARG }, 0 },
    { ERRPUSH, "ERRPUSH",	op_errpush,	1, { NUM_ARG }, 0 },
    { GETLVAR, "GETLVAR",	op_getlvar,	1, { VARNO_ARG }, 0 },
    { GETGVAR, "GETGVAR",	op_getgvar,	1, { ID_ARG }, 0 },
    { GETGVAREXPR, "GETGVAREXPR", op_getgvarexpr, 1, { NUM_ARG }, 0 },
    { GETSYSVAR, "GETSYSVAR",	op_getsysvar,	1, { ID_ARG }, 0 },
    { ASGNLVAR, "ASGNLVAR",	op_asgnlvar,	1, { VARNO_ARG }, 0 },
    { ASGNGVAR, "ASGNGVAR",	op_asgngvar,	1, { ID_ARG }, 0 },
    { ASGNGVAREXPR, "ASGNGVAREXPR", op_asgngvarexpr, 1, { NUM_ARG }, 0 },
    { ASGNLVARINDEX, "ASGNLVARINDEX", op_asgnlvarindex, 1, { VARNO_ARG }, 0 },
    { ASGNGVARINDEX, "ASGNGVARINDEX", op_asgngvarindex,	1, { ID_ARG }, 0 },
    { PARENTS, "PARENTS",	op_parents,	0, 0, 0 },
    { THIS, "THIS",		op_this,	0, 0, 0 },
    { PLAYER, "PLAYER",		op_player,	0, 0, 0 },
    { CALLER, "CALLER",		op_caller,	0, 0, 0 },
    { ARGS, "ARGS",		op_args,	0, 0, 0 },
    { SETPLAYER, "SETPLAYER",	op_setplayer,	0, 0, 0 },
    { ADD, "ADD",		op_add,		0, 0, 0 },
    { SUB, "SUB",		op_sub,		0, 0, 0 },
    { MUL, "MUL",		op_mul,		0, 0, 0 },
    { DIV, "DIV",		op_div,		0, 0, 0 },
    { MOD, "MOD",		op_mod,		0, 0, 0 },
    { NEGATE, "NEGATE",		op_negate,	0, 0, 0 },
    { AND, "AND",		op_and,		1, { NUM_ARG }, 0 },
    { OR, "OR",			op_or,		1, { NUM_ARG }, 0 },
    { ENDAND, "ENDAND",		op_null,	0, 0, 0 },
    { ENDOR, "ENDOR",		op_null,	0, 0, 0 },
    { NOT, "NOT",		op_not,		0, 0, 0 },
    { EQ, "EQ",			op_eq,		0, 0, 0 },
    { NE, "NE",			op_ne,		0, 0, 0 },
    { GT, "GT",			op_gt,		0, 0, 0 },
    { GE, "GE",			op_ge,		0, 0, 0 },
    { LT, "LT",			op_lt,		0, 0, 0 },
    { LE, "LE",			op_le,		0, 0, 0 },
    { MESSAGE, "MESSAGE",	op_message,	1, { STR_ARG }, 0 },
    { MESSAGE_EXPR, "MESSAGE_EXPR", op_message_expr, 0, 0, 0 },
    { INDEX, "INDEX",		op_index,	0, 0, 0 },
    { IN, "IN",			op_in,		0, 0, 0 },
    { SUBSET, "SUBSET",		op_subset,	0, 0, 0 },
    { LSUBSET, "LSUBSET",	op_lsubset,	0, 0, 0 },
    { RSUBSET, "RSUBSET",	op_rsubset,	0, 0, 0 },
    { SPLICE, "SPLICE", 	op_splice,	0, 0, 0 },
    { ARGSTART, "ARGSTART",	op_argstart,	0, 0, 0 },
    { LENGTHOF, "LENGTHOF",	op_lengthof,	0, 0, 0 },
    { SETADD, "SETADD",		op_setadd,	0, 0, 0 },
    { SETREMOVE, "SETREMOVE",	op_setremove,	0, 0, 0 },
    { LISTINSERT, "LISTINSERT",	op_listinsert,	0, 0, 0 },
    { LISTAPPEND, "LISTAPPEND",	op_listappend,	0, 0, 0 },
    { LISTDELETE, "LISTDELETE",	op_listdelete,	0, 0, 0 },
    { LISTASSIGN, "LISTASSIGN",	op_listassign,	0, 0, 0 },
    { EXPLODE, "EXPLODE",	op_explode,	0, 0, 0 },
    { SORT, "SORT",		op_sort,	0, 0, 0 },
    { STRSUB, "STRSUB",		op_strsub,	0, 0, 0 },
    { PAD, "PAD",		op_pad,		0, 0, 0 },
    { PSUB, "PSUB",		op_psub,	0, 0, 0 },
    { STRCMP, "STRCMP",		op_strcmp,	0, 0, 0 },
    { F_INDEX, "F_INDEX",	op_f_index,	0, 0, 0 },
    { RINDEX, "RINDEX",		op_rindex,	0, 0, 0 },
    { TOLOWER, "TOLOWER",	op_tolower,	0, 0, 0 },
    { TOUPPER, "TOUPPER",	op_toupper,	0, 0, 0 },
    { RANDOM, "RANDOM",		op_random,	0, 0, 0 },
    { TOSTR, "TOSTR",		op_tostr,	0, 0, 0 },
    { TONUM, "TONUM",		op_tonum,	0, 0, 0 },
    { TOOBJ, "TOOBJ",		op_toobj,	0, 0, 0 },
    { TOERR, "TOERR",		op_toerr,	0, 0, 0 },
    { TYPEOF, "TYPEOF",		op_typeof,	0, 0, 0 },
    { SERVEROF, "SERVEROF",	op_serverof,	0, 0, 0 },
    { SERVERNAME, "SERVERNAME",	op_servername,	0, 0, 0 },
    { SERVERS, "SERVERS",	op_servers,	0, 0, 0 },
    { STOP, "STOP",		op_stop,	0, 0, 0 },
    { T_RAISE, "RAISE",		op_raise,	0, 0, 0 },
    { T_RETURN, "RETURN",	op_return,	0, 0, 0 },
    { BREAK, "BREAK",		op_break,	0, 0, 0 },
    { CONTINUE, "CONTINUE",	op_continue,	0, 0, 0 },
    { ECHO, "ECHO",		op_echo,	0, 0, 0 },
    { ECHON, "ECHON",		op_echon,	0, 0, 0 },
    { ECHO_FILE, "ECHO_FILE",	op_echo_file,	0, 0, 0 },
    { CONNECT, "CONNECT",	op_connect,	0, 0, 0 },
    { TIME, "TIME",		op_time,	0, 0, 0 },
    { CTIME, "CTIME",		op_ctime,	0, 0, 0 },
    { CLONE, "CLONE",		op_clone,	0, 0, 0 },
    { DESTROY, "DESTROY",	op_destroy,	0, 0, 0 },
    { CHPARENTS, "CHPARENTS",	op_chparents,	0, 0, 0 },
    { FIND_METHOD, "FIND_METHOD", op_find_method, 0, 0, 0 },
    { SPEW_METHOD, "SPEW_METHOD", op_spew_method, 0, 0, 0 },
    { LIST_METHOD, "LIST_METHOD", op_list_method, 0, 0, 0 },
    { DECOMPILE, "DECOMPILE",	op_decompile,	0, 0, 0 },
    { POP, "POP",		op_pop,		0, 0, 0 },
    { CRYPT, "CRYPT",		op_crypt,	0, 0, 0 },
    { CHECKMEM, "CHECKMEM",	op_checkmem,	0, 0, 0 },
    { CACHE_STATS, "CACHE_STATS", op_cache_stats, 0, 0, 0 },
    { SET_PARSE, "SET_PARSE",	op_set_parse,	0, 0, 0 },
    { LOCK, "LOCK",		op_lock,	0, 0, 0 },
    { UNLOCK, "UNLOCK",		op_unlock,	0, 0, 0 },
    { HASPARENT, "HASPARENT",	op_hasparent,	0, 0, 0 },
    { OBJSIZE, "OBJSIZE",	op_objsize,	0, 0, 0 },
    { VARS, "VARS",		op_vars,	0, 0, 0 },
    { VERBS, "VERBS",		op_verbs,	0, 0, 0 },
    { METHODS, "METHODS",	op_methods,	0, 0, 0 },
    { VERB, "VERB",		op_verb,	0, 0, 0 },
    { RMVERB, "RMVERB",		op_rmverb,	0, 0, 0 },
    { RMMETHOD, "RMMETHOD",	op_rmmethod,	0, 0, 0 },
    { RMVAR, "RMVAR",		op_rmvar,	0, 0, 0 },
    { PROGRAM, "PROGRAM",	op_program,	0, 0, 0 },
    { COMPILE, "COMPILE",	op_compile,	0, 0, 0 },
    { MATCH, "MATCH",		op_match,	0, 0, 0 },
    { MATCH_FULL, "MATCH_FULL",	op_match_full,	0, 0, 0 },
    { SHUTDOWN, "SHUTDOWN",	op_shutdown,	0, 0, 0 },
    { SYNC, "SYNC",		op_sync,	0, 0, 0 },
    { WRITELOG, "WRITELOG",	op_writelog,	0, 0, 0 },
    { DISCONNECT, "DISCONNECT",	op_disconnect,	0, 0, 0 },
    { KILL, "KILL",		op_kill,	0, 0, 0 },
    { PS, "PS",			op_ps,		0, 0, 0 },
    { PASS, "PASS",		op_pass,	1, { NUM_ARG }, 0 },
};

Op_entry	*opcodes[LAST_TOKEN];

void
opcode_init(void)
{
    int		i;

    for (i = 0; i < LAST_TOKEN; i++) {
	opcodes[i] = 0;
    }
    for (i = 0; i < Arraysize(opcode_info); i++) {
	opcode_info[i].builtin = bf_id2entry(opcode_info[i].opcode);
	opcodes[opcode_info[i].opcode] = &opcode_info[i];
    }
}

void
opcode_free_symbols(Object *o, Method *m)
{
    int		pc = 0, arg;
    Op_entry   *entry;

    while (pc < m->ninst) {
	if (!(entry = opcodes[m->code[pc++]])) {
	    return;
	}
	for (arg = 0; arg < entry->nargs; arg++, pc++) {
	    if (entry->arg_type[arg] == STR_ARG
	     || entry->arg_type[arg] == ID_ARG) {
		sym_free(o, m->code[pc]);
	    }
	}
    }
}
