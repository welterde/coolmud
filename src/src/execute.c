/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "netio.h"
#include "execute.h"
#include "servers.h"
#include "x.tab.h"

/*
 * execute.c
 * 
 * This file contains execution global parameters, and is the interface
 * to the finite state machine executor.  The interface is through
 * call_method(), resume_method_*() or call_verb().
 *
 * It also contains functions useful for the opcodes of the state machine,
 * including push(), pop(), pushn(), raise() and send_message_and_block().
 */

/*
 * The actual execution globals
 */

Event		 frame;		/* contains pc, sp, local vars, etc. */
Object *	 this;
enum state	 ex_state;	/* current execution state */
Var		 ex_retval;	/* return value of current method */
int		 nargs;		/* argument count to built-in function */

/*
 * Prototypes for functions local to this file
 */

static void	  execute(void);
static Error	  call_verb_recursive(Object *o, int msgid, int age, int ticks,
		    Objid player, Objid from, Objid to, const char *verb,
		    const char *argstr);
static void	  free_frame(Event *e);
static void	  clear_locks(Object *o, int msgid);
static String	 *add_traceback(String *str);

static void
do_method(Object *o, Method *m, int msgid, int age, int ticks, Objid player,
    Objid from, Objid to, const char *message, List *args)
{
    frame.sp = 0; frame.pc = 0;
    frame.nvars = var_count_local(m);
    var_init_local(o, m, frame.stack);
    frame.sp += frame.nvars;
    frame.msgid = msgid;
    frame.age = age;
    frame.ticks = ticks;
    frame.player = player;
    frame.caller = from;
    frame.this = to;
    frame.on = o;
    frame.on->ref++;
    frame.args = list_dup(args);
    frame.m = m;
    frame.m->ref++;
    frame.last_opcode = -1;
    pushn(-1);	/* the terminator */
    ex_state = RUNNING;
    execute();
}
    
Error
call_method(int msgid, int age, int ticks, Objid player, Objid from, Objid to,
	    const char *message, List *args, Objid on)
{
    Method	*m;
    Object	*o = retrieve(on);

    if (!o) {
	return E_OBJNF;
    } else if ((m = find_method_recursive(o, message, &o))) {
	do_method(o, m, msgid, age, ticks, player, from, to, message, args);
	return E_NONE;
    } else {		/* not found */
	return E_METHODNF;
    }
}

/*
 * call_verb() - Call a verb on the local object
 *
 * Trashes "cmd", but does not free it
 *
 */

Error
call_verb(int msgid, int age, int ticks, Objid player, Objid from, Objid to,
	  List *args)
{
    char	*cmd, *start, *end, *argstr;
    Error	 r;

    if (args->len != 1) {
	return E_NARGS;
    } else if (args->el[0].type != STR) {
	return E_ARGTYPE;
    }
/*
 * parse out verb (first word)
 */

    cmd = str_dup(args->el[0].v.str->str);
    start = cmd;
    while (isspace(*start)) {
	start++;
    }
    if (!*start) {
	FREE(cmd);
	return E_VERBNF;
    }
    end = start;
    while (*end && !isspace(*end))
	end++;
    if (*end) {
	argstr = end + 1;
	*end = '\0';
    } else {
	argstr = end;
    }
    r = call_verb_recursive(retrieve(to), msgid, age, ticks, player, from, to,
			       start, argstr);
    FREE(cmd);
    return r;
}

/*
 * call_verb_recursive()
 *
 * Attempts to find a verb on an object, or any of its ancestors.
 * If one is found, the associated method is called, again starting
 * at the base object.
 *
 * Return values:  E_VERBNF indicates no verb was found;
 *		   E_NONE indicates a verb was found.
 */

