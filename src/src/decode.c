/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "x.tab.h"
#include "execute.h"

extern void	 writelog(void);
static String	*decode_arg(String *s, Object *o, Method *m,
			   enum arg_type  type, int arg);
static int	 expr_prec(int opcode);
static void	 tpush(Inst *i);
static Inst 	*tpop(void);

/*
 * decode_method()
 *
 * Takes a list of tokens and turns it into readable code.
 *
 * level:  0 - integers	(least readable)
 *	   1 - tokens and integers (./compile -n produces this)
 *	   2 - symbol table lookups are performed (most readable)
 *
 * Returned as a 1-line string
 */

String *
decode_method(Object *o, Method *m, int level)
{
    int		 i, arg;
    String	*s = string_new(0);
    Op_entry	*op;

    for (i = 0; i < m->ninst; ) {
	if (level == 0) {		/* level zero, numeric tokens only */
	    s = string_catnum(s, m->code[i++]);
	    s = string_cat(s, " ");
	} else {
	    op = opcodes[m->code[i]];
	    if (!op) {
		writelog();
		fprintf(stderr,
		    "decode_method():  Unknown opcode %d in method %s on #%d\n",
		    m->code[i++], sym_get(o, m->name)->str, o->id.id);
		return s;
	    } else {
		s = string_cat(s, op->name);
		s = string_cat(s, " ");
		i++;
		for (arg = 0; arg < op->nargs; arg++) {
		    if (level == 2) {
			s = decode_arg(s, o, m, op->arg_type[arg],
					m->code[i++]);
			s = string_cat(s, " ");
		    } else {	/* level 1 */
			s = string_catnum(s, m->code[i++]);
			s = string_cat(s, " ");
		    }
		}
	    }
	}
    }
    return s;
}

static String *
decode_arg(String *s, Object *o, Method *m, enum arg_type  type, int arg)
{
    int		n;

    switch (type) {
      case NUM_ARG:
	s = string_catnum(s, arg);
	break;
      case ID_ARG:
	s = string_cat(s, sym_get(o, arg)->str);
	break;
      case STR_ARG:
	s = string_cat(s, "\"");
	s = string_cat(s, sym_get(o, arg)->str);
	s = string_cat(s, "\"");
	break;
      case VARNO_ARG:
	n = var_find_local(m, arg)->name;
	s = string_cat(s, sym_get(o, n)->str);
	break;
      case none:
	break;		/* must be a mistake in token table */
    }
    return s;
}

#define MAX_TSTACK	100
static Inst    *tstack[MAX_TSTACK];	/* stack for unparsing exprs */
static int	tsp;

#define MAX_TREE	200
static Inst	tree[MAX_TREE];		/* parse tree memory */
static int	tptr;

static int	indent;			/* current indentation value */
static int	indent_by;		/* how much to indent by (shiftwid) */
static int	full_bracketing;	/* full bracketing flag */
static int	lineno;			/* current line # */
static int	find_pc;		/* what PC to search line # for */
static int	found_lineno;		/* found line # (corresponding to pc) */
static int	number_lines;		/* flag:  number lines? */

static String	*decompile_local_vars(String *s, Object *o, Method *m);
static String	*decompile_ehandler(String *s, Object *o, Method *m);
static String	*decompile_stmts(String *s, Object *o, Method *m, int i);
static String	*decompile_expr(String *s, Object *o, Method *m, Inst *t,
			       int prec);
static String	*decompile_args(String *s, Object *o, Method *m, Inst in);
static String	*decompile_map(String *s, Object *o, Method *m, Inst in);
static void	 tree_list(Method *m, int i);
static void	 tree_map(Method *m, int i);
static int	 tree_count_args(void);
static void	 tree_args(int len);
static void	 tree_message(Method *m, int i);
static void	 tree_message_expr(Method *m, int i);
static void	 tree_pass(Method *m, int i);
static void	 tree_ternop(Method *m, int i);
static void	 tree_binop(Method *m, int i);
static void	 tree_unop(Method *m, int i);
static void	 tree_func(Method *m, int i);
static String	*line_start(String *str, int ind, const char *s);

