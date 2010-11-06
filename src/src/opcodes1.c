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
#include "x.tab.h"

void
op_add(void)
{
    Var		right, left, ret;

    right = pop(); left = pop();
    if (left.type != right.type && left.type != LIST) {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    } else if (left.type == NUM) {
	left.v.num = left.v.num + right.v.num;
	push(left);
    } else if (left.type == STR) {
	ret.type = STR;
	if (left.v.str->ref == 1) {
	    ret.v.str = string_cat(left.v.str, right.v.str->str);
	} else {
	    ret.v.str = string_new(left.v.str->len + right.v.str->len + 1);
	    strcpy(ret.v.str->str, left.v.str->str);
	    strcpy(ret.v.str->str + left.v.str->len, right.v.str->str);
	    ret.v.str->len = left.v.str->len + right.v.str->len;
	    var_free(left);
	}
	var_free(right);
	push(ret);
    } else if (left.type == LIST) {
	ret.type = LIST;
	ret.v.list = list_setadd(left.v.list, right);
	push(ret);
    } else {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    }
}

void
op_sub(void)
{
    Var		right, left;

    right = pop(); left = pop();
    if (left.type != right.type && left.type != LIST) {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    } else if (left.type == NUM) {
	left.v.num -= right.v.num;
	push(left);
    } else if (left.type == LIST) {
	left.v.list = list_setremove(left.v.list, right);
	var_free(right);
	push(left);
    } else {
	raise(E_ARGTYPE);
    }
}

void
op_mul(void)
{
    Var		right, left;

    right = pop(); left = pop();
    if (left.type != NUM || right.type != NUM) {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    } else {
	left.v.num *= right.v.num;
	push(left);
    }
}

void
op_div(void)
{
    Var		right, left;

    right = pop(); left = pop();

    if (left.type != NUM || right.type != NUM) {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    } else if (!right.v.num) {
	raise(E_DIV);
    } else {
	left.v.num /= right.v.num;
	push(left);
    }
}

void
op_mod(void)
{
    Var		right, left;

    right = pop(); left = pop();

    if (left.type != NUM || right.type != NUM) {
	var_free(left);  var_free(right);
	raise(E_TYPE);
    } else if (!right.v.num) {
	raise(E_DIV);
    } else {
	left.v.num %= right.v.num;
	push(left);
    }
}

void
op_negate(void)
{
    Var		arg;

    arg = pop();
    if (arg.type != NUM) {
	var_free(arg);
	raise(E_TYPE);
    } else {
	arg.v.num = -arg.v.num;
	push(arg);
    }
}

void
op_and(void)
{
    Var		left;
    Var		ret;

    left = pop();
    if (ISTRUE(left)) {	/* LHS true, evaluate RHS */
	frame.pc++;	/* skip over breakout goto */
    } else {		/* LHS false, abort */
	ret.type = NUM;
	ret.v.num = 0;
	push(ret);
	frame.pc = frame.m->code[frame.pc];	 /* skip to breakout */
    }
    var_free(left);
}

void
op_or(void)
{
    Var		left;
    Var		ret;

    left = pop();
    if (!ISTRUE(left)) {	/* LHS false, evaluate RHS */
	frame.pc++;		/* skip over breakout goto */
    } else {			/* LHS true, skip to end */
	ret.type = NUM;
	ret.v.num = 1;
	push(ret);
	frame.pc = frame.m->code[frame.pc];	/* skip to breakout */
    }
    var_free(left);
}

void
op_not(void)
{
    Var		ret, arg;

    arg = pop();
    ret.type = NUM;
    ret.v.num = !ISTRUE(arg);
    var_free(arg);
    push(ret);
}

#define REL(NAME, OP) \
void NAME(void) \
{ \
    Var		ret, right, left; \
\
    right = pop(); left = pop(); \
    if (left.type != right.type) { \
	raise(E_TYPE); \
    } else if (left.type != STR && left.type != NUM) { \
	raise(E_TYPE); \
    } else { \
	ret.type = NUM; ret.v.num = var_compare(left, right) OP 0; \
	push(ret); \
    } \
    var_free(left);  var_free(right); \
}