static Error
call_verb_recursive(Object *o, int msgid, int age, int ticks, Objid player,
		    Objid from, Objid to, const char *verb, const char *argstr)
{
    Verbdef	*v;
    const char	*dobj = 0, *prep = 0, *iobj = 0;
    int		 dobjlen = 0, preplen = 0;
    List	*args, *parents;
    int		 i;
    Error	 r;

    if (!o) {
	return E_VERBNF;  /* not a valid object, therefore no verb found */
    }
    for (v = o->verbs; v; v = v->next) {	/* for all verbs */
	if (verb_match(sym_get(o, v->verb)->str, verb)) {
	    if (v->prep == -1) {
		dobj = argstr;
		dobjlen = strlen(argstr);
	    } else if (!prep_match(sym_get(o, v->prep)->str, argstr,
			&dobj, &prep, &iobj, &dobjlen, &preplen)) {
		continue;	/* prep doesn't match; keep looking */
	    }
	    args = list_new(4);
	    args->el[0].type = STR;
	    args->el[0].v.str = string_cpy(verb);
	    args->el[1].type = args->el[2].type = args->el[3].type = STR;
	    if( prep )
	    {
		args->el[1].v.str = string_ncpy(dobj, dobjlen);
	    } else {
		args->el[1].v.str = string_fixquote( 
			string_ncpy(dobj, dobjlen) );
	    }
	    args->el[2].v.str = prep ? string_ncpy(prep, preplen) : sym_sys(BLANK);
	    if( iobj )
	    {
		args->el[3].v.str = string_fixquote(
			string_cpy(iobj));
	    } else {
		args->el[3].v.str = sym_sys(BLANK);
	    }
	    r = call_method(msgid, age, ticks, player, from, to,
		       sym_get(o, v->method)->str, args, to);
	    list_free(args);
	    return r;
	    /* found a match, quit */
	}
    }
    parents = list_dup(o->parents);	/* just in case o gets swapped out */
    for (i = 0; i < parents->len; i++) {
	r = call_verb_recursive(retrieve(parents->el[i].v.obj), msgid, age,
				ticks, player, from, to, verb, argstr);
	if (r != E_VERBNF) {
	    list_free(parents);
	    return r;	/* found a match on ancestor, quit */
	}
    }
    list_free(parents);
    return E_VERBNF;			/* no match found */
}

static void
execute(void)
{
    Op_entry	 *op;		/* index into opcode table */

    ex_retval.type = NUM;
    ex_retval.v.num = 0;
    this = retrieve(frame.this);
    if (!this) {
	ex_state = STOPPED;
    }
    while (ex_state == RUNNING) {
	if ((op = opcodes[frame.m->code[frame.pc]])) {
	    frame.last_opcode = frame.m->code[frame.pc];
	    frame.pc++;
	    if (op->builtin) {
		nargs = count_args();
		if ((op->builtin->maxargs >= 0 && nargs > op->builtin->maxargs)
		 || (nargs < op->builtin->minargs)) {
		    raise(E_NARGS);
		} else {
		    (op->func)();
		}
	    } else {
		(op->func)();
	    }
	    frame.ticks++;
	    if (frame.ticks >= max_ticks) {
		raise(E_TICKS);
	    }
	} else {
	    writelog();
	    fprintf(stderr, "execute(): unknown opcode %d\n",
		    frame.m->code[frame.pc]);
	    raise(E_INTERNAL);
	    break;
	}
    }
    if (ex_state != BLOCKED) {
	clear_locks(this, frame.msgid);
	free_frame(&frame);
    }
    if (ex_state == STOPPED) {
	List	*args;

	if (ex_retval.type == NUM && ex_retval.v.num == 0) {
	    args = list_dup(zero);
	} else {
	    args = list_new(1);
	    args->el[0] = ex_retval;
	}

	(void) send_message(frame.msgid, frame.age, frame.ticks, frame.player,
	frame.this, frame.caller, sym_sys(RETURN), args, 0,
	frame.caller);
    }
}

String *
add_traceback_header(String *str, Error e)
{
    str = string_cat(str, "ERROR:  ");
    str = string_cat(str, err_id2desc(e));
    str = string_cat(str, "\r\nraised in ");
    str = add_traceback(str);
    return str;
}

static String *
add_traceback(String *str)
{
    int		 lineno;

      /* calcluate the offending line # using decompile_method() */
    (void) decompile_method((String *) 0, frame.on, frame.m, 0, 0, 0, 0,
			     frame.pc, &lineno);

    str = string_cat(str, "method \"");
    str = string_cat(str, sym_get(frame.on, frame.m->name)->str);
    str = string_cat(str, "\" on ");
    str = string_catobj(str, frame.on->id, 1);
    str = string_cat(str, ", line ");
    str = string_catnum(str, lineno);
#if 0
    if ((opc = opcodes[frame.last_opcode]->name)) {
	str = string_cat(str, ", last opcode ");
	str = string_cat(str, opc);
    }
    str = string_cat(str, ", PC = ");
    str = string_catnum(str, frame.pc);
#endif
    return str;
}

