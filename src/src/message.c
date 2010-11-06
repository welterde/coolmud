/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "netio.h"
#include "servers.h"
#include "execute.h"

#define ISREPLY(message) (!cool_strcasecmp(message->str, "return") \
		       || !cool_strcasecmp(message->str, "raise") \
		       || !cool_strcasecmp(message->str, "halt"))

typedef struct Message Message;

struct Message {
    int		 msgid;
    int		 age;
    int		 ticks;
    Objid	 player;
    Objid	 from;
    Objid	 to;
    String      *message;
    List	*args;
    Objid	 on;
    Message	*prev;
    Message	*next;
};

Event		*event_head, *event_tail;
Message		*msg_head, *msg_tail;

static Message	*find_reply(Event *e);
static int	 find_lock(Object *o, const char *name, int msgid);
static int	 process_event(Event *e, Timeval cur_time, Timeval *timeout);
static int	 process_message_event(Event *e, Timeval cur_time,
						 Timeval *timeout);
static int	 process_sys_message_event(Event *e, Timeval cur_time,
						     Timeval *timeout);
static int	 process_lock_event(Event *e, Timeval cur_time,
		    			      Timeval *timeout);
static int	 process_timer_event(Event *e, Timeval cur_time,
					       Timeval *timeout);
static int	 next_msgid = 0;

static void	 event_free_assoc(Event *e);
static void	 msg_add(Message *m), msg_rm(Message *m);
static String	*compose_var(String *st, Var  v);
static String	*compose_list(String *st, List *list);
static String	*compose_map(String *st, Map *map);
static void	 do_parse(Playerid player, int msg, List *args);

/*
 * process_queues()
 *
 * Process the message and event queues.  Any incoming reply messages are
 * checked against waiting events.  If a matching event is found,
 * the event is resumed and removed.  If not, the message is discarded.
 * Each incoming non-reply message calls call_method().
 * The time value pointed to by 'timeout' is updated to reflect the next time
 * that the server must call process_queues() (for timeouts, retrys, etc)
 */

void
process_queues(Timeval cur_time, Timeval *timeout)
{
    Event	*e, *enext;
    Message	*m, *mnext;
    Timeval	 newtime;

    int		events_handled, msgs_handled;

    do {	/* continue both loops while there are any messages */
	msgs_handled = 0;
	events_handled = 0;
	for (e = event_head; e; e = enext) {
	    enext = e->next;
	    if (process_event(e, cur_time, timeout)) {
		event_rm(e);
		events_handled++;
	    } else {
		newtime = timer_sub(e->timeout_at, cur_time);
		*timeout = timercmp(&newtime, timeout, <) ? newtime : *timeout;
	    }
	}
	for (m = msg_head; m; m = mnext) {	/* process all messages */
	    mnext = m->next;

	    if (!ISREPLY(m->message)) {
		if (!valid(m->to)) {
		    send_message(m->msgid, m->age, m->ticks, m->player, m->to,
			m->from,
			sym_sys(RAISE), make_raise_args(E_OBJNF), 0, m->from);
		} else if (!cool_strcasecmp(m->message->str, "call_verb")) {
		    Error r = call_verb(m->msgid, m->age, m->ticks, m->player,
		   	   m->from, m->to, m->args);
		    if (r == E_VERBNF) {
			send_message(m->msgid, m->age, m->ticks, m->player,
			m->to,
			m->from, sym_sys(RETURN), list_dup(one), 0, m->from);
		    } else if (r != E_NONE) {
			send_message(m->msgid, m->age, m->ticks, m->player,
			m->to, m->from, sym_sys(RAISE), make_raise_args(r), 0,
			    m->from);
		    }
		} else if (call_method(m->msgid, m->age, m->ticks, m->player,
	    m->from, m->to, m->message->str, m->args, m->on) != E_NONE) {
		    send_message(m->msgid, m->age, m->ticks, m->player, m->to,
				 m->from, sym_sys(RAISE),
				 make_raise_args(E_METHODNF), 0, m->from);
		}
		msg_rm(m);
		msgs_handled++;
	    }
	}
    } while (msgs_handled || events_handled);
/*
 * remove orphaned messages
 */
    for (m = msg_head; m; m = mnext) {		/* process all messages */
	mnext = m->next;
	if (ISREPLY(m->message)) {
	    msg_rm(m);
	}
    }
    cache_reset();
    if (timeout->tv_sec < 0) {
	timeout->tv_sec = 0;
    }
}