REL(op_lt, <)
REL(op_gt, >)
REL(op_le, <=)
REL(op_ge, >=)

void
op_eq(void)
{
    Var		ret, right, left;

    right = pop(); left = pop();

    ret.type = NUM; ret.v.num = var_eq(left, right);
    push (ret);
    var_free(left);  var_free(right);
}

void
op_ne(void)
{
    Var		ret, right, left;

    right = pop(); left = pop();
    ret.type = NUM; ret.v.num = !var_eq(left, right);
    push (ret);
    var_free(left);  var_free(right);
}

void
op_index(void)
{
    Var		ret, i, base;

    i = pop(); base = pop();
    switch (base.type) {
      case STR:
	if (i.type != NUM) {
	    raise(E_TYPE);
	} else if (i.v.num < 1 || i.v.num > base.v.str->len) {
	    raise(E_RANGE);
	} else {
	    ret.type = STR;
	    ret.v.str = string_new(2);
	    ret.v.str->str[0] = base.v.str->str[i.v.num - 1];
	    ret.v.str->str[1] = '\0';
	    ret.v.str->len = 1;
	    push(ret);
	}
	break;
      case LIST:
	if (i.type != NUM) {
	    raise(E_TYPE);
	} else if (i.v.num < 1 || i.v.num > base.v.list->len) {
	    raise(E_RANGE);
	} else {
	    ret = var_dup(base.v.list->el[i.v.num - 1]);
	    push(ret);
	}
	break;
      case MAP:
	if (map_find(base.v.map, i, &ret)) {
	    raise(E_MAPNF);
	} else {
	    push(var_dup(ret));
	}
	break;
      default:
	raise(E_TYPE);
    }
    var_free(base); var_free(i);
}

void
op_subset(void)
{
    Var		base, left, right, ret;
    int		len = 0;

    right = pop(); left = pop();
    base = pop();

    ret.type = base.type;

    if (base.type == STR) {
	len = base.v.str->len;
    } else if (base.type == LIST) {
	len = base.v.list->len;
    }
    if (left.type != NUM || right.type != NUM) {
	var_free(left);  var_free(right);
	var_free(base); raise(E_TYPE); return;
    }
    if (right.v.num < 1) {
	right.v.num = len;
    }
    if (left.v.num > right.v.num
     || left.v.num < 1 || left.v.num > len
     || right.v.num < 1 || right.v.num > len) {
	raise(E_RANGE);
    } else if (base.type == STR) {
	len = right.v.num - left.v.num + 1;	/* substring length */
	ret.type = STR;
	ret.v.str = string_new(len);
	strncpy(ret.v.str->str, base.v.str->str + left.v.num - 1, len);
	ret.v.str->str[len] = '\0';
	ret.v.str->len = len;
	push(ret);
    } else if (base.type == LIST) {
	ret.v.list = list_subset(base.v.list, left.v.num, right.v.num);
	push(ret);
    } else {
	raise(E_TYPE);
    }
    var_free(base);
    var_free(left); var_free(right);
}

void
op_lsubset(void)
{
    Var		left, right;

    right = pop();
    left.type = NUM;
    left.v.num = 1;
    push(left);
    push(right);
    op_subset();
}

void
op_rsubset(void)
{
    Var		left, right;

    left = pop();
    right.type = NUM;
    right.v.num = -1;
    push(left);
    push(right);
    op_subset();
}

void
op_splice(void)
{
    int		i;
    Var		list = pop();

    if (list.type != LIST) {
	raise(E_TYPE);
    } else {
	for (i = 0; i < list.v.list->len; i++) {
	    push(var_dup(list.v.list->el[i]));
	}
    }
    var_free(list);
}

void
op_return(void)
{
    ex_retval = pop();
    ex_state = STOPPED;
}

void
op_pop(void)
{
    Var		v;
     
    v = pop();
    var_free(v);
}

void
op_numpush(void)
{
    Var		n;
    
    n.type = NUM;
    n.v.num = frame.m->code[frame.pc++];
    push(n);
}