List *
make_raise_args(Error e)
{
    List	*r = list_new(2);

    r->el[0].type = ERR;
    r->el[0].v.err = e;
    r->el[1].type = STR;
    r->el[1].v.str = string_new(0);
    return r;
}

void
send_raise(List *raise_args)
{
    (void) send_message(frame.msgid, frame.age, frame.ticks, frame.player,
	frame.this, frame.caller, sym_sys(RAISE),
	raise_args, 0, frame.caller);
}

void
raise(Error e)
{
    Var		 ev;
    List	*raise_args;

    ev.type = ERR;  ev.v.err = e;
    if (e != E_STACKOVR) {
	push(ev); 
    }
    if (e == E_NONE) {
	return;
    }
    switch(frame.m->ehandler[e]) {
      case EH_DEFAULT:
	raise_args = make_raise_args(e);
	raise_args->el[1].v.str = add_traceback_header(raise_args->el[1].v.str,
	    e);
	send_raise(raise_args);
	ex_state = RAISED;
	break;
      case EH_IGNORE:
      case EH_CATCH:	/* catch is unimplemented, does an ignore for now */
	break;
    }
}

/*
 * send_message_and_block()
 *
 * Sends a message and queues up an event for the message's reply.
 * As with send_message(), "msg" and "args" are eaten.
 */

void
send_message_and_block(Objid from, Objid to, String *msg, List *args, Objid on)
{
    Event	*e = MALLOC(Event, 1);
    Error	 r;
    Timeval	 now;

    *e = frame;
    e->msg = 0;
    gettimeofday(&now, 0);
    e->timeout_at = now;
    e->timeout_at.tv_sec += MSG_TIMEOUT;
    e->retry_interval = MSG_RETRY;
    e->retry_at = timer_addmsec(now, e->retry_interval);
    r = send_message(-1, frame.age + 1, frame.ticks, frame.player, from,
		    to, msg, args, e, on);
    if (r == E_NONE) {
	ex_state = BLOCKED;
    } else {
	FREE(e);
	raise(r);
    }
}

void
resume_method_return(Event *e, Var retval)
{
    frame = *e;				/* restore execution parameters */
    ex_state = RUNNING;			/* start the beast */
    retval = var_dup(retval);
    push(retval);			/* push return value */
    execute();
}

void
resume_method_raise(Event *e, List *args)
{
    frame = *e;				/* restore execution parameters */
    switch(e->m->ehandler[args->el[0].v.err]) {
      case EH_DEFAULT:
	if (args->el[1].v.str->str[0]) {
	    args->el[1].v.str = string_cat(args->el[1].v.str, "\r\ncalled from ");
	    args->el[1].v.str = add_traceback(args->el[1].v.str);
	} else {
	    args->el[1].v.str = add_traceback_header(args->el[1].v.str,
				 args->el[0].v.err);
	}
	send_raise(list_dup(args));
	ex_state = RAISED;
	break;
      case EH_IGNORE:
      case EH_CATCH:
	push(args->el[0]);
	ex_state = RUNNING;
	break;
    }
    execute();
}

void
resume_method_halt(Event *e)
{
    frame = *e;
    ex_state = HALTED;
    execute();
}

void
resume_method(Event *e)
{
    frame = *e;
    ex_state = RUNNING;
    execute();
}

static void
free_frame(Event *e)
{
    Var		v;
    while(e->sp) {
	v = pop();  var_free(v);
    }
    list_free(e->args);
    free_method(e->on, e->m);
    free_object(e->on);
}

static void
clear_locks(Object *o, int msgid)
{
    Lock	*l, *prev = 0, *next;

    if (!o) return;

    for (l = o->locks; l; l = next) {
	next = l->next;
	if (l->added_by == msgid) {
	    if (prev) {
		prev->next = l->next;
	    } else {
		o->locks = l->next;
	    }
	    string_free(l->name);
	    FREE(l);
	} else {
	    prev = l;
	}
    }
}