static int
process_event(Event *e, Timeval cur_time, Timeval *timeout)
{
    List	*args;

    switch (e->blocked_on) {
      case BL_MESSAGE:
	return process_message_event(e, cur_time, timeout);
      case BL_LOCK:
	return process_lock_event(e, cur_time, timeout);
      case BL_TIMER:
	return process_timer_event(e, cur_time, timeout);
      case BL_SYS_MESSAGE:
	return process_sys_message_event(e, cur_time, timeout);
      case BL_DEAD:
	args = make_raise_args(E_TERM);
	resume_method_raise(e, args);  /****/
	list_free(args);
	return 1;
    }
    return 0;
}

static int
process_message_event(Event *e, Timeval cur_time, Timeval *timeout)
{
    Message	*m = find_reply(e);
    Timeval	 newtime;

    if (m) {
	e->ticks = m->ticks;
	if (!cool_strcasecmp(m->message->str, "return")) {
	    resume_method_return(e, m->args->el[0]);
	} else if (!cool_strcasecmp(m->message->str, "raise")) {
	    resume_method_raise(e, m->args);
	} else if (!cool_strcasecmp(m->message->str, "halt")) {
	    resume_method_halt(e);
	}
	msg_rm(m);
	return 1;
    } else if (e->blocked_objid.server) {	/* if blocked on remote msg */
	if (timercmp(&cur_time, &e->timeout_at, >)) {
	    List	*args = make_raise_args(E_TIMEOUT);
	    resume_method_raise(e, args);  /****/
	    list_free(args);
	    return 1;
	} else if (timercmp(&cur_time, &e->retry_at, >)) {
#ifdef EXP_BACKOFF
	    e->retry_interval *= 2;
#endif
	    e->retry_at = timer_addmsec(cur_time, e->retry_interval);
	    if (yo(e->blocked_objid.server, e->msg->str)) {
		List	*args = make_raise_args(E_TIMEOUT);
		resume_method_raise(e, args);  /****/
		list_free(args);
		return 1;
	    }
	}
	newtime = timer_sub(e->retry_at, cur_time);
	*timeout = timercmp(&newtime, timeout, <) ? newtime : *timeout;
	return 0;
    } else {
	return 0;
    }
}

static int
process_sys_message_event(Event *e, Timeval cur_time, Timeval *timeout)
{
    Message	*m = find_reply(e);

    if (m) {
	switch(e->msgid) {
	  case PARSE:
	    if (!cool_strcasecmp(m->message->str, "raise")) {
	        tell(m->player.id, m->args->el[1].v.str->str, 1 );
	    }
	    break;
	}
	msg_rm(m);
	return 1;
    } else if (timercmp(&cur_time, &e->timeout_at, >)) {
	tell(e->player.id, "Parse timed out; programmer error.", 1);
	return 1;
    } else {
	return 0;
    }
}

static int
process_lock_event(Event *e, Timeval cur_time, Timeval *timeout)
{
    if (!find_lock(retrieve(e->this), e->lock->str, e->msgid)) {
	resume_method(e);
	return 1;
    } else if (timercmp(&cur_time, &e->timeout_at, >)) {
	List	*args = make_raise_args(E_TIMEOUT);
	resume_method_raise(e, args);  /****/
	list_free(args);
	return 1;
    } else {
	return 0;
    }
}

static int
process_timer_event(Event *e, Timeval cur_time, Timeval *timeout)
{
    if (timercmp(&cur_time, &e->timeout_at, >)) {
	resume_method(e);
	return 1;
    } else {
	return 0;
    }
}

static Message *
find_reply(Event *e)
{
    Message	*m;

    for (m = msg_head; m; m = m->next) {
	if (m->msgid == e->blocked_msgid && ISREPLY(m->message)) {
	    return m;
	}
    }
    return 0;
}

static int
find_lock(Object *o, const char *name, int msgid)
{
    Lock	*l;

    for (l = o->locks; l; l = l->next) {
	if (!cool_strcasecmp(l->name->str, name)) {
	    if (l->added_by == msgid) {		/* our lock comes first */
		return 0;
	    } else {			/* some other lock comes first */
		return 1;
	    }
	}
    }
    return 0;
}