static String	*decompile_parents(String *s, Object *o);
static String	*decompile_global_vars(String *s, Object *o);
static String	*decompile_verbs(String *s, Object *o);
static String	*decompile_methods(String *s, Object *o);

struct prec_entry {
    int		op;
    int		prec;
} prec_table[] = {
    { ASGNLVAR,	1 },
    { ASGNGVAR,	1 },
    { UPTO,	2 },
    { ENDOR,	3 },
    { ENDAND,	4 },
    { IN,	5 },
    { EQ,	6 },
    { NE,	6 },
    { LT,	7 },
    { LE,	7 },
    { GT,	7 },
    { GE,	7 },
    { ADD,	8 },
    { SUB,	8 },
    { MUL,	9 },
    { DIV,	9 },
    { MOD,	9 },
    { NOT,	10 },
    { NEGATE,   10 },
    { SPLICE,   10 },
};

static int
expr_prec(int opcode)
{
    int		i;

    for (i = 0; i < Arraysize(prec_table); i++) {
	if (prec_table[i].op == opcode) {
	    return prec_table[i].prec;
	}
    }
    return 0;	/* default to low precedence */
}

String *
decompile_method(String *s, Object *o, Method *m, int opt_lineno,
		 int opt_brackets, int opt_indent, int indent_from,
		 int opt_find_pc, int *pfound_lineno)
{
    tptr = 0;  tsp = 0;
    number_lines = opt_lineno;
    indent_by = opt_indent;
    full_bracketing = opt_brackets;
    lineno = found_lineno = 1;  find_pc = opt_find_pc;
    indent = indent_from;
    s = decompile_local_vars(s, o, m);
    s = decompile_ehandler(s, o, m);
    s = decompile_stmts(s, o, m, 0);
    if (s) {
	s->len -= 2;
	s->str[s->len] = '\0';		/* nuke last CRLF */
    }
    if (pfound_lineno) {
	*pfound_lineno = found_lineno;
    }
    return s;
}

static String *
decompile_local_vars(String *s, Object *o, Method *m)
{
    Vardef	*v;

    if (!m->vars) {
	return s;
    }
    s = string_indent_cat(s, indent + (number_lines ? 6 : 0), "var ");
    for (v = m->vars; v; v = v->next) {
	s = string_cat(s, sym_get(o, v->name)->str);
	if (v->next) {
	    s = string_cat(s, ", ");
	}
    }
    s = string_cat(s, ";\r\n");
    return s;
}

static String *
decompile_ehandler(String *s, Object *o, Method *m)
{
    int		i, started = 0;

    for (i = 0; i < NERRS; i++) {
	if (m->ehandler[i] == EH_IGNORE) {
	    if (!started) {
		s = string_indent_cat(s, indent + (number_lines ? 6 : 0),
			"ignore ");
		started = 1;
	    } else {
		s = string_cat(s, ", ");
	    }
	    s = string_cat(s, err_id2name(i));
	}
    }
    if (started) {
	s = string_cat(s, ";\r\n");
    }
    return s;
}

static void
tpush(Inst *i)
{
    if (tsp >= MAX_TSTACK) {
	tsp++;			/* stack full; fake it */
    } else {
	tstack[tsp++] = i;
    }
}

static Inst *
tpop(void)
{
    if (!tsp) {
	return 0;
    } else if(tsp >= MAX_TSTACK) {
	--tsp;			/* fake it again */
	return 0;
    } else {
	return tstack[--tsp];
    }
}

#define UNPARSE_BINOP(STR) \
	s = decompile_expr(s, o, m, (Inst *) t[2], tprec); \
	s = string_cat(s, (STR)); \
	s = decompile_expr(s, o, m, (Inst *) t[1], tprec + 1);

