/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "servers.h"
#include "netio.h"
#include "execute.h"

void
op_shutdown(void)
{
    if (!is_wizard(frame.this)) {
	raise(E_PERM);
    } else {
	server_running = 0;
	pushn(0);
    }
}

void
op_sync(void)
{
    if (!is_wizard(frame.this)) {
	raise(E_PERM);
    } else {
	cache_sync();
	pushn(0);
    }
}

void
op_writelog(void)
{
    Var		arg;

    arg = pop();
    if (frame.this.id != SYS_OBJ && !is_wizard(frame.this)) {
	raise(E_PERM);
    } else if (arg.type != STR) {
	raise(E_ARGTYPE);
    } else {
	writelog();
	fprintf(stderr, "%s\n", arg.v.str->str);
	pushn(0);
    }
    var_free(arg);
}

void
op_disconnect(void)
{
    Var		arg;

    switch (nargs) {
      case 0:
	disconnect(frame.this.id);
	pushn(0);
	break;
      case 1:
        arg = pop();
        if (arg.type != NUM) {
	    var_free(arg);
	    raise(E_TYPE);
	} else if (!is_wizard(frame.this)) {
	    raise(E_PERM);
	} else {
	    disconnect_fd(arg.v.num);
	    pushn(0);
	}
        break;
    }
}

void
op_raise(void)
{
    Var		 e;
    List	*raise_args;

    e = pop();
    if (e.type != ERR) {
	var_free(e);
	raise(E_ARGTYPE);
	(void) pop();
    } else {
	ex_state = RAISED;
	raise_args = make_raise_args(e.v.err);
	raise_args->el[1].v.str = add_traceback_header(raise_args->el[1].v.str,
				    e.v.err);
	send_raise(raise_args);
    }
}

void
op_pass(void)
{
    Objid	 parent;
    Var		 args;

    parent.server = 0;
    parent.id = frame.m->code[frame.pc++];
    args = pop_args(count_args());
    send_message_and_block(frame.this, frame.this,
	string_dup(sym_get(frame.on, frame.m->name)), args.v.list, parent);
}

void
op_hasparent(void)
{
    Var		parent;

    parent = pop();

    if (parent.type != OBJ) {
	var_free(parent);
	raise(E_ARGTYPE);
    } else {
	pushn(hasparent(this, parent.v.obj));
    }
}

void
op_objsize(void)
{
    pushn(size_object(this));
}

void
op_spew_method(void)
{
    Var		 name, ret;
    Method	*m;

    name = pop();
    if (name.type != STR) {
	var_free(name);
	raise(E_ARGTYPE);
    } else {
	if ((m = find_method(this, name.v.str->str))) {
	    ret.type = STR;
	    ret.v.str = decode_method(this, m, 2);
	    push(ret);
	} else {
	    raise(E_METHODNF);
	}
    }
    var_free(name);
}

void
op_list_method(void)
{
    Var		name, lineno, brackets, indent, ret;
    Method	*m;

    lineno.type = brackets.type = indent.type = NUM;
    lineno.v.num = 1;  brackets.v.num = 0;  indent.v.num = 2;

    switch (nargs) {
      case 4:
	indent = pop();
      case 3:
	brackets = pop();
      case 2:
	lineno = pop();
      case 1:
	name = pop();
    }
    
    if (name.type != STR || indent.type != NUM || brackets.type != NUM
     || lineno.type != NUM) {
	var_free(name);  var_free(indent);  var_free(brackets);
	var_free(lineno);
	raise(E_ARGTYPE);
    } else {
	if ((m = find_method(this, name.v.str->str))) {
	    ret.type = STR;
	    ret.v.str = string_new(0);
	    ret.v.str = decompile_method(ret.v.str, this, m, lineno.v.num,
					 brackets.v.num, indent.v.num, 0, 0, 0);
	    push(ret);
	} else {
	    raise(E_METHODNF);
	}
    }
    var_free(name);
}

void
op_decompile(void)
{
    Var		ret;

    ret.type = STR;
    ret.v.str = decompile_object(this);
    push(ret);
}

void
op_find_method(void)
{
    Var		arg, ret;
    Object	*where;

    arg = pop();
    if (arg.type != STR) {
	raise(E_ARGTYPE);
    } else {
	(void) find_method_recursive(this, arg.v.str->str, &where);
	ret.type = OBJ;
	if (where) {
	    ret.v.obj = where->id;
	} else {
	    ret.v.obj.server = 0;
	    ret.v.obj.id = NOTHING;
	}
	push (ret);
    } /* if */
    var_free(arg);
}
void
op_this(void)
{
    Var		v;

    v.type = OBJ; v.v.obj = frame.this;
    push(v);
}

