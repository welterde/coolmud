/* Copyright (c) 1993 Stephen F. White */

#ifndef EXECUTE_H
#define EXECUTE_H

#ifdef PROTO
extern void	gettimeofday(struct timeval *tp, struct timezone *tzp);
#endif

typedef struct timeval Timeval;

typedef struct Event Event;

struct Event {	
    int		msgid;		/* current message id */
    int		age;		/* current message's age */
    int		ticks;		/* current task's ticks */
    Objid	player;		/* current player */
    Objid       this;		/* object to which message was sent */
    Object     *on;		/* object on which method is located */
    Objid	caller;		/* object which sent the message */
    List       *args;		/* arguments to message */
    Method     *m;		/* method which was being executed */

      /* the following fields are used when the method gets blocked */
    enum { BL_MESSAGE, BL_LOCK, BL_TIMER, BL_SYS_MESSAGE, BL_DEAD }
				blocked_on;
				/* what the method is blocked on */
    Timeval	timeout_at;	/* time at which message should time out */
    int		blocked_msgid;	/* message for which MESSAGE event is waiting */
    Objid	blocked_objid;	/* object to which MESSAGE event was sent */
    String     *msg;		/* if sent to remote, text of message */
    Timeval	retry_at;	/* absolute time to retry at */
    int		retry_interval;	/* relative msec on next retry */

    String     *lock;		/* name of lock which LOCK is blocked on */

    int		sp;		/* stack pointer */
    int		pc;		/* program counter */
    Var	        stack[STACK_SIZE];	/* the stack */
    int         nvars;		/* and how many of them */
    int		last_opcode;	/* useful for error recovery */
    Event      *prev;		/* previous event in queue */
    Event      *next;		/* next event in queue */
};

/*
 * Execution globals
 */

extern Event	 	 frame;		/* the currently executing frame */
extern enum state	 ex_state;	/* execution state */
extern Var		 ex_retval;	/* return value (set by op_return()) */
extern Object		*this;		/* this object, may be 0 */
extern int		 nargs;		/* number of arguments to function */

/*
 * from execute.c
 */

extern void		 push(Var v);
extern Var		 pop(void);
extern Var		 pop_args(int num);	/* pop an argument list */
extern void		 pushn(long i);	/* push an integer onto the stack */
extern void		 pushpc(int i);	/* push the PC onto the stack */
extern void		 raise(Error e);	/* raise an error */
extern void		 send_raise(List *raise_args);	/* send the raise msg */
extern Error		 call_verb(int msgid, int age, int ticks, Objid player,
				   Objid from, Objid to, List *args);
extern Method		*find_method(Object *o, const char *name);
extern void		 send_message_and_block(Objid from, Objid to,
					    String *msg, List *args, Objid on);
extern void		 resume_method_return(Event *e, Var retval);
extern void		 resume_method_raise(Event *e, List *raise_args);
extern void		 resume_method_halt(Event *e);
extern void		 resume_method(Event *e);
extern String		*add_traceback_header(String *str, Error e);
extern List		*make_raise_args(Error e);
extern Timeval		 timer_sub(Timeval t1, Timeval t2);
extern Timeval		 timer_addmsec(Timeval t, int msec);
extern int		 count_args(void);

/*
 * from message.c
 */
extern Error	send_message(int msgid, int age, int ticks, Objid player,
    Objid from, Objid to, String *message, List *args, Event *e, Objid on);
extern void		 event_add(Event *e);
extern void		 event_rm(Event *e);

/*
 * Prototypes for all the opcodes
 */

extern void	op_numpush(void), op_strpush(void), op_objpush(void),
		op_errpush(void), op_listpush(void), op_mappush(void),
		op_pushpc(void), op_if(void), op_elseif(void), op_null(void),
		op_for(void), op_forrng(void), op_while(void), op_do(void),
		op_dowhile(void), op_break(void), op_continue(void),
		op_add(void), op_sub(void), op_mul(void), op_div(void),
		op_mod(void), op_negate(void), op_message(void),
		op_message_expr(void), op_and(void), op_or(void),
		op_not(void), op_index(void), op_subset(void),
		op_lsubset(void), op_rsubset(void), op_splice(void),
		op_gt(void), op_lt(void),
		op_ge(void), op_le(void), op_eq(void), op_ne(void),
		op_echo(void), op_echon(void), op_echo_file(void), op_clone(void),
		op_destroy(void), op_chparents(void), op_time(void),
		op_ctime(void), op_in(void), op_explode(void),
		op_strsub(void), op_psub(void), op_random(void), op_pop(void),
		op_pad(void), op_setadd(void), op_setremove(void),
		op_listinsert(void), op_listappend(void), op_listdelete(void),
		op_listassign(void), op_asgnlvar(void), op_asgngvar(void),
		op_asgngvarindex(void), op_asgnlvarindex(void),
		op_getsysvar(void), op_sort(void),
		op_asgngvarexpr(void), op_getlvar(void), op_getgvar(void),
		op_getgvarexpr(void), op_parents(void), op_this(void),
		op_player(void), op_caller(void), op_args(void),
		op_setplayer(void), op_return(void), op_stop(void),
		op_abort(void), op_crypt(void), op_checkmem(void),
		op_cache_stats(void), op_set_parse(void), op_connect(void),
		op_lock(void), op_unlock(void),
		op_at(void), op_tostr(void), op_tonum(void), op_toobj(void),
		op_toerr(void), op_typeof(void), op_lengthof(void),
		op_serverof(void), op_servername(void), op_servers(void),
		op_verbs(void), op_vars(void), op_methods(void),
		op_verb(void),  op_rmverb(void), op_rmmethod(void),
		op_rmvar(void), op_program(void), op_compile(void),
		op_find_method(void), op_spew_method(void),
		op_list_method(void), op_decompile(void), op_hasparent(void),
		op_objsize(void), op_match(void), op_match_full(void),
		op_strcmp(void), op_f_index(void), op_rindex(void),
		op_tolower(void), op_toupper(void), op_argstart(void),
		op_shutdown(void), op_sync(void), op_disconnect(void),
		op_writelog(void), op_raise(void), op_pass(void),
		op_sleep(void), op_ps(void), op_kill(void);

#endif /* !EXECUTE_H */