static String *
decompile_stmts(String *s, Object *o, Method *m, int i)
{
    int		 n, stopped = 0;
    Inst	*i1;

    while (!stopped) {
	if (find_pc >= 0 && i >= find_pc) {
	    found_lineno = lineno;
	    find_pc = -1;
	}
	switch (m->code[i]) {
	  case IF:
	    s = line_start(s, indent, "if (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ")\r\n");
	    lineno++;
	    indent += indent_by;
	    s = decompile_stmts(s, o, m, i + 3);
	    if (m->code[m->code[i + 1]] != STOP) {
		s = decompile_stmts(s, o, m, m->code[i + 1]);
	    }
	    indent -= indent_by;
	    s = line_start(s, indent, "endif\r\n");
	    lineno++;
	    i = m->code[i + 2];
	    break;
	  case ELSEIF:
	    s = line_start(s, indent - indent_by, "elseif (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ")\r\n");
	    lineno++;
	    s = decompile_stmts(s, o, m, i + 2);
	    i = m->code[i + 1];
	    break;
	  case ELSE:
	    s = line_start(s, indent - indent_by, "else\r\n");
	    lineno++;
	    i++;
	    break;
	  case FOR:
	    (void) tpop();	/* skip NUMPUSH */
	    s = line_start(s, indent, "for ");
	    n = var_find_local(m, m->code[i+1])->name;
	    s = string_cat(s, sym_get(o, n)->str);
	    s = string_cat(s, " in (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ")\r\n");
	    lineno++;
	    indent += indent_by;
	    s = decompile_stmts(s, o, m, i + 3);
	    indent -= indent_by;
	    s = line_start(s, indent, "endfor\r\n");
	    lineno++;
	    i = m->code[i + 2];
	    break;
	  case FORRNG:
	    s = line_start(s, indent, "for ");
	    n = var_find_local(m, m->code[i+1])->name;
	    s = string_cat(s, sym_get(o, n)->str);
	    s = string_cat(s, " in [");
	    i1 = tpop();
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, "..");
	    s = decompile_expr(s, o, m, i1, 0);
	    tptr = 0;
	    s = string_cat(s, "]\r\n");
	    lineno++;
	    indent += indent_by;
	    s = decompile_stmts(s, o, m, i + 3);
	    indent -= indent_by;
	    s = line_start(s, indent, "endfor\r\n");
	    lineno++;
	    i = m->code[i + 2];
	    break;
	  case WHILE:
	    s = line_start(s, indent, "while (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ")\r\n");
	    lineno++;
	    indent += indent_by;
	    s = decompile_stmts(s, o, m, i + 2);
	    indent -= indent_by;
	    s = line_start(s, indent, "endwhile\r\n");
	    lineno++;
	    i = m->code[i + 1];
	    break;
	  case DO:
	    s = line_start(s, indent, "do\r\n");
	    lineno++;
	    indent += indent_by;
	    i += 2;
	    break;
	  case DOWHILE:
	    indent -= indent_by;
	    s = line_start(s, indent, "while (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ");\r\n");
	    tptr = 0;
	    lineno++;
	    i += 3;
	    break;
	  case AT:
	    s = line_start(s, indent, "at (");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ")\r\n");
	    lineno++;
	    indent += indent_by;
	    s = decompile_stmts(s, o, m, i + 2);
	    indent -= indent_by;
	    s = line_start(s, indent, "endat\r\n");
	    lineno++;
	    i = m->code[i + 1];
	    break;
	  case POP:			/* expr; */
	    s = line_start(s, indent, "");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    tptr = 0;
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    i++;
	    break;
	  case ASGNGVAR:
	    s = line_start(s, indent, sym_get(o, m->code[i + 1])->str);
	    s = string_cat(s, " = ");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i += 2;
	    break;
	  case ASGNLVAR:
	    n = var_find_local(m, m->code[i + 1])->name;
	    s = line_start(s, indent, sym_get(o, n)->str);
	    s = string_cat(s, " = ");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i += 2;
	    break;
	  case ASGNGVARINDEX:
	    s = line_start(s, indent, sym_get(o, m->code[i + 1])->str);
	    s = string_cat(s, "[");
	    i1 = tpop();
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, "] = ");
	    s = decompile_expr(s, o, m, i1, 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i += 2;
	    break;
	  case ASGNLVARINDEX:
	    n = var_find_local(m, m->code[i + 1])->name;
	    s = line_start(s, indent, sym_get(o, n)->str);
	    i1 = tpop();
	    s = string_cat(s, "[");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, "] = ");
	    s = decompile_expr(s, o, m, i1, 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i += 2;
	    break;
	  case SETPLAYER:
	    s = line_start(s, indent, "player = ");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i++;
	    break;
	  case T_RETURN:
	    s = line_start(s, indent, "return ");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i++;
	    break;
	  case T_RAISE:
	    s = line_start(s, indent, "raise ");
	    s = decompile_expr(s, o, m, tpop(), 0);
	    s = string_cat(s, ";\r\n");
	    lineno++;
	    tptr = 0;
	    i++;
	    break;
	  case CONTINUE:
	    if (m->code[i + 1] > 1) {
		s = line_start(s, indent, "continue ");
		s = string_catnum(s, m->code[i + 1]);
		s = string_cat(s, ";\r\n");
	    } else {
		s = line_start(s, indent, "continue;\r\n");
	    }
	    lineno++;
	    i += 2;
	    break;
	  case BREAK:
	    if (m->code[i + 1] > 1) {
		s = line_start(s, indent, "break ");
		s = string_catnum(s, m->code[i + 1]);
		s = string_cat(s, ";\r\n");
	    } else {
		s = line_start(s, indent, "break;\r\n");
	    }
	    lineno++;
	    i += 2;
	    break;
/* 
 * everything below here is an expr, and pushed onto the stack
 */

/*
 * leaf nodes - these op's don't have any descendants, so we
 * can push the address of the method's actual code
 */
	  case PARENTS:  case ARGS:  case THIS:  case PLAYER:  case CALLER:
	    tpush(m->code + i);
	    i++;
	    break;
	  case NUMPUSH:  case STRPUSH:  case ERRPUSH:
	  case GETLVAR:  case GETGVAR:  case GETSYSVAR:
	    tpush(m->code + i);
	    i += 2;
	    break;
	  case OBJPUSH:
	    tpush(m->code + i);
	    i += 3;
	    break;
	  case LISTPUSH:
	    tree_list(m, i);
	    i += 1;
	    break;
	  case MAPPUSH:
	    tree_map(m, i);
	    i += 2;
	    break;
	  case MESSAGE:
	    tree_message(m, i);
	    i += 2;
	    break;
	  case MESSAGE_EXPR:
	    tree_message_expr(m, i);
	    i += 1;
	    break;
	  case PASS:
	    tree_pass(m, i);
	    i += 2;
	    break;
/*
 * binary operators
 */
	  case ADD:  case SUB:  case MUL:  case DIV:  case MOD:
	  case EQ:  case NE:  case GT:  case GE:  case LT:  case LE:
	  case INDEX:  case IN:  case LSUBSET: case RSUBSET:
	    tree_binop(m, i);
	    i++;
	    break;
	    
/*
 * ternary operators
 */
	  case SUBSET:
	    tree_ternop(m, i);
	    i++;
	    break;
/*
 * unary operators
 */
	  case NEGATE: case NOT:  case SPLICE:
	    tree_unop(m, i);
	    i++;
	    break;
/*
 * ignore these; effectively pushes LHS
 */
	  case AND: case OR:
	    i += 2;
	    break;
/* 
 * actually do something here
 */
	  case ENDAND:  case ENDOR:
	    tree_binop(m, i);
	    i++;
	    break;
	  case PUSHPC:
	    i++;
	    break;
	  case ARGSTART:
	    tpush((Inst *) ARGSTART);
	    i++;
	    break;
	  case STOP:
	    stopped = 1;
	    break;
/*
 * functions
 */
	  default:
	    tree_func(m, i);
	    i += 1;
	    break;
	}
    }
    return s;
}