/*
 * send_message() - da big one
 *
 * Send a message from one object to another.  If the object is local,
 * the message is queued directly.  If the object is remote, the message
 * is converted to ASCII and sent out the network port.  In either case,
 * the "msg" and "args" arguments are eaten (ie., they must be allocated
 * prior to the call).  If msgid is negative, a new msgid is generated
 * for this message (ie., it's an original message).  Otherwise, msgid
 * is used (it's a response).  If the server requested is down, E_SERVERDN
 * is returned.  If the age exceeds max_age, E_MAXREC is returned.
 */

Error
send_message(int msgid, int age, int ticks, Objid player, Objid from, Objid to,
	     String *msg, List  *args, Event *e, Objid on)
{
    String	*message;
    Message	*m;

    if (msgid < 0) {
	if (age >= max_age && max_age > 0) {
	    string_free(msg);  list_free(args);
	    return E_MAXREC;
	}
	msgid = next_msgid++;
    }

/* 
 * if it's a local message, queue it directly
 */
    if (!to.server) {
	m = MALLOC(Message, 1);
	m->from = from;
	m->to = to;
	m->player = player;
	m->message = msg;
	m->args = args;
	m->msgid = msgid;
	m->age = age;
	m->ticks = ticks;
	m->on = on;
	msg_add(m);
    } else {
	message = string_new(0);
	message = string_catnum(message, msgid);
	message = string_catc(message, ' ');
	message = string_catnum(message, age);
	message = string_catc(message, ' ');
	message = string_catnum(message, ticks);
	message = string_catc(message, ' ');
	message = string_catobj(message, player, 1);
	message = string_catc(message, ' ');
	message = string_catobj(message, from, 1);
	message = string_catc(message, ' ');
	message = string_catobj(message, to, 1);
	message = string_cat(message, " \"");
	message = string_backslash(message, msg->str);
	message = string_cat(message, "\" ");
	message = compose_list(message, args);
	message = string_catc(message, '\n');
	string_free(msg);
	list_free(args);
#ifdef LOG_YO
	writelog();
	fprintf(stderr, "SENT:  %s", message->str);
#endif /* LOG_YO */
	if (yo(to.server, message->str)) {
	    string_free(message);
	    return E_SERVERDN;
	} else if (e) {
	    e->msg = message;
	} else {
	    string_free(message);
	}
    }
    if (e) {
	e->blocked_on = BL_MESSAGE;
	e->blocked_msgid = msgid;
	e->blocked_objid = to;
	event_add(e);
    }
    return E_NONE;
}

/*
 * msg_add()
 *
 * Add a message to the message queue
 */

static void
msg_add(Message  *m)
{
    m->next = m->prev = 0;
    if (!msg_head) {
	msg_head = msg_tail = m;
    } else {
	msg_tail->next = m;
	m->prev = msg_tail;
	msg_tail = m;
    }
}

/*
 * event_add()
 *
 * Add an event to the event queue.
 */

void
event_add(Event  *e)
{
    e->next = e->prev = 0;
    if(!event_head) {
	event_head = event_tail = e;
    } else {
	event_tail->next = e;
	e->prev = event_tail;
	event_tail = e;
    }
}

/*
 * event_rm()
 *
 * Removes an event from the event queue.
 * Frees the space used by the event.
 */

void
event_rm(Event *e)
{
    if(e->next) {
	e->next->prev = e->prev;
    } else {
	event_tail = e->prev;
    }
    if(e->prev) {
	e->prev->next = e->next;
    } else {
	event_head = e->next;
    }
    event_free_assoc(e);
    FREE(e);
}

/*
 * event_free_assoc()
 *
 * Frees the space associated with an Event.  Does not free the event itself.
 */

static void
event_free_assoc(Event *e)
{
    if (e->blocked_on == BL_LOCK) {
	string_free(e->lock);
    } else if (e->blocked_on == BL_MESSAGE && e->msg) {
	string_free(e->msg);
    }
}

/*
 * msg_rm()
 *
 * Remove a message from the message queue.
 * Frees the space used by the message structure, its message, and its
 * arguments.
 */

static void
msg_rm(Message *m)
{
    if(m->next) {
	m->next->prev = m->prev;
    } else {
	msg_tail = m->prev;
    }
    if(m->prev) {
	m->prev->next = m->next;
    } else {
	msg_head = m->next;
    }
    list_free(m->args);
    string_free(m->message);
    FREE(m);
}