void
op_strpush(void)
{
    Var		s;
    int		symno;

    symno = frame.m->code[frame.pc++];
    s.type = STR;
    s.v.str = string_dup(sym_get(frame.on, symno));
    push(s);
}

void
op_objpush(void)
{
    Var		o;

    o.type = OBJ;
    o.v.obj.id = frame.m->code[frame.pc++];
    o.v.obj.server = frame.m->code[frame.pc++];
    push(o);
}

void
op_listpush(void)
{
    Var		list;

    list = pop_args(count_args());
    push(list);
}

void
op_mappush(void)
{
    int		 num = frame.m->code[frame.pc++], i;
    Var		 v;
    Var		 from, to;

    v.type = MAP;
    v.v.map = map_new(num);
    for (i = 0; i < num; i++) {
	to = pop();  from = pop();
	v.v.map = map_add(v.v.map, from, to);
    }
    push(v);
}

void
op_errpush(void)
{
    Var		e;

    e.type = ERR;
    e.v.err = frame.m->code[frame.pc++];
    push(e);
}

void
op_message(void)
{
    Var		args;
    Var		dest;
    String	*msg;

    args = pop_args(count_args());
    dest = pop();
    if (dest.type != OBJ) {
	var_free(args); var_free(dest);
	frame.pc++;
	raise(E_INVIND);
    } else {
	msg = string_dup(sym_get(frame.on, frame.m->code[frame.pc]));
	frame.pc++;
        send_message_and_block(frame.this, dest.v.obj, msg,
			       args.v.list, dest.v.obj);
    }
}

void
op_message_expr(void)
{
    Var		args;
    Var		msg, dest;

    args = pop_args(count_args());
    msg = pop(); dest = pop();

    if (dest.type != OBJ) {
	var_free(args); var_free(dest); var_free(msg);
	raise(E_INVIND);
    } else if (msg.type != STR) {
	var_free(args); var_free(msg);
	raise(E_TYPE);
    } else {
	send_message_and_block(frame.this, dest.v.obj, msg.v.str,
			       args.v.list, dest.v.obj);
    }
}

void
op_if(void)
{
    Var		cond;

    cond = pop();
    if(ISTRUE(cond)) {
	pushn(frame.m->code[frame.pc + 1]);	/* push end address */
	frame.pc += 2;				/* skip to code */
    } else if (frame.m->code[frame.pc]) {	/* if there's an elseif */
	pushn(frame.m->code[frame.pc + 1]);	/* push end address */
	frame.pc = frame.m->code[frame.pc];	/* go to elseif code */
    } else {
	frame.pc = frame.m->code[frame.pc + 1];	/* go to end address */
    }
    var_free(cond);
}

void
op_null(void)		/* null function */
{ }

void
op_elseif(void)
{
    Var		cond;

    cond = pop();
    if (ISTRUE(cond)) {
	frame.pc++;
    } else {
	frame.pc = frame.m->code[frame.pc];
    }
    var_free(cond);
}

void
op_for(void)
{
    Var		idx, list;
    
    idx = pop();
    list = pop();
    
    if( list.type == MAP)
    {
	list.v.list = map_keys( list.v.map );
	list.type = LIST;
    }
    if( list.type == STR)
    {
	list.v.list = string_list( list.v.str );
	list.type = LIST;
    }
    if (list.type != LIST) {
	var_free(list);
	raise(E_FOR);
    } else if (idx.v.num >= list.v.list->len) {	/* loop is complete */
	var_free(list);
	frame.pc = frame.m->code[frame.pc + 1];	/* skip to end */
    } else {
	var_assign_local(frame.stack, frame.m->code[frame.pc],
		var_dup(list.v.list->el[idx.v.num]));
	idx.v.num++;
	push(list);		/* push list */
	push(idx);		/* push new index */
	pushpc(frame.pc - 1);	/* push address of FOR statement */
	frame.pc += 2;		/* go to first instruction in loop */
    }
}