static String *
decompile_expr(String *s, Object *o, Method *m, Inst *t, int prec)
{
    Objid	obj;
    int		tprec;		/* precedence of current statement */
    const char *funcname;	/* function name, for unparsing func() */
    int		n;

    if (!t) {
	s = string_cat(s, " /* Expression too complex */ ");
	return s;
    }
    tprec = expr_prec (t[0]);
    if (tprec && (tprec < prec || full_bracketing)) {
	s = string_cat(s, "(");
    }
    switch(t[0]) {
      case GETLVAR:
	n = var_find_local(m, t[1])->name;
	s = string_cat(s, sym_get(o, n)->str);
	break;
      case GETGVAR:
	s = string_cat(s, sym_get(o, t[1])->str);
	break;
      case GETSYSVAR:
        s = string_catc(s, '$');
        s = string_cat(s, sym_get(o, t[1])->str);
        break;
      case MESSAGE:
	s = decompile_expr(s, o, m, (Inst *) t[2], 0);
	s = string_cat(s, ".");
	s = string_cat(s, sym_get(o, t[1])->str);
	if (t[3]) {
	    s = string_cat(s, "(");
	    s = decompile_args(s, o, m, (Inst) (t + 3));
	    s = string_cat(s, ")");
	}
	break;
      case MESSAGE_EXPR:
	s = decompile_expr(s, o, m, (Inst *) t[1], 0);
	s = string_cat(s, ".(");
	s = decompile_expr(s, o, m, (Inst *) t[2], 0);
	s = string_cat(s, ")");
	if (t[3]) {
	    s = string_cat(s, "(");
	    s = decompile_args(s, o, m, (Inst) (t + 3));
	    s = string_cat(s, ")");
	}
	break;
      case PASS:
	s = string_cat(s, "pass(");
	s = decompile_args(s, o, m, (Inst) (t + 2));
	s = string_cat(s, ")");
	if (t[1] != o->parents->el[0].v.obj.id) {
	    s = string_cat(s, " to #");
	    s = string_catnum(s, t[1]);
	}
	break;
      case INDEX:
	s = decompile_expr(s, o, m, (Inst *) t[2], tprec);
	s = string_cat(s, "[");
	s = decompile_expr(s, o, m, (Inst *) t[1], tprec);
	s = string_cat(s, "]");
	break;
      case SUBSET:
	s = decompile_expr(s, o, m, (Inst *) t[3], tprec);
	s = string_cat(s, "[");
	s = decompile_expr(s, o, m, (Inst *) t[2], 0);
	s = string_cat(s, "..");
	s = decompile_expr(s, o, m, (Inst *) t[1], 0);
	s = string_cat(s, "]");
	break;
      case LSUBSET:
	s = decompile_expr(s, o, m, (Inst *) t[2], tprec);
	s = string_cat(s, "[..");
	s = decompile_expr(s, o, m, (Inst *) t[1], 0);
	s = string_cat(s, "]");
	break;
      case RSUBSET:
	s = decompile_expr(s, o, m, (Inst *) t[2], tprec);
	s = string_cat(s, "[");
	s = decompile_expr(s, o, m, (Inst *) t[1], 0);
	s = string_cat(s, "..]");
	break;
      case NUMPUSH:
	s = string_catnum(s, t[1]);
	break;
      case STRPUSH:
	s = string_cat(s, "\"");
	s = string_backslash(s, sym_get(o, t[1])->str);
	s = string_cat(s, "\"");
	break;
      case OBJPUSH:
	obj.id = t[1];  obj.server = t[2];
	s = string_catobj(s, obj, obj.server);
	break;
      case LISTPUSH:
	s = string_cat(s, "{");
	s = decompile_args(s, o, m, (Inst) (t + 1));
	s = string_cat(s, "}");
	break;
      case MAPPUSH:
	s = string_cat(s, "[");
	s = decompile_map(s, o, m, (Inst) (t + 1));
	s = string_cat(s, "]");
	break;
      case ERRPUSH:
	s = string_cat(s, err_id2name(t[1]));
	break;
      case ADD:
	UNPARSE_BINOP(" + ");
	break;
      case SUB:
	UNPARSE_BINOP(" - ");
	break;
      case MUL:
	UNPARSE_BINOP(" * ");
	break;
      case DIV:
	UNPARSE_BINOP(" / ");
	break;
      case MOD:
	UNPARSE_BINOP(" % ");
	break;
      case EQ:
	UNPARSE_BINOP(" == ");
	break;
      case NE:
	UNPARSE_BINOP(" != ");
	break;
      case GT:
	UNPARSE_BINOP(" > ");
	break;
      case GE:
	UNPARSE_BINOP(" >= ");
	break;
      case LT:
	UNPARSE_BINOP(" < ");
	break;
      case LE:
	UNPARSE_BINOP(" <= ");
	break;
      case AND:
      case OR:
	  /* ignore 'em */
	break;
      case ENDAND:
	UNPARSE_BINOP(" && ");
	break;
      case ENDOR:
	UNPARSE_BINOP(" || ");
	break;
      case IN:
	UNPARSE_BINOP(" in ");
	break;
      case NOT:
	s = string_cat(s, "!");
	s = decompile_expr(s, o, m, (Inst *) t[1], tprec);
	break;
      case NEGATE:
	s = string_cat(s, "-");
	s = decompile_expr(s, o, m, (Inst *) t[1], tprec);
	break;
      case SPLICE:
        s = string_cat(s, "@");
        s = decompile_expr(s, o, m, (Inst *) t[1], tprec);
        break;
      case PARENTS:
	s = string_cat(s, "parents");
	break;
      case THIS:
	s = string_cat(s, "this");
	break;
      case PLAYER:
	s = string_cat(s, "player");
	break;
      case CALLER:
	s = string_cat(s, "caller");
	break;
      case ARGS:
	s = string_cat(s, "args");
	break;
      default:
	if (!(funcname = bf_id2name(t[0]))) {
	    writelog();
	    fprintf(stderr, "decompile_expr():  Unknown opcode %d\n", t[0]);
	} else {
	    s = string_cat(s, funcname);
	    s = string_cat(s, "(");
	    s = decompile_args(s, o, m, (Inst) (t + 1));
	    s = string_cat(s, ")");
	}
	break;
    }
    if (tprec && (tprec < prec || full_bracketing)) {
	s = string_cat(s, ")");
    }
    return s;
}