Message *
parse_message (const char *msg);

/*
 * receive_message()
 *
 * Receive and parse a message from a remote server.  If the message is
 * parseable, it is added to the queue.  Otherwise, an error is written
 * to the log, and the message is discarded.
 */

int
receive_message(Serverid server, const char *msg)
{
    Message	*m;

    if ((m = parse_message(msg))) {
#ifdef LOG_YO
	writelog();
	fprintf(stderr, "RECD:  %s", msg);
#endif /* LOG_YO */
	if (!ISREPLY(m->message)
	  && serv_discardmsg(m->from.server, m->msgid)) {
	    list_free(m->args);
	    string_free(m->message);
	    FREE(m);
	} else {
	    msg_add(m);
	}
	return 0;
    } else {
	writelog();
	fprintf(stderr, "UNPARSEABLE msg from server #%d: %s\n", server, msg);
	return 1;
    }
}

/*
 * parse()
 * 
 * Receive a command from a player, and queue up a "parse" message
 * from SYS_OBJ to the player object.  The "return" to this message
 * is ignored.  This function is called by the interface when
 * input comes from a player's socket.
 *
 */

void
parse(Playerid player, const char *command, int fd)
{
    List	*args = list_new(2);

    args->el[0].type = STR;
    args->el[0].v.str = string_cpy(command);
    args->el[1].type = NUM;
    args->el[1].v.num = fd;
    do_parse(player, PARSE, args);
}

void
new_connect(Playerid player, int fd)
{
    List	*args = list_new(1);

    args->el[0].type = NUM;
    args->el[0].v.num = fd;
    do_parse(player, SYM_CONNECT, args);
}

static void
do_parse(Playerid player, int msg, List *args)
{
    Message	*m = MALLOC(Message, 1);
    Event	*e = MALLOC(Event, 1);
    Timeval	 cur_time;

    gettimeofday(&cur_time, 0);
    m->msgid = e->blocked_msgid = next_msgid++;
    m->age = m->ticks = 0;
    m->player.id = player;
    m->player.server = 0;
    m->from.id = player;
    m->from.server = 0;
    m->to.id = player;
    m->to.server = 0;
    m->message = sym_sys(msg);
    m->args = args;
    m->on.id = player;
    m->on.server = 0;

    e->msg = 0;
    gettimeofday(&e->timeout_at, 0);
    e->m = 0;
    e->timeout_at.tv_sec += MSG_TIMEOUT * 2;
    e->blocked_on = BL_SYS_MESSAGE;
    e->msgid = PARSE;			/* system message 'PARSE' */

    msg_add(m);  event_add(e);
}

/*
 * These routines are used to parse incoming messages from remote servers.
 */

const char *parse_num(const char *s, int *num);
const char *parse_obj(const char *s, Objid *obj);
const char *parse_string(const char *s, String **str);
const char *parse_id(const char *s, char **id);
const char *parse_list(const char *s, List **list);
const char *parse_err(const char *s, Error *err);
const char *parse_var(const char *s, Var *v);
const char *parse_map(const char *s, Map **map);

Message *
parse_message (const char *msg)
{
    Message	*m = MALLOC(Message, 1);

    m->args = 0;
    m->message = 0;

    if (!(msg = parse_num(msg, &m->msgid))
     || !(msg = parse_num(msg, &m->age))
     || !(msg = parse_num(msg, &m->ticks))
     || !(msg = parse_obj(msg, &m->player))
     || !(msg = parse_obj(msg, &m->from))
     || !(msg = parse_obj(msg, &m->to))
     || !(msg = parse_string(msg, &m->message))
     || !(msg = parse_list(msg, &m->args))) {
	if (m->message) {
	    FREE(m->message);
	}
	if (m->args) {
	    list_free(m->args);
	}
	FREE(m);
	return 0;
    }
    m->on = m->to;		/* look for methods on 'to' object */
    return m;
}

const char *
parse_num(const char *msg, int *num)
{
    int		neg = 0;
    *num = 0;
    while (isspace(*msg))
	msg++;
    if (*msg == '-') {
	neg = 1;
	msg++;
    }
    if (!isdigit(*msg)) {
	return 0;
    }
    do {
	*num = *num * 10 + *msg++ - '0';
    } while (isdigit(*msg));
    *num = neg ? -*num : *num;
    return msg;
}