void
op_forrng(void)
{
    Var		v, upper;

    upper = pop();
    v = pop();
    if (v.type != NUM || upper.type != NUM) {
	var_free(upper);  var_free(v);
	raise(E_TYPE);
    } else if (v.v.num > upper.v.num) {
	frame.pc = frame.m->code[frame.pc + 1];
    } else {
	var_assign_local(frame.stack, frame.m->code[frame.pc], v);
	v.v.num++;
	push(v);
	push(upper);
	pushpc(frame.pc - 1);
	frame.pc += 2;
    }
}

void
op_pushpc(void)
{
    pushpc(frame.pc - 1);
}

void
op_argstart(void)
{
    pushpc(-ARGSTART);
}

void
op_while(void)
{
    Var		cond;

    cond = pop();
    if (!ISTRUE(cond)) {
	(void) pop();			/* take pc off stack */
	frame.pc = frame.m->code[frame.pc];
    } else {
	frame.pc++;		/* go to first instruction in loop */
    }
    var_free(cond);
}

void
op_do(void)
{
    pushpc(frame.m->code[frame.pc++]);
}

void
op_dowhile(void)
{
    Var		cond;

    cond = pop();
    if (ISTRUE(cond)) {				/* keep looping */
	frame.pc = frame.m->code[frame.pc];
    } else {					/* loop is finished */
	(void) pop();				/* remove sentinel */
	frame.pc += 2;				/* skip to end */
    }
    var_free(cond);
}

void
op_break(void)
{
    Var		newpc;
    int		break_lvl = frame.m->code[frame.pc++];

    while (break_lvl--) {
	do {
	    newpc = pop();
	    var_free(newpc);
	} while (newpc.type != PC || newpc.v.num < 0);
    }
    if (frame.m->code[newpc.v.num] == FOR) {
	(void) pop();			/* pop index */
	newpc = pop();  var_free(newpc);/* pop list */
	frame.pc = frame.m->code[newpc.v.num + 2];
    } else if (frame.m->code[newpc.v.num] == FORRNG) {
	(void) pop();			/* pop upper range */
	(void) pop();			/* pop current value */
	frame.pc = frame.m->code[newpc.v.num + 2];
    } else if (frame.m->code[newpc.v.num] == DOWHILE) {
	frame.pc = newpc.v.num + 3;
    } else if (frame.m->code[newpc.v.num] == PUSHPC) {	/* WHILE, actually */
	while (frame.m->code[newpc.v.num] != WHILE) {
	    newpc.v.num++;
	}
	frame.pc = frame.m->code[newpc.v.num + 1];
    }
}

void
op_continue(void)
{
    Var		newpc;
    int		break_lvl = frame.m->code[frame.pc++];

    while (break_lvl--) {
	do {
	    newpc = pop();
	    var_free(newpc);
	} while (newpc.type != PC || newpc.v.num < 0);
    }
    if (frame.m->code[newpc.v.num] == DOWHILE) {
	pushpc(newpc.v.num);
	frame.pc = frame.m->code[newpc.v.num + 2];
    } else {
	frame.pc = newpc.v.num;		/* all the others push their own PC */
    }
}

void
op_asgnlvar(void)
{
    Var		v;

    v = pop();
    var_assign_local(frame.stack, frame.m->code[frame.pc++], v);
}

void
op_asgngvar(void)
{
    Var		v;
    int		varno = frame.m->code[frame.pc++];

    v = pop();
    raise(var_assign_global(this, sym_get(frame.on, varno), v));
    (void) pop();
}

static int	do_asgnindex(Var var, Var idx, Var expr, Var *ret);

void
op_asgnlvarindex(void)
{
    Var		var, idx, expr, ret;
    int		varno = frame.m->code[frame.pc++];
    
    expr = pop();  idx = pop();

    var = frame.stack[varno];
    if (!do_asgnindex(var, idx, expr, &ret)) {
	frame.stack[varno] = ret;
    }
}