static String *
decompile_args(String *s, Object *o, Method *m, Inst in)
{
    Inst	*t = (Inst *) in;
    int		 i;

    for (i = 0; i < t[0]; i++) {
	s = decompile_expr(s, o, m, (Inst *) t[i + 1], 0);
	if (i < t[0] - 1) {
	    s = string_cat(s, ", ");
	}
    }
    return s;
}

static String *
decompile_map(String *s, Object *o, Method *m, Inst in)
{
    Inst	*t = (Inst *) in;
    int		 i;

    for (i = 0; i < t[0]; i++) {
	s = decompile_expr(s, o, m, (Inst *) t[i * 2 + 2], 0);
	s = string_cat(s, " => ");
	s = decompile_expr(s, o, m, (Inst *) t[i * 2 + 1], 0);
	if (i < t[0] - 1) {
	    s = string_cat(s, ", ");
	}
    }
    return s;
}

static void
tree_func(Method *m, int i)
{
    Inst	*start = tree + tptr;
    int		 tnargs = tree_count_args();

    if (tptr + tnargs >= MAX_TREE) {		/* not enough tree mem left */
	tpush(0);
    } else {
	tree[tptr++] = m->code[i];		/* func */
	tree_args(tnargs);
	tpush(start);
    }
}

static int
tree_count_args(void)
{
    Inst	*s = 0, *t;
    int		count;

    for (count = 0; count < tsp; count++) {
	t = tstack[tsp - count - 1];
	tstack[tsp - count - 1] = s;
	if (((Inst) t) == ARGSTART) {
	    break;
	}
	s = t;
    }
    tsp--;
    return count;
}