const char *
parse_obj(const char *msg, Objid *obj)
{
    char	*serv;

    while (isspace(*msg))
	msg++;
    if (*msg++ != '#') {
	return 0;
    }
    if (!(msg = parse_num(msg, &obj->id))) {
	return 0;
    }
    while (isspace(*msg))
	msg++;
    if (*msg != '@') {
	obj->server = 0;
	return msg;
    }
    msg++;
    if (!(msg = parse_id(msg, &serv))) {
	return 0;
    }
    if ((obj->server = serv_name2id(serv)) < 0) {
	FREE(serv);
	return 0;
    }
    FREE(serv);
    return msg;
}

const char *
parse_id(const char *msg, char **id)
{
    int		 len = 0;
    const char	*s;

    while (isspace(*msg))
	msg++;
    if (*msg && !isalpha(*msg) && *msg != '_') {
	return 0;
    }
    s = msg;
    while (*msg && (isalnum(*msg) || *msg == '_')) {
	len++; msg++;
    }
    *id = strndup(s, len);
    return msg;
}

const char *
parse_string(const char *s, String **r)
{
    int		 n;
    const char	*start;
    while (isspace(*s))
	s++;
    if (*s++ != '"') {
	return 0;
    }
    for (start = s, n = 0; *s != '"'; s++, n++) {
	if (*s == '\\') {	/* escaped char (eg., '\n') */
	    s++;
	} else if (!*s) {	/* non-terminated string */
	    return 0;
	}
    }
    *r = string_new(n);
    for (s = start; n; s++, n--) {
	if (*s == '\\') {
	    switch (*++s ) {
	      case 'n':
		*r = string_cat(*r, "\r\n");
		break;
	      case 't':
		*r = string_catc(*r, '\t');
		break;
	      default:
		*r = string_catc(*r, *s);
		break;
	    }
	} else {
	    *r = string_catc(*r, *s);
	}
    }
    return ++s;
}

/*
 * compose_list()
 *
 * Converts a list to YO ASCII format, in the form of a String.
 * The contents are surrounded by braces and separated by
 * spaces.  The first value indicates the length of the list.  This
 * routine should only be used by functions inteded to send YO
 * packets onto the net (ie., not for COOL).  The ASCII
 * representation is appended to the buffer.  (ie., the buffer is
 * *not* initialized.)
 */

static String *
compose_var(String *st, Var v)
{
    switch (v.type) {
      case LIST:
	st = compose_list(st, v.v.list); 
	break;
      case MAP:
	st = compose_map(st, v.v.map);
	break;
      case STR:
	st = string_catc(st, '"');
	st = string_backslash(st, v.v.str->str);
	st = string_catc(st, '"');
	break;
      case NUM:
	st = string_catnum(st, v.v.num);
	break;
      case OBJ:
	st = string_catobj(st, v.v.obj, 1);
	break;
      case ERR:
	st = string_cat(st, err_id2name(v.v.err));
	break;
      case PC:		/* shouldn't happen */
	break;
    }
    return st;
}

static String *
compose_list(String *st, List *list)
{
    int		i;

    st = string_cat(st, "{ ");
    st = string_catnum(st, list->len);
    st = string_catc(st, ' ');
    for (i = 0; i < list->len; i++) {
	st = compose_var(st, list->el[i]);
	st = string_catc(st, ' ');
    }
    st = string_catc(st, '}');
    return st;
}

static String *
compose_map(String *st, Map *map)
{
    int		 i;
    MapPair	*pair;

    st = string_cat(st, "[ ");
    st = string_catnum(st, map->num);
    st = string_catc(st, ' ');
    for (i = 0; i < map->size; i++) {
	for (pair = map->table[i]; pair; pair = pair->next) {
	    st = compose_var(st, pair->from);
	    st = string_catc(st, ' ');
	    st = compose_var(st, pair->to);
	    st = string_catc(st, ' ');
	}
    }
    st = string_cat(st, "]");
    return st;
}

const char *
parse_map(const char *s, Map **map)
{
    int		 i, num;
    Var		 from, to;

    while (isspace(*s)) s++;
    if (*s++ != '[') {
	return 0;
    } else if (!(s = parse_num(s, &num))) {
	return 0;
    }
    *map = map_new(num);
    for (i = 0; i < num; i++) {
	if (!(s = parse_var(s, &from))) {
	    return 0;
	} else if (!(s = parse_var(s, &to))) {
	    return 0;
	}
	*map = map_add(*map, from, to);
    }
    while (isspace(*s))
	s++;
    if (*s++ != ']') {
	return 0;
    }
    return s;
}