void
op_player(void)
{
    Var		v;

    v.type = OBJ; v.v.obj = frame.player;
    push(v);
}

void
op_caller(void)
{
    Var		v;

    v.type = OBJ; v.v.obj = frame.caller;
    push(v);
}

void
op_args(void)
{
    Var		v;

    v.type = LIST; v.v.list = list_dup(frame.args);
    push(v);
}

void
op_setplayer(void)
{
    Var		newplayer;

    newplayer = pop();
    if (newplayer.type != OBJ) {
	raise(E_ARGTYPE);
    } else {
	frame.player = newplayer.v.obj;
    }
    var_free(newplayer);
}

void
op_program(void)
{
    while (nargs--) {
	var_free(pop());
    }
    pushn(0);
}

static Playerid		 progr;
static const char	*mem_code;
static int		 code_pos, mem_eof;
static int		 mem_getc(void);
static void		 mem_ungetc(int c);
static void		 mem_perror(const char *s);

static int
mem_getc(void)
{
    if (mem_code[code_pos]) {
	return mem_code[code_pos++];
    } else {
	mem_eof = 1;
	return EOF;
    }
}

static void
mem_ungetc(int c)
{
    if (code_pos > 0 && !mem_eof) {
	code_pos--;
    }
}

static void
mem_perror(const char *s)
{
    tell(progr, s, 1 );
}

void
op_compile(void)
{
    Var		obj, method, pcode;
    Object	*o;

    pcode = pop();
    if (pcode.type != STR) {
	var_free(pcode);
	while (--nargs) {
	    method = pop();
	    var_free(method);
	}
	raise(E_ARGTYPE);
	return;
    }
    mem_code = pcode.v.str->str;
    code_pos = 0;
    mem_eof = 0;
    progr = frame.this.id;
    switch(nargs) {
      case 1:	/* program some objects */
	pushn(compile(progr, mem_getc, mem_ungetc, mem_perror, 0, 0, 0, 0, 0));
	break;
      case 2:	/* invalid */
	method = pop();  var_free(method);
	raise(E_RANGE);
	break;
      case 3:	/* program a single method */
	method = pop();
	obj = pop();
	if (obj.type != OBJ || method.type != STR) {
	    raise(E_ARGTYPE);
	} else if (!(o = retrieve(obj.v.obj))) {
	    raise(E_OBJNF);
	} else if (!can_program(frame.player, obj.v.obj)) {
	    raise(E_PERM);
	} else if (!valid_ident(method.v.str->str)) {
	    raise(E_METHODNF);	/* not quite the right error, but hey */
	} else {
	    pushn(compile(progr, mem_getc, mem_ungetc, mem_perror, 1, o,
			   method.v.str, 0, 0));
	    
	}
	var_free(obj);  var_free(method);
	this = retrieve(frame.this);	/* in case we recompiled "this" */
	break;
    }
    var_free(pcode);
}

void
op_verb(void)
{
    Var		 verb, method, prep;
    int		 verbno, methodno, prepno;

    method = pop();
    prep = pop();
    verb = pop();
    if (verb.type != STR || prep.type != STR || method.type != STR) {
	var_free(verb);  var_free(prep);  var_free(method);
	raise(E_ARGTYPE);
    } else {
	verbno = sym_add(this, verb.v.str);
	if (prep.v.str->str[0]) {
	    prepno = sym_add(this, prep.v.str);
	} else {
	    string_free(prep.v.str);
	    prepno = -1;
	}
	methodno = sym_add(this, method.v.str);
	verb_add(this, verbno, prepno, methodno);
	cache_put(this, frame.this.id);
	pushn(0);
    }
}

void
op_rmverb(void)
{
    Var		 verb;

    verb = pop();
    if (verb.type != STR) {
	raise(E_ARGTYPE);
    } else {
	if (verb_rm(this, verb.v.str->str)) {
	    pushn(0);
	} else {
	    raise(E_VERBNF);
	}
	cache_put(this, frame.this.id);
    }
    var_free(verb);
} 

void
op_rmmethod(void)
{
    Var		method;

    method = pop();
    if (method.type != STR) {
	raise(E_ARGTYPE);
    } else {
	raise(rm_method(this, method.v.str->str));
	cache_put(this, frame.this.id);
    }
    var_free(method);
}

void
op_rmvar(void)
{
    Var		varname;

    varname = pop();
    if (varname.type != STR) {
	raise(E_ARGTYPE);
    } else {
	raise(var_rm_global(this, varname.v.str->str));
	cache_put(this, frame.this.id);
    }
    var_free(varname);
}