static void
tree_args(int len)
{
    int		 i;

    tree[tptr++] = len;
    for (i = 1; i <= len; i++) {
	tree[tptr + len - i] = (Inst) tpop();
    }
    tptr += len;
}

static void
tree_list(Method *m, int i)
{
    Inst	*start = tree + tptr;
    int		 len = tree_count_args();

    if (tptr + len + 2 >= MAX_TREE) {
	tpush(0);			/* not enough tree mem left */
    } else {
	tree[tptr++] = LISTPUSH;
	tree_args(len);
	tpush(start);
    }
}

static void
tree_map(Method *m, int i)
{
    Inst	*start = tree + tptr;
    int		j, len = m->code[i + 1];

    if (tptr + len * 2 + 2 >= MAX_TREE) {
	tpush(0);			/* not enough tree mem left */
    } else {
	tree[tptr++] = MAPPUSH;
	tree[tptr++] = len;
	for (j = 1; j <= len; j++) {
	    tree[tptr++] = (Inst) tpop();
	    tree[tptr++] = (Inst) tpop();
	}
	tpush(start);
    }
}

static void
tree_pass(Method *m, int i)
{
    Inst	*start = tree + tptr;
    int		 tnargs = tree_count_args();

    if (tptr + 3 + tnargs >= MAX_TREE) {
	tpush(0);			/* not enough tree mem left */
    } else {
	tree[tptr++] = PASS;
	tree[tptr++] = m->code[i + 1];	/* parent */
	tree_args(tnargs);
	tpush(start);
    }
}

