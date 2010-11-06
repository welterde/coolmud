/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "servers.h"
#include "netio.h"
#include "execute.h"

static void	do_match(int full);
static void base_echo( char newline );

/*********
 * base_echo
 * By Robin Powell
 *
 * Does the echo work, taking an argument for whether to send
 * a newline or not
 ***********/
void
base_echo( char newline )
{
    Var		str, id;
    int		fd = -1;

    if (nargs > 1) {
	id = pop();
	if (id.type != NUM) {
	    var_free(id);  var_free(pop());
	    raise(E_ARGTYPE);
	    return;
	} else {
	    fd = id.v.num;
	}
    }
    str = pop();
    if (str.type != STR) {
	raise(E_ARGTYPE);
    }
    else
    {
	str.v.str = string_output( str.v.str );
	if (fd >= 0) {
	    tell_fd(fd, str.v.str->str, newline);
	    pushn(0);
	} else {
	    tell(frame.this.id, str.v.str->str, newline);
	    pushn(0);
	}
    }
    var_free(str);
}


void
op_echo(void)
{
    base_echo( 1 );
}


/***********
 * op_echon
 * By Robin Powell
 *
 * Echo without the trailing \r\n
 ***********/

void
op_echon(void)
{
    base_echo( 0 );
}

#define LINELEN	256

void
op_echo_file(void)
{
    Var		 str, id;
    FILE	*f;
    char	 line[LINELEN], *newline;
    char	 filename[MAX_PATH_LEN];
    int		 fd = -1;

    if (nargs > 1) {
	id = pop();
	if (id.type != NUM) {
	    var_free(id);  var_free(pop());
	    raise(E_ARGTYPE);
	    return;
	} else {
	    fd = id.v.num;
	}
    }
    str = pop();
    if (str.type != STR) {
	raise(E_ARGTYPE);
    } else if (str_in("..", str.v.str->str)  /* no ..'s allowed */
	    || str.v.str->str[0] == '/') {   /* can't start with a / either */
	raise(E_FILE);
    } else {
	sprintf(filename, "%s/%s", RUNDIR, str.v.str->str);
        if (!(f = fopen(filename, "r"))) {
	    raise(E_FILE);
	} else {
	    while (fgets(line, LINELEN, f)) {
		if ((newline = index(line, '\n'))) *newline = '\0';
		if ((newline = index(line, '\r'))) *newline = '\0';
		if (fd >= 0) {
		    tell_fd(fd, line, 1);
		} else {
		    tell(frame.this.id, line, 1);
		}
	    }
	    fclose(f);
	    pushn(0);
	}
    }
    var_free(str);
}

void
op_connect(void)
{
    Var		hostname, port;
    int		r;

    port = pop();  hostname = pop();

    if (hostname.type != STR || port.type != NUM) {
	raise(E_ARGTYPE);
    } else {
	r = net_open_connect(frame.this.id, hostname.v.str->str,
			     (short) port.v.num);
        if (r < 0) {
	    switch (r) {
	      case -1:			/* address lookup failed */
		raise(E_SERVERNF);
	      case -2:			/* address lookup timed out */
		raise(E_TIMEOUT);
	      case -3:			/* connect() failed */
	        raise(E_SERVERNF);
	      case -4:			/* connection in progress */
	        pushn(-1);
	        break;
	      default:
	        raise(E_SERVERNF);
	    }
	} else {
	    pushn(r);
	}
    }
    var_free(hostname);  var_free(port);
}

void
op_clone(void)
{
    Var		oid;
    Object *o = clone(frame.this);

    oid.type = OBJ;
    if (o) {
	oid.v.obj = o->id;
    } else {
	oid.v.obj.id = -1;
	oid.v.obj.server = 0;
    }
    push(oid);
}

void
op_destroy(void)
{
    destroy(frame.this);
    disconnect(frame.this.id);		/* just in case it's a player */
    this = 0;		/* can't do anything with this object anymore */
    pushn(0);
    ex_state = STOPPED;
}

void
op_chparents(void)
{
    Var		newparents;
    Error	r;

    newparents = pop();
    if (newparents.type != LIST) {
	var_free(newparents);
	raise(E_ARGTYPE);
    } else if ((r = check_parents(frame.caller, this, newparents.v.list))
		== E_NONE) {
	list_free(this->parents);
	this->parents = newparents.v.list;
	cache_put(this, frame.this.id);
	pushn(0);
    } else {
	var_free(newparents);
	raise(r);
    }
}

extern time_t	time( time_t * );

void
op_time(void)
{
    pushn( (long) time( (time_t *) 0));
}