const char *
parse_list(const char *s, List **list)
{
    int		i, len;

    while (isspace(*s))
	s++;
    if (*s++ != '{') {
	return 0;
    }
    if (!(s = parse_num(s, &len))) {
	return 0;
    }
    *list = list_new(len);
    for (i = 0; i < len; i++) {
	if (!(s = parse_var(s, &((*list)->el[i])))) {
	    (*list)->len = i;	/* abort free_list() before current value */
	    return 0;
	}
    }
    while (isspace(*s))
	s++;
    if (*s++ != '}') {
	return 0;
    }
    return s;
}

const char *
parse_err(const char *s, Error *err)
{
    const char	*end;
    char	*name;

    while (isspace(*s))
	s++;
    if (*s != 'E') {
	return 0;
    }
    if (!(end = index(s, ' '))) {
	end = index(s, '\0');
    }
    name = strndup(s, end - s);
    *err = err_name2id(name);
    FREE(name);
    return end;
}

const char *
parse_var(const char *s, Var *v)
{
    while (isspace(*s))
	s++;
    if (*s == '"') {
	v->type = STR;
	return parse_string(s, &v->v.str);
    } else if (isdigit(*s) || *s == '-') {
	v->type = NUM;
	return parse_num(s, &v->v.num);
    } else if (*s == '#') {
	v->type = OBJ;
	return parse_obj(s, &v->v.obj);
    } else if (*s == '{') {
	v->type = LIST;
	return parse_list(s, &v->v.list);
    } else if (*s == '[') {
	v->type = MAP;
	return parse_map(s, &v->v.map);
    } else if (*s == 'E') {
	v->type = ERR;
	return parse_err(s, &v->v.err);
    } else {
	return 0;
    }
}

List *
ps(void)
{
    List	*l;
    Var		*el;
    Event	*e;
    int		 len;

    len = 0;
    for (e = event_head; e; e = e->next) {
	if (e->blocked_on == BL_SYS_MESSAGE) continue;
	len++;
    } /* for */

    l = list_new(len);
    
    len = 0;
    for (e = event_head; e; e = e->next) {
	if (e->blocked_on == BL_SYS_MESSAGE) continue;
	l->el[len].type = LIST;
	l->el[len].v.list = list_new(11);
	el = l->el[len].v.list->el;
	el[0].type = NUM;   el[0].v.num = e->msgid;
	el[1].type = NUM;   el[1].v.num = e->age;
	el[2].type = NUM;   el[2].v.num = e->ticks;
	el[3].type = OBJ;   el[3].v.obj = e->player;
	el[4].type = OBJ;   el[4].v.obj = e->this;
	el[5].type = OBJ;   el[5].v.obj = e->on->id;
	el[6].type = OBJ;   el[6].v.obj = e->caller;
	el[7].type = LIST;  el[7].v.list = list_dup(e->args);
	el[8].type = STR;
	el[8].v.str = string_dup(sym_get(e->on, e->m->name));
	el[9].type = NUM;   el[9].v.num = e->blocked_on;
	el[10].type = NUM;  el[10].v.num = e->timeout_at.tv_sec;
	len++;
    } /* for */
    return l;
} /* ps() */

int
cmkill(int pid, Objid who) {
    Event	*e;

    for (e = event_head; e; e = e->next) {
	if (e->msgid == pid && e->blocked_on != BL_SYS_MESSAGE) {
	    if ((e->player.id == who.id && e->player.server == who.server)
		|| is_wizard(who)) {
		event_free_assoc(e);
		e->blocked_on = BL_DEAD;
		e->timeout_at.tv_sec = 0;
		return 0;
	    } else {
		return -2;
	    } /* if */
	} /* if */
    } /* for */
    return -1;
} /* cmkill() */

void
cmkill_all(void)
{
    Event	*e;
    struct timeval	dummy = {0, 0};

    for (e = event_head; e; e = e->next) {
	if (e->blocked_on != BL_SYS_MESSAGE) {
	    event_free_assoc(e);
	    e->blocked_on = BL_DEAD;
	    e->timeout_at.tv_sec = 0;
	}
    }
    process_queues(dummy, &dummy);
}