void
op_asgngvarindex(void)
{
    Var		var, idx, expr, ret;
    int		varno = frame.m->code[frame.pc++];
    Error	r;

    expr = pop();  idx = pop();

    r = var_get_global(this, sym_get(frame.on, varno)->str, &var);
    if (r != E_NONE) {
	raise(r);
    } else if (!do_asgnindex(var, idx, expr, &ret)) {
	var = var_dup(var);
	var_assign_global(this, sym_get(frame.on, varno), ret);
    }
}

static int
do_asgnindex(Var var, Var idx, Var expr, Var *ret)
{
    ret->type = var.type;
    if (var.type == LIST) {
	if (idx.type != NUM) {
	    var_free(idx);  var_free(expr);
	    raise(E_TYPE);  return -1;
	} else if (idx.v.num <= 0 || idx.v.num > var.v.list->len) {
	    var_free(expr);
	    raise(E_RANGE);  return -2;
	} else {
	    ret->v.list = list_assign(var.v.list, expr, idx.v.num);
	    return 0;
	}
    } else if (var.type == MAP) {
	ret->v.map = map_add(var.v.map, idx, expr);
	return 0;
    } else {
	var_free(expr);  var_free(idx);
	raise(E_TYPE);  return -3;
    }
}    

void
op_getlvar(void)
{
    Var		v;

    var_get_local(frame.stack, frame.m->code[frame.pc++], &v);
    push (var_dup(v));
}

void
op_getgvar(void)
{
    Var		ret;
    Error	r;
    int		varname;

    varname = frame.m->code[frame.pc++];
    r = var_get_global(this, sym_get(frame.on, varname)->str, &ret);
    if (r != E_NONE) {
	raise(r);
	(void) pop();		/* what's this for?  i forget */
    } else {
	push (var_dup(ret));
    }
}

void
op_getgvarexpr(void)
{
    Var		arg, ret;
    Error	r;

    arg = pop();
    if (arg.type != STR) {
	raise(E_ARGTYPE);
    } else {
	r = var_get_global(this, arg.v.str->str, &ret);
	if (r != E_NONE) {
	    raise(r);
	} else {
	    push (var_dup(ret));
	}
    }
    var_free(arg);
}

void
op_getsysvar(void)
{
    int		varno = frame.m->code[frame.pc++];
    String	*varname = sym_get(frame.on, varno);
    Var		ret;
    Object      *sys;

    if (!(sys = retrieve(sys_obj))) {
        raise(E_OBJNF);
    } else if (var_get_global(sys, varname->str, &ret) == E_VARNF) {
        raise(E_VARNF);
    } else {
        push(var_dup(ret));
    }
}

void
op_asgngvarexpr(void)
{
    Var		arg1, arg2;
    Error	r;

    arg2 = pop();  arg1 = pop();
    if (arg1.type != STR) {
	raise(E_ARGTYPE);
    } else if (!valid_ident(arg1.v.str->str)) {
	raise(E_RANGE);
    } else {
	r = var_assign_global(this, arg1.v.str, arg2);
	if (r != E_NONE) {
	    raise(r);
	} else {
	    push (var_dup(arg2));
	}
    }
    var_free(arg1);
}
	
void
op_stop(void)
{
    Var		newpc;

    newpc = pop();
    if (newpc.type != PC && newpc.type != NUM) {
	writelog();
	fprintf(stderr, "EXECUTION ERROR:  STOP encountered non-num PC\n");
	raise(E_INTERNAL);
	var_free(newpc);
    } else {
	if (newpc.v.num < 0 && newpc.v.num != -ARGSTART) {
	    ex_state = STOPPED;
	} else {
	    frame.pc = newpc.v.num;
	}
    }
}

void
op_in(void)
{
    Var		left, right, r;

    right = pop();
    left = pop();
    r.type = NUM;

    if (right.type == LIST) {
	r.v.num = list_ismember(left, right.v.list);
	push(r);
    } else if (right.type == STR) {
	if (left.type != STR) {
	    raise(E_TYPE);
	} else {
	    r.v.num = str_in(left.v.str->str, right.v.str->str);
	    push(r);
	}
    } else {
	raise(E_TYPE);
    }
    var_free(left);
    var_free(right);
}