void
op_listinsert(void)
{
    Var		list, value, pos;

    pos.type = NUM;
    pos.v.num = -1;
    if (nargs > 2) {
	pos = pop();
    }
    value = pop();
    list = pop();
    if (list.type != LIST || pos.type != NUM) {
	var_free(pos);  var_free(list);  var_free(value);
	raise(E_ARGTYPE);
    } else if (pos.v.num > list.v.list->len) {
	raise(E_RANGE);
    } else {
	list.v.list = list_insert(list.v.list, value, pos.v.num);
	push(list);
    }
}

void
op_listappend(void)
{
    Var		list, value, pos;

    pos.type = NUM;
    pos.v.num = -1;
    if (nargs > 2) {
	pos = pop();
    }
    value = pop();
    list = pop();
    if (list.type != LIST || pos.type != NUM) {
	var_free(pos);  var_free(list);  var_free(value);
	raise(E_ARGTYPE);
    } else if (pos.v.num > list.v.list->len) {
	var_free(pos);  var_free(list);  var_free(value);
	raise(E_RANGE);
    } else {
	list.v.list = list_append(list.v.list, value, pos.v.num);
	push(list);
    }
}

void
op_listdelete(void)
{
    Var		pos;	/* position to delete */
    Var		list;	/* list to delete it from */

    pos = pop();
    list = pop();
    if (list.type != LIST || pos.type != NUM) {
	var_free(pos);  var_free(list);
	raise(E_ARGTYPE);
    } else if (pos.v.num < 1 || pos.v.num > list.v.list->len) {
	var_free(list);
	raise(E_RANGE);
    } else {
	list.v.list = list_delete(list.v.list, pos.v.num);
	push(list);
    }
}

void
op_listassign(void)
{
    Var		pos;		/* position to assign */
    Var		value;		/* value to assign it */
    Var		list;		/* list to modify */

    pos = pop();
    value = pop();
    list = pop();
    if (list.type != LIST || pos.type != NUM) {
	var_free(pos);  var_free(list);  var_free(value);
	raise(E_ARGTYPE);
    } else if (pos.v.num < 1 || pos.v.num > list.v.list->len) {
	var_free(list);	 var_free(value);
	raise(E_RANGE);
    } else {
	list.v.list = list_assign(list.v.list, value, pos.v.num);
	push(list);
    }
}

void
op_sort(void)
{
    int		reverseflag = 0;
    Var		list, reverse, ret;

    if (nargs > 1) {
	reverse = pop();
	reverseflag = ISTRUE(reverse);
	var_free(reverse);
    }
    list = pop();
    if (list.type != LIST) {
	var_free(list);
	raise(E_ARGTYPE);
    } else {
	ret.type = LIST;
	ret.v.list = list_sort(list.v.list, reverseflag);
	push(ret);
    }
}

void
op_strsub(void)
{
    Var		source, what, with, caseflag, ret;

    caseflag.type = NUM;  caseflag.v.num = 0;
    if (nargs > 3) {
	caseflag = pop();
    }
    with = pop();  what = pop();  source = pop();
    if (source.type != STR || what.type != STR || with.type != STR) {
	raise(E_ARGTYPE);
    } else {
	ret.type = STR;
	ret.v.str = strsub(source.v.str->str, what.v.str->str, with.v.str->str,
			   ISTRUE(caseflag));
	push(ret);
    }
    var_free(source);  var_free(what);  var_free(with);  var_free(caseflag);
}

void
op_psub(void)
{
    Var		source, ret;

    source = pop();
    if (source.type != STR) {
	raise(E_ARGTYPE);
    } else {
	ret.type = STR;
	ret.v.str = psub(source.v.str->str);
	push(ret);
    }
    var_free(source);
}

void
op_servers(void)
{
    Var			servlist;

    servlist.type = LIST;
    servlist.v.list = list_dup(server_list);

    push(servlist);
}

void
op_ps(void)
{
    Var		v;

    v.type = LIST;  v.v.list = ps();
    push(v);
} /* op_ps() */

void
op_kill(void)
{
    Var		pid;
    int		r;

    pid = pop();
    if (pid.type != NUM) {
	var_free(pid);
	raise(E_ARGTYPE);
    } else {
	r = cmkill(pid.v.num, frame.caller);
        switch (r) {
	  case 0:
	  case -1:
	    pushn(r);
	    break;
	  case -2:
	    raise(E_PERM);
	    break;
	}
    } /* if */
} /* op_kill() */

void
op_set_parse(void)
{
    Var		id, oid;

    oid = pop();  id = pop();
    if (oid.type != OBJ || id.type != NUM) {
	var_free(oid);  var_free(id);
	raise(E_ARGTYPE);
    } else if (!is_wizard(frame.this)) {
	raise(E_PERM);
    } else {
	set_parse(id.v.num, oid.v.obj.id);
	pushn(0);
    }
}