void
op_ctime(void)
{
    Var		arg, ret;
    long	t;

    if (nargs == 0) {
	t = time( (time_t *) 0);
    } else {
	arg = pop();
	if (arg.type != NUM) {
	    raise(E_ARGTYPE);
	    return;
	}
	t = arg.v.num;
    }
    ret.type = STR;
    ret.v.str = string_cpy(ctime((time_t *) &t));
    ret.v.str->str[--ret.v.str->len] = '\0';	/* nuke the newline */
    push(ret);
}

void
op_crypt(void)
{
    Var		a1, a2, r;
    char	salt[3];
    static	char saltstuff[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

    if (nargs > 1) {
	a2 = pop();
	if (a2.type != STR) {
	    var_free(a2);
	    a2 = pop(); var_free(a2);
	    raise(E_ARGTYPE);
	    return;
	} else if (a2.v.str->len != 2) {
	    var_free(a2);
	    a2 = pop(); var_free(a2);
	    raise(E_RANGE);
	    return;
	}
	salt[0] = a2.v.str->str[0];
	salt[1] = a2.v.str->str[1];
	var_free(a2);
    } else {
	salt[0] = saltstuff[cool_random() % strlen(saltstuff)];
	salt[1] = saltstuff[cool_random() % strlen(saltstuff)];
    }
    salt[2] = '\0';
    a1 = pop();
    if (a1.type != STR) {
	var_free(a1);
	raise(E_ARGTYPE);
	return;
    }
    r.type = STR;
    r.v.str = string_cpy(crypt(a1.v.str->str, salt));
    var_free(a1);
    push(r);
}

void
op_checkmem(void)
{
    Var		s;

    s.type = STR;
    s.v.str = string_cpy(check_malloc());
    push(s);
}

void
op_cache_stats(void)
{
    Var		s;

    s.type = STR;
    s.v.str = string_cpy(cache_stats());
    push(s);
}

void
op_explode(void)
{
    Var		str, tokv, ret;
    int		nwords;
    char        *sep;
    char	space[2]=" ";

    sep = space;
    if (nargs > 1) {
	tokv = pop();
	if (tokv.type != STR) {
	    var_free(tokv);  str = pop(); var_free(str);
	    raise(E_ARGTYPE);  return;
	} else if (!tokv.v.str->str[0]) {
	    var_free(tokv);  str = pop(); var_free(str);
	    raise(E_RANGE);  return;
	} else {
	    sep = malloc( strlen( tokv.v.str->str ) + 1 );
	    strcpy( sep, tokv.v.str->str );
	    var_free(tokv);
	}
    }
    str = pop();
    if (str.type != STR) {
	raise(E_ARGTYPE);
    } else {
	nwords = count_words(str.v.str->str, sep);
	ret.type = LIST;
	ret.v.list = explode(str.v.str->str, sep, nwords);
	push(ret);
    }
    var_free(str);
    if( sep != space )
	free( sep );
}

void
op_random(void)
{
    Var		arg;

    arg = pop();
    if (arg.type != NUM) {
	raise(E_ARGTYPE);
    } else if (arg.v.num <= 0) {
	raise(E_RANGE);
    } else {
	pushn(cool_random() % arg.v.num + 1);
    }
}

void
op_setadd(void)
{
    Var		new, list;

    new = pop(); list = pop();
    if (list.type != LIST) {
	var_free(list); var_free(new);
	raise(E_ARGTYPE);
    } else {
	list.v.list = list_setadd(list.v.list, new);
	push(list);
    }
}

void
op_setremove(void)
{
    Var		what, list;

    what = pop(); list = pop();
    if (list.type != LIST) {
	var_free(list);
	raise(E_ARGTYPE);
    } else {
	list.v.list = list_setremove(list.v.list, what);
	push(list);
    }
    var_free(what);
}

void
op_lock(void)
{
    Var		 name;
    Event	*e;
    Lock	*l, *new;

    name = pop();
    if (name.type != STR) {
	raise(E_ARGTYPE);
    } else {
	pushn(0);		/* lock() always returns 0 */
	for (l = this->locks; l; l = l->next) {
	    if (!cool_strcasecmp(l->name->str, name.v.str->str)) {
		  /* found lock */
		e = MALLOC(Event, 1);
		*e = frame;
		e->blocked_on = BL_LOCK;
		e->lock = string_dup(name.v.str);
		gettimeofday(&e->timeout_at, 0);
		e->timeout_at.tv_sec += LOCK_TIMEOUT;
		e->msg = 0;
		event_add(e);
		ex_state = BLOCKED;
		break;
	    }
	}
	new = MALLOC(Lock, 1);
	new->name = string_dup(name.v.str);
	new->added_by = frame.msgid; 
	new->next = 0;
	if (!this->locks) {
	    this->locks = new;
	} else {
	    for (l = this->locks; l->next; l = l->next)
		;
	    l->next = new;
	}
    }
    var_free(name);
}

void
op_unlock(void)
{
    Var		name;
    Lock       *l, *prev = 0;

    name = pop();
    if (name.type != STR) {
	raise(E_ARGTYPE);
    } else {
	for (l = this->locks; l; l = l->next) {
	    if (!cool_strcasecmp(l->name->str, name.v.str->str)
	      && l->added_by == frame.msgid) {	/* found lock */
		if (prev) {					/* remove it */
		    prev->next = l->next;
		} else {
		    this->locks = l->next;
		}
		string_free(l->name);
		FREE(l);
		pushn(1);
		break;
	    }
	    prev = l;
	}
	if (!l) {
	    pushn(0);
	}
    }
    var_free(name);
}

void
op_at(void)
{
    int		i, endat = frame.m->code[frame.pc++];
    Var		at_time;

    at_time = pop();
    if (at_time.type != NUM) {
	raise(E_ARGTYPE);
	pop();			/* at is a statement, has no value */
    } else {
	Event *e = MALLOC(Event, 1);

	*e = frame;
	e->blocked_on = BL_TIMER;
	e->timeout_at.tv_sec = at_time.v.num;
	e->timeout_at.tv_usec = 0;
	e->on->ref++;
	e->m->ref++;
	e->args = list_dup(frame.args);
	e->msg = 0;
	for (i = 0; i < frame.nvars ;i++) {
	    e->stack[i] = var_dup(e->stack[i]);
	}
	e->sp = e->nvars;
	e->stack[e->sp].type = NUM;
	e->stack[e->sp].v.num = -1;		/* add a new terminator */
	e->sp++;
	event_add(e);
    }
    frame.pc = endat;
}

void
op_sleep(void)
{
    Var		delay;
    int i;

    delay.type = NUM;  delay.v.num = 0;
    if (frame.m->code[frame.pc++]) {
	delay = pop();
    }
    if (delay.type != NUM) {
	raise(E_ARGTYPE);
    } else {
	Event *e = MALLOC(Event, 1);

	pushn(0);			/* sleep() always returns 0 */
	*e = frame;
	e->on->ref++;
	e->m->ref++;
	e->blocked_on = BL_TIMER;
	gettimeofday(&e->timeout_at, 0);
	e->timeout_at.tv_sec += delay.v.num;
	e->timeout_at.tv_usec = 0;
	if (delay.v.num >= sleep_to_refresh) {
	    e->ticks = 0;
	    e->age = 0;
	}
	e->sp--;
	event_add(e);
	ex_state = BLOCKED;
    }
}

void
op_typeof(void)
{
    Var		arg;

    arg = pop();
    pushn(arg.type);
}

void
op_lengthof(void)
{
    Var		arg;

    arg = pop();
    switch (arg.type) {
      case STR:
	pushn(arg.v.str->len);
	break;
      case LIST:
	pushn(arg.v.list->len);
	break;
      case MAP:
	pushn(arg.v.map->num);
	break;
      default:
	raise(E_ARGTYPE);
	break;
    }
    var_free(arg);
}

void
op_serverof(void)
{
    Var		arg;

    arg = pop();
    if (arg.type != OBJ) {
	raise(E_ARGTYPE);
    } else {
	pushn(arg.v.obj.server);
    }
    var_free(arg);
}

void
op_servername(void)
{
    Var		arg, ret;

    arg = pop();
    if (arg.type != OBJ) {
	var_free(arg);
	raise(E_ARGTYPE);
    } else {
	ret.type = STR;
	ret.v.str = string_cpy(serv_id2name(arg.v.obj.server));
	push(ret);
    }
}

void
op_verbs(void)
{
    Verbdef	*v;
    int		 i, nverbs = 0;
    Var		 ret;
    List	 *info;

    for (v = this->verbs; v; v = v->next) {
	nverbs++;
    }
    ret.type = LIST;
    ret.v.list = list_new(nverbs);
    for (v = this->verbs, i = 0; v; v = v->next, i++) {
	info = list_new(3);
	info->el[0].type = info->el[1].type = info->el[2].type = STR;
	info->el[0].v.str = sym_get(this, v->verb);
	if(v->prep >= 0) {
	    info->el[1].v.str = sym_get(this, v->prep);
	} else {
	    info->el[1].v.str = sym_sys(BLANK);
	}
	info->el[2].v.str = sym_get(this, v->method);
	info->el[0].v.str->ref++;
	info->el[1].v.str->ref++;
	info->el[2].v.str->ref++;
	ret.v.list->el[i].type = LIST;
	ret.v.list->el[i].v.list = info;
    }
    push(ret);
}

void
op_vars(void)
{
    Vardef	*v;
    Var		 ret;
    int		 i, hval;

    if (this->vars) {
	ret.type = LIST;
	ret.v.list = list_new(this->vars->num);
	i = 0;
	for (hval = 0; hval < this->vars->size; hval++) {
	    for (v = this->vars->table[hval]; v; v = v->next) {
		ret.v.list->el[i].type = STR;
		ret.v.list->el[i].v.str = sym_get(this, v->name);
		ret.v.list->el[i].v.str->ref++;
		i++;
	    }
	}
    } else {
	ret.type = LIST;
	ret.v.list = list_dup(empty_list);
    }
    push(ret);
}

void
op_methods(void)
{
    Method	*m;
    Var		 ret;
    int		 i, hval;

    if (this->methods) {
	ret.type = LIST;
	ret.v.list = list_new(this->methods->num);
	i = 0;
	for (hval = 0; hval < this->methods->size; hval++) {
	    for (m = (Method *) this->methods->table[hval]; m;
		 m = m->next) {
		ret.v.list->el[i].type = STR;
		ret.v.list->el[i].v.str = sym_get(this, m->name);
		ret.v.list->el[i].v.str->ref++;
		i++;
	    }
	}
    } else {
	ret.type = LIST;
	ret.v.list = list_dup(empty_list);
    }
    push(ret);
}

void
op_parents(void)
{
    Var		 ret;

    ret.type = LIST;
    ret.v.list = list_dup(this->parents);
    push(ret);
}

void
op_pad(void)
{
    Var		 what, len, with, ret;
    char	 tok = ' ';
    int		 prepad = 0;

    if (nargs > 2) {
	with = pop();
	if (with.type != STR) {
	    var_free(with); raise(E_ARGTYPE); return;
	} else if (with.v.str->len < 1) {
	    var_free(with); raise(E_RANGE); return;
	} else {
	    tok = with.v.str->str[0];
	}
    }
    len = pop();  what = pop();
    if (len.type != NUM || what.type != STR) {
	raise(E_ARGTYPE);
    } else {
	ret.type = STR;
	if (what.v.str->ref == 1) {			/* if just one ref, */
	    ret.v.str = string_dup(what.v.str);		/* use this str */
	} else {
	    ret.v.str = string_cpy(what.v.str->str);	/* make a copy */
	}
	if (len.v.num < 0) {
	    len.v.num = -len.v.num;
	    prepad = 1;
	}
	if (len.v.num < what.v.str->len) {
	    ret.v.str->str[len.v.num] = '\0';		/* truncate */
	} else if (prepad) {				/* pad */
	    ret.v.str = string_prepad(ret.v.str, len.v.num - what.v.str->len, tok);
	} else {
	    ret.v.str = string_pad(ret.v.str, len.v.num - what.v.str->len, tok);
	}
	ret.v.str->len = len.v.num;
	push(ret);
    }
    var_free(len);  var_free(what);
}

void
op_tostr(void)
{
    Var		 what, ret;

    what = pop();
    ret.type = STR;
    ret.v.str = string_new(0);
    ret.v.str = var_tostring(ret.v.str, what, 1);
    push (ret);
    var_free(what);
}

void
op_tonum(void)
{
    Var		 arg, ret;

    ret.type = NUM;
    arg = pop();
    switch (arg.type) {
      case STR:
	ret.v.num = atoi(arg.v.str->str);
	break;
      case NUM:
	ret = arg;
	break;
      case OBJ:
	ret.v.num = arg.v.obj.id;
	break;
      case LIST:  case PC:  case MAP:
	raise(E_ARGTYPE);
	var_free(arg);
	return;
      case ERR:
	ret.v.num = (int) arg.v.err;
	break;
    }
    var_free(arg);
    push(ret);
}

void
op_toobj(void)
{
    Var		 arg, ret;

    ret.type = OBJ;
    arg = pop();
    switch (arg.type) {
      case STR:
	if (!parse_obj(arg.v.str->str, &ret.v.obj)) {
	    ret.v.obj.id = -1;
	    ret.v.obj.server = 0;
	}
	var_free(arg);
	break;
      case NUM:
	ret.v.obj.server = 0;
	ret.v.obj.id = arg.v.num;
	break;
      case OBJ:
	ret = arg;
	break;
      case LIST:  case PC:  case MAP:
	var_free(arg);
	raise(E_ARGTYPE);
	return;
      case ERR:
	ret.v.obj.server = 0;
	ret.v.obj.id = (int) arg.v.err;
	break;
    }
    push (ret);
}

void
op_toerr(void)
{
    Var		 arg, ret;

    ret.type = ERR;
    ret.v.err = E_NONE;
    arg = pop();
    switch (arg.type) {
      case STR:
	if (!parse_err(arg.v.str->str, &ret.v.err)) {
	    ret.v.err = -1;
	}
	break;
      case NUM:
	ret.v.err = arg.v.num;
	break;
      case OBJ:
	ret.v.err = arg.v.obj.id;
	break;
      case LIST:  case PC:  case MAP:
	raise(E_ARGTYPE);
	var_free(arg);
	return;
      case ERR:
	ret = arg;
	break;
    }
    var_free(arg);
    push (ret);
}

static void
do_match(int full)
{
    Var		template, str, marker;
    char	tok;

    if (nargs == 3) {
	marker = pop();
	if (marker.type != STR || !(tok = marker.v.str->str[0])) {
	    var_free(marker);
	    str = pop();  template = pop();
	    var_free(str); var_free(template);	/* free other 2 args */
	    raise(E_ARGTYPE);
	    return;
	}
	var_free(marker);
    } else {
	tok = ' ';			/* default separator is a space */
    }
    str = pop();  template = pop();	/* get 2 args */

    if (template.type != STR || str.type != STR) {
	raise(E_ARGTYPE);
    } else if (full) {
	pushn(match_full(template.v.str->str, str.v.str->str, tok));
    } else {
	pushn(match(template.v.str->str, str.v.str->str, tok));
    }
    var_free(template);  var_free(str);
}

void
op_match(void)
{
    do_match(0);
}

void
op_match_full(void)
{
    do_match(1);
}

void
op_strcmp(void)
{
    Var		s1, s2;

    s2 = pop();  s1 = pop();
    if (s1.type != STR || s2.type != STR) {
	raise(E_ARGTYPE);
    } else {
	pushn(strcmp(s1.v.str->str, s2.v.str->str));
    }
    var_free(s1);  var_free(s2);
}

void
op_tolower(void)
{
    Var		arg, ret;
    int		i;

    arg = pop();
    if (arg.type != STR) {
	raise(E_ARGTYPE);
    } else {
	if (arg.v.str->ref == 1) {
	    ret = arg;
	    arg.v.str->ref++;
	} else {
	    ret.type = STR;
	    ret.v.str = string_cpy(arg.v.str->str);
	}
	for (i = 0; i < arg.v.str->len; i++) {
	    ret.v.str->str[i] = tolower(arg.v.str->str[i]);
	}
	push(ret);
    }
    var_free(arg);
}

void
op_toupper(void)
{
    Var		arg, ret;
    int		i;

    arg = pop();
    if (arg.type != STR) {
	raise(E_ARGTYPE);
    } else {
	if (arg.v.str->ref == 1) {
	    ret = arg;
	    arg.v.str->ref++;
	} else {
	    ret.type = STR;
	    ret.v.str = string_cpy(arg.v.str->str);
	}
	for (i = 0; i < arg.v.str->len; i++) {
	    ret.v.str->str[i] = toupper(arg.v.str->str[i]);
	}
	push(ret);
    }
    var_free(arg);
}

void
op_f_index(void)
{
    Var		s1, s2, case_matters;

    case_matters.type = NUM;  case_matters.v.num = 0;
    if (nargs > 2) {
	case_matters = pop();
    }
    s2 = pop();  s1 = pop();
    if (s1.type != STR || s2.type != STR || case_matters.type != NUM) {
	raise(E_ARGTYPE);
    } else {
	pushn(strindex(s1.v.str->str, s2.v.str->str, case_matters.v.num));
    }
    var_free(s1);  var_free(s2);  var_free(case_matters);
}


void
op_rindex(void)
{
    Var		s1, s2, case_matters;

    case_matters.type = NUM;  case_matters.v.num = 0;
    if (nargs > 2) {
	case_matters = pop();
    }
    s2 = pop();  s1 = pop();
    if (s1.type != STR || s2.type != STR || case_matters.type != NUM) {
	raise(E_ARGTYPE);
    } else {
	pushn(strrindex(s1.v.str->str, s2.v.str->str, case_matters.v.num));
    }
    var_free(s1);  var_free(s2);  var_free(case_matters);
}