void
push(Var v)
{
    if (frame.sp >= STACK_SIZE) {
	raise(E_STACKOVR);
    } else {
	frame.stack[frame.sp++] = v;
    }
}

Var
pop(void)
{
    if (frame.sp <= 0) {
	raise(E_STACKUND);
	return zero->el[0];
    } else {
	return frame.stack[--frame.sp];
    }
}

void
pushpc(int i)
{
    Var		n;

    n.type = PC;
    n.v.num = i;
    push(n);
}

void
pushn(long i)
{
    Var		n;

    n.type = NUM;
    n.v.num = i;
    push(n);
}

Var
pop_args(int num)
{
    Var		r;

    r.type = LIST;
    r.v.list = list_new(num);
    for (; num; num--) {
	r.v.list->el[num - 1] = pop();
    }
    return r;
}

/* count_args()
 *
 * Rifle through the stack looking for an ARGSTART.  Shift all the
 * arguments on the stack down by one so that the argmarker is 
 * written over.  Return the number of arguments.
 */

int
count_args(void)
{
    int		i;		/* argcount: index from current top of stack */
    Var		sval, pval;	/* value in sp - i position */

    pval.type = NUM;  pval.v.num = 0;

    for (i = 0; i < frame.sp; i++) {
	sval = frame.stack[frame.sp - i - 1];
	frame.stack[frame.sp - i - 1] = pval;
	if (sval.type == PC && sval.v.num == -ARGSTART) {
	    frame.sp--;
	    return i;
	}
	pval = sval;
    }
    raise(E_STACKUND);
    return -1;
}

/*
 * psub() - do % substitutions from variables
 * 
 * `s' is the string to be substituted on.  Returns a String containing
 * the result:  Each %foo is replaced by variable 'foo'.
 *
 * NOTE:  psub() is in here (execute.c) because it needs access to the 
 *	  stack for local vars.
 */

static String	*dosub(String *r, const char *vname, int vnamelen);

String *
psub(const char *s)
{
    String	*r = string_new(0);
    int		 state = 0;
    const char	*vname = s;

    while(*s) {
	switch(state) {
	  case 0:		/* waiting for a % */
	    if (*s == '%') {
		state = 1;
	    } else {
		r = string_catc(r, *s);
	    }
	    break;
	  case 1:			/* got %, waiting for a varname */
	    if (*s == '%') {		/* got %%, */
		r = string_catc(r, '%');	/* append % */
		state = 0;
	    } else {
		vname = s;		/* got varname start */
		state = 2;
	    }
	    break;
	  case 2:
	    if (*s == '%') {		/* got %foo%, do substitution */
		r = dosub(r, vname, s - vname);
		state = 0;
	    } else if (!isalnum(*s)) {	/* got %foo, do substitution */
		r = dosub(r, vname, s - vname);
		r = string_catc(r, *s);	/* append terminating char */
		state = 0;
	    }
	    break;
	}
	s++;
    }
    if (state == 2) {
	r = dosub(r, vname, s - vname);
    }
    return r;
}

static String *
dosub(String *r, const char *vname, int vnamelen)
{
    int		varno;
    char	*start;

    if (var_find_local_by_name(frame.on, frame.m, vname, vnamelen, &varno)) {
	start = r->str + r->len;
	r = var_tostring(r, frame.stack[varno], 0);
	if (isupper(vname[0]) && islower(*start)) {	/* if caps sub, */
	    *start = toupper(*start);			/* capitalize result */
	}
    }
    return r;
}

Timeval
timer_sub(Timeval t1, Timeval t2)
{
    t1.tv_sec -= t2.tv_sec;
    t1.tv_usec -= t2.tv_usec;
    if (t1.tv_usec < 0) {
	t1.tv_usec += 1000000;
	t1.tv_sec--;
    }
    return t1;
}

Timeval
timer_addmsec(Timeval t, int msec)
{
    t.tv_sec += msec / 1000;
    t.tv_usec += (msec % 1000) * 1000;
    if (t.tv_usec > 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }
    return t;
}