static void
tree_message(Method *m, int i)
{
    Inst	*start = tree + tptr;
    Inst	*toref;
    int		 tnargs = tree_count_args();

    if (tptr + 4 + tnargs >= MAX_TREE) {
	tpush(0);			/* not enough tree mem left */
    } else {
	tree[tptr++] = MESSAGE;
	tree[tptr++] = m->code[i + 1];	/* message */
	toref = &(tree[tptr++]);	/* save address of dest expr */
	tree_args(tnargs);
	*toref = (Inst) tpop();		/* store destination expr */
	tpush(start);
    }
}

static void
tree_message_expr(Method *m, int i)
{
    Inst	*start = tree + tptr;
    Inst	*toref, *msgref;
    int		 tnargs = tree_count_args();

    if (tptr + 4 + tnargs >= MAX_TREE) {
	tpush(0);			/* not enough tree mem left */
    } else {
	tree[tptr++] = MESSAGE_EXPR;
	toref = &(tree[tptr++]);	/* save address of dest expr */
	msgref = &(tree[tptr++]);	/* save address of msg expr */
	tree_args(tnargs);
	*msgref = (Inst) tpop();	/* store message expr */
	*toref = (Inst) tpop();		/* store destination expr */
	tpush(start);
    }
}

static void
tree_ternop(Method *m, int i)
{
    Inst	*start = tree + tptr;

    if (tptr + 4 >= MAX_TREE) {
	tpush(0);
    } else {
	tree[tptr++] = m->code[i];
	tree[tptr++] = (Inst) tpop();	/* left argument */
	tree[tptr++] = (Inst) tpop();	/* middle argument */
	tree[tptr++] = (Inst) tpop();	/* right argument */
	tpush(start);
    }
}

static void
tree_binop(Method *m, int i)
{
    Inst	*start = tree + tptr;

    if (tptr + 3 >= MAX_TREE) {		/* not enough tree mem left */
	tpush(0);
    } else {
	tree[tptr++] = m->code[i];	/* store opcode */
	tree[tptr++] = (Inst) tpop();	/* store right argument */
	tree[tptr++] = (Inst) tpop();	/* store left argument */
	tpush(start);			/* push address of node */
    }
}

static void
tree_unop(Method *m, int i)
{
    Inst	*start = tree + tptr;

    if (tptr + 2 >= MAX_TREE) {		/* not enough tree mem left */
	tpush(0);
    } else {
	tree[tptr++] = m->code[i];		/* store opcode */
	tree[tptr++] = (Inst) tpop();	/* store arg */
	tpush(start);			/* push address of node */
    }
}

static String *
line_start(String *str, int ind, const char *s)
{
    char	buf[INT_SIZE + 3];

    if (number_lines) {
	sprintf(buf, "%3d:  ", lineno);
	str = string_cat(str, buf);
    }
    str = string_indent_cat(str, ind, s);
    return str;
}

String *
decompile_object(Object *o)
{
    String	*s;

    if (!o) return 0;
    s = string_new(0);
    s = string_cat(s, "object ");
    s = string_catobj(s, o->id, 0);
    s = string_cat(s, "\r\n");
    s = decompile_parents(s, o);
    s = decompile_global_vars(s, o);
    s = decompile_verbs(s, o);
    s = decompile_methods(s, o);
    s = string_cat(s, "endobject\r\n\r\n");
    return s;
} /* decompile_object() */

static String *
decompile_parents(String *s, Object *o)
{
    int		i;

    if (o->parents->len > 0) {
	s = string_cat(s, "  parents ");
	for (i = 0; i < o->parents->len - 1; i++) {
	    s = string_catobj(s, o->parents->el[i].v.obj, 0);
	    s = string_cat(s, ", ");
	}
	s = string_catobj(s, o->parents->el[i].v.obj, 0);
	s = string_cat(s, ";\r\n\r\n");
    } /* if */
    return s;
} /* decompile_parents() */
	
static String *
decompile_global_vars(String *s, Object *o)
{
    Vardef	*var;
    int		 hval;

    if (!o->vars) return s;
    for (hval = 0; hval < o->vars->size; hval++) {
	for (var = o->vars->table[hval]; var; var = var->next) {
	    switch (var->value.type) {
	      case STR:
		s = string_cat(s, "  str  ");  break;
	      case NUM:
		s = string_cat(s, "  num  ");  break;
	      case OBJ:
		s = string_cat(s, "  obj  ");  break;
	      case LIST:
		s = string_cat(s, "  list ");  break;
	      case MAP:
		s = string_cat(s, "  map  ");  break;
	      case ERR:
		s = string_cat(s, "  err  ");  break;
	      case PC:	/* should never happen */
		break;
	    } /* switch */
	    s = string_cat(s, sym_get(o, var->name)->str);
	    s = string_cat(s, " = ");
	    s = var_tostring(s, var->value, 1);
	    s = string_cat(s, ";\r\n");
	} /* for */
    } /* for */
    s = string_cat(s, "\r\n");
    return s;
} /* decompile_global_vars */

static String *
decompile_verbs(String *s, Object *o)
{
    Verbdef	*v;

    for (v = o->verbs; v; v = v->next) {
	s = string_cat(s, "  verb \"");
	s = string_cat(s, sym_get(o, v->verb)->str);
	s = string_cat(s, "\"");
	if (v->prep >= 0) {
	    s = string_cat(s, " : \"");
	    s = string_cat(s, sym_get(o, v->prep)->str);
	    s = string_cat(s, "\"");
	} /* if */
	s = string_cat(s, " = ");
	s = string_cat(s, sym_get(o, v->method)->str);
	s = string_cat(s, ";\r\n");
    } /* for */
    s = string_cat(s, "\r\n");
    return s;
} /* decompile_verbs() */

static String *
decompile_methods(String *s, Object *o)
{
    Method	*m;
    int		 hval;

    if (!o->methods) return s;
    for (hval = 0; hval < o->methods->size; hval++) {
	for (m = o->methods->table[hval]; m; m = m->next) {
	    if (m->blocked) {
		s = string_cat(s, "  blocked method ");
	    } else {
		s = string_cat(s, "  method ");
	    } /* if */
	    s = string_cat(s, sym_get(o, m->name)->str);
	    s = string_cat(s, "\r\n");
	    s = decompile_method(s, o, m, 0, 0, 2, 4, 0, 0);
	    s = string_cat(s, "\r\n");
	    s = string_cat(s, "  endmethod\r\n\r\n");
	} /* for */
    }
    return s;
} /* decompile_methods */
