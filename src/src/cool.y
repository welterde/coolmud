/* Copyright (c) 1993 Stephen F. White */

%{
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "code.h"
#include "execute.h"
#include "servers.h"

static void	yyerror(const char *s);
static void	error(const char  *s, const char *t);
int		yyparse(void);
static int	yylex(void);
#ifdef PROTO
extern MALL_T	malloc(unsigned size);
extern void	free(MALL_T ptr);
#endif
static void	do_check_parents(Object *o);
static void	add_parent(Object *o, Objid newparent);
static void	new_global_var(int name, Type_spec type, Var value);
static int	builtin_var(int varno);
static void	start_method(int mname, int blocked);
static void	end_method(void);
static int	code_assign(int varname, int local_opcode,
			    int global_opcode);
static int	find_loopvar(int varname);

static Object	*cur_obj;		/* current object */
static Method	*cur_method;		/* current method */
static int	cur_type;		/* current type specifier */
static int	parse_expecting;	/* OBJECT or METHOD */
static int	lineno;			/* current line number */
static int	loop_depth;		/* current depth of nested loops */
static int	loopvar;		/* variable used for iteration */

%}
%union {
    int		 num;
    Objid	 obj;
    Var		 var;
    List	*list;
    Map		*map;
}

%right '='
%left	UPTO MAPSTO
%left	OR
%left	AND
%left	IN
%left   EQ NE
%left   LT LE GT GE
%left	'+' '-'
%left	'*' '/' '%'
%left	NOT NEGATE '@'
%right	'(' ')' '[' ']' '{' '}' '.'

%token <num> NUMBER_CONST STRING_CONST ERROR_CONST TYPE_SPEC ID
%token IF ELSE ELSEIF ENDIF FOR IN ENDFOR AT ENDAT T_BLOCKED
%token T_RETURN CONTINUE BREAK IGNORE TO
%token PARENTS OBJECT ENDOBJECT METHOD ENDMETHOD VERB VAR
%token RMVERB RMMETHOD RMVAR

%token ARGS PLAYER CALLER THIS
%token SETPLAYER

%token STOP
%token MESSAGE MESSAGE_EXPR ADD SUB MUL DIV MOD
%token INDEX SUBSET LSUBSET RSUBSET
%token FIND_METHOD SPEW_METHOD LIST_METHOD DECOMPILE
%token GETLVAR GETGVAR GETGVAREXPR ASGNLVAR ASGNGVAR ASGNGVAREXPR POP
%token ASGNLVARINDEX ASGNGVARINDEX GETSYSVAR
%token OBJPUSH NUMPUSH STRPUSH LISTPUSH MAPPUSH ERRPUSH
%token CLONE DESTROY CHPARENTS TIME CTIME EXPLODE STRSUB PSUB PAD RANDOM
%token SETADD SETREMOVE LISTINSERT LISTAPPEND LISTDELETE LISTASSIGN
%token CRYPT CHECKMEM CACHE_STATS SET_PARSE SORT CONNECT STRCMP
%token F_INDEX RINDEX TOLOWER TOUPPER
%token LOCK UNLOCK TONUM TOOBJ TOSTR TOERR
%token TYPEOF LENGTHOF SERVEROF SERVERNAME SERVERS
%token VERBS VARS METHODS PARENTS CHILDREN HASPARENT OBJSIZE
%token MATCH MATCH_FULL SHUTDOWN DUMP WRITELOG ECHO DISCONNECT PROGRAM COMPILE
%token T_RAISE PASS ENDAND ENDOR ECHO_FILE SLEEP FORRNG DO DOWHILE
%token WHILE ENDWHILE PUSHPC KILL PS SPLICE ARGSTART ECHON SYNC

%token EXPECTING_OBJECT EXPECTING_METHOD

%token LAST_TOKEN	/* this HAS to be the last token in this file */

%type <num> expr pass_expr
%type <num> builtin_objvar
%type <num> ifstart elseifs elseifstart endif
%type <num> forstart endfor atstart endat whilecond endwhile
%type <num> dostart forrng end
%type <num> count maplist var
%type <obj> object_const opt_pass_to
%type <var> const_expr
%type <list> list_const list_of_constants
%type <map> map_const list_of_constant_pairs
%%
target:
	  EXPECTING_OBJECT OBJECT object_const { cur_obj->id = $3; }
	  object_body ENDOBJECT
	| EXPECTING_METHOD method_body
	;
object_body:
	  parents			{ do_check_parents(cur_obj); }
	  obj_declarations methods
	;
parents:
	  /* NOTHING */			{ cur_obj->parents = list_new(0); }
	| PARENTS parentlist ';'
	;
parentlist:
	  object_const			{ add_parent(cur_obj, $1); }
	| parentlist ',' object_const	{ add_parent(cur_obj, $3); }
	;
method_declarations:
	  /* NOTHING */	
	| method_declarations method_decl
	;
obj_declarations:
	  /* NOTHING */	
	| obj_declarations obj_decl
	;
obj_decl:
	  verb_decl
	| global_var_decl
	;
method_decl:
	  local_var_decl
	| ignore_decl
	;
local_var_decl:
	  VAR { cur_type = VAR; } local_list ';' 
	;
global_var_decl:
	  TYPE_SPEC { cur_type = $1; } global_list ';'
	| ID '=' const_expr ';'
	    { new_global_var($1, $3.type, $3); }
	;
verb_decl:
	  VERB STRING_CONST '=' ID ';' 
	    { verb_add(cur_obj, $2, -1, $4); }
	| VERB STRING_CONST ':' STRING_CONST '=' ID ';' 
	    { verb_add(cur_obj, $2, $4, $6); }
	;
ignore_decl:
	  IGNORE errorlist ';'
	;
errorlist:
	  error_const
	| errorlist ',' error_const
error_const:
	  ERROR_CONST
	    {
		if (NOIGNORE($1)) {
		    error("Cannot catch or ignore:", err_id2name($1));
		} else if (cur_method) {
		    cur_method->ehandler[$1] = EH_IGNORE;
		}
	    }
	;
global_list:  global_item
	| global_list ',' global_item
	;
global_item:
	  ID
	    { new_global_var($1, cur_type, var_init(cur_type)); }
	| ID '=' const_expr
	    { new_global_var($1, cur_type, $3); }
	;
local_list:	local_item
	| local_list ',' local_item
	;
local_item:
	  ID
	    {   int dummy;
		if (cur_method && var_add_local(cur_method, $1, &dummy)) {
		    error("Variable multiply defined:  ",
			sym_get(cur_obj, $1)->str);
		    sym_free(cur_obj, $1);
		}
	    }
	;
methods:
	  /* NOTHING */
	| methods method
	;
method:
	  METHOD ID
	    { start_method($2, 0); }
	  method_body ENDMETHOD
	    { end_method(); }
	| T_BLOCKED METHOD ID
	    { start_method($3, 1); }
	  method_body ENDMETHOD
	    { end_method(); }
	;
method_body:
	  method_declarations statements
	;
statements:
	  /* NOTHING */
	| statements statement
	;
statement:
	  ifstart statements end elseifs elsepart endif
	    { machine[$1 + 1] = $3; machine[$1 + 2] = $6; }
	| forstart statements endfor	{ machine[$1 + 4] = $3; }
	| forrng statements endfor	{ machine[$1 + 2] = $3; }
	| WHILE { code(PUSHPC); } whilecond statements endwhile	
					{ machine[$3 + 1] = $5; }
	| dostart statements WHILE '('	{ $<num>$ = progi; }
	  expr ')' ';'			{ machine[$1 + 1] =
					  code3(DOWHILE, $1 + 2, $<num>5);
					  loop_depth--; }
	| atstart statements endat	{ machine[$1 + 1] = $3; }
	| expr ';'			{ code(POP); }
        | ID '=' expr ';'
	    { code_assign($1, ASGNLVAR, ASGNGVAR); }
	| ID '[' expr ']' '=' expr ';'
	    { code_assign($1, ASGNLVARINDEX, ASGNGVARINDEX); }
	| T_RETURN expr ';'		{ code(T_RETURN); }
	| T_RETURN ';'			{ code3(NUMPUSH, 0, T_RETURN); }
	| T_RAISE expr ';'		{ code(T_RAISE); }
	| CONTINUE count ';'	
	    {
	        if ($2 < 1 || $2 > loop_depth) {
		    yyerror("Invalid continue statement.");
		} else {
		    code2(CONTINUE, $2);
		}
	    }
	| BREAK count ';'
	    {
	        if ($2 < 1 || $2 > loop_depth) {
		    yyerror("Invalid break statement.");
		} else {
		    code2(BREAK, $2);
		}
	    }
	| ';'
	| error ';'
	;
count:	  /* NOTHING */		{ $$ = 1; }
	| NUMBER_CONST		{ $$ = $1; }
	;
ifstart:
	  IF '(' expr ')'	{ $$ = code3(IF, 0, 0); }
	;
elseifs:
	  /* NOTHING */				 { $$ = -1; }
	| elseifs elseifstart statements end	
	    { machine[$2 + 1] = $4; if ($1 >= 0) $$ = $1; else $$ = $2; }
	;
elseifstart:
	  ELSEIF '(' expr ')'	{ $$ = code2(ELSEIF, 0); }
	;
elsepart:
	  /* NOTHING */
	| ELSE { code(ELSE); } statements end
	;
end:      /* NOTHING */		{ $$ = code(STOP) + 1; }
	;
endif:	    ENDIF			{ $$ = code(STOP) + 1; }
	;
whilecond:	'(' expr ')'	{ $$ = code2(WHILE, 0); loop_depth++; }
	;
endwhile:   ENDWHILE		{ loop_depth--; $$ = code(STOP) + 1; }
	;
dostart:	DO		{ $$ = code2(DO, 0); loop_depth++; }
	;
forstart:
	  FOR ID IN '(' expr ')'
			        { loopvar = find_loopvar($2);
				  $$ = code2(NUMPUSH, 0);
				  code3(FOR, loopvar, 0);
				  loop_depth++; }
	;
forrng:
	  FOR ID IN '[' expr UPTO expr ']'	
				{ loopvar = find_loopvar($2);
				  $$ = code3(FORRNG, loopvar, 0); 
				  loop_depth++; }
	;
endfor:	  ENDFOR		{ loop_depth--; $$ = code(STOP) + 1; }
	;
atstart:
	  AT '(' expr ')'	{ $$ = code2(AT, 0); }
	;
endat:
	  ENDAT			{ $$ = code(STOP) + 1; }
	;
arglist:
	  /* NOTHING */	
	| nonempty_arglist
	;
nonempty_arglist:
	  argexpr
        | nonempty_arglist ',' argexpr
        ;
argexpr:
	  expr			{ }
	| '@' expr		{ code(SPLICE); }
	;
maplist:
	  /* NOTHING */		{ $$ = 0; }
	| expr MAPSTO expr		{ $$ = 1; }
	| maplist ',' expr MAPSTO expr	{ $$ = $1 + 1; }
	;
object_const:
	  '#' NUMBER_CONST '@' ID
	    {
		int		serv;

		if ((serv = serv_name2id(sym_get(cur_obj, $4)->str)) < 0) {
		    error("Server not found:", sym_get(cur_obj, $4)->str);
		} else {
		    $$.id = $2;
		    $$.server = serv;
		}
		sym_free(cur_obj, $4);
	    }
	| '#' NUMBER_CONST
	    { $$.id = $2; $$.server = 0; }
	| '#' '-' NUMBER_CONST	 
	    { $$.id = - $3; $$.server = 0; }
	;
opt_pass_to:
	  /* NOTHING*/
	    {
		if (!cur_obj->parents->len) {
		    yyerror("Object has no parents to pass() to");
		} else {
		    $$ = cur_obj->parents->el[0].v.obj;
		}
	    }
	| TO object_const
	    {
		if (!hasparent(cur_obj, $2)) {
		    yyerror("Object doesn't have indicated parent");
		} else {
		    $$ = $2;
		}
	    }
pass_expr:
	  PASS '(' { code(ARGSTART); } arglist ')' opt_pass_to
	    {
		$$ = code2(PASS, $6.id);
	    }
	;
builtin_objvar:
	  PLAYER	{ $$ = code(PLAYER); }
	| CALLER	{ $$ = code(CALLER); }
	| THIS		{ $$ = code(THIS); }
	| ARGS		{ $$ = code(ARGS); }
	;
expr:	  NUMBER_CONST	{ $$ = code2(NUMPUSH, $1); }
	| STRING_CONST  { $$ = code2(STRPUSH, $1); }
	| object_const  { $$ = code3(OBJPUSH, $1.id, $1.server); }
	| ERROR_CONST	{ $$ = code2(ERRPUSH, $1); }
	| TYPE_SPEC	{ $$ = code2(NUMPUSH, $1); }
	| '$' ID	{ $$ = code2(GETSYSVAR, $2); }
	| '{' { code(ARGSTART); } arglist '}'	{ $$ = code(LISTPUSH); }
	| '[' maplist ']'	{ $$ = code2(MAPPUSH, $2); }
	| pass_expr
	| PARENTS	{ $$ = code(PARENTS); }
	| ID '(' { code(ARGSTART); } arglist ')'
	    {
		int	f = bf_lookup(sym_get(cur_obj, $1)->str);
		    
		if (f < 0) {
		    error("Built-in function not found:  ",
			sym_get(cur_obj, $1)->str);
		    break;
		} else {
		    $$ = code(f);
		}
		sym_free(cur_obj, $1);
	    }
	| builtin_objvar
	| var
	| expr '.' ID
	    { $$ = $1; code3(ARGSTART, MESSAGE, $3); }
	| expr '.' '(' expr ')'
	    { $$ = $1; code2(ARGSTART, MESSAGE_EXPR); }
	| expr '.' ID '(' { code(ARGSTART); } arglist ')'
	    { $$ = $1; code2(MESSAGE, $3); }
	| expr '.' '(' expr ')' '(' { code(ARGSTART); } arglist ')'
	    { $$ = $1; code(MESSAGE_EXPR);  }
	| expr '[' expr ']'	{ $$ = $1; code(INDEX); }
	| expr '[' expr UPTO expr ']'		{ $$ = $1; code(SUBSET); }
	| expr '[' UPTO expr ']'		{ $$ = $1; code(LSUBSET); }
	| expr '[' expr UPTO ']'		{ $$ = $1; code(RSUBSET); }
	| expr '+' expr		{ $$ = $1; code(ADD); }
	| expr '-' expr		{ $$ = $1; code(SUB); }
	| expr '*' expr		{ $$ = $1; code(MUL); }
	| expr '/' expr		{ $$ = $1; code(DIV); }
	| expr '%' expr		{ $$ = $1; code(MOD); }
	| '-' expr  %prec NEGATE	{ $$ = $2; code(NEGATE); }
	| expr AND		{ $<num>$ = code2(AND, 0); }
	           expr		{ machine[$<num>3 + 1] = code(ENDAND);
				  $$ = $1; }
	| expr OR 		{ $<num>$ = code2(OR, 0); }
		   expr		{ machine[$<num>3 + 1] = code(ENDOR);
		  		  $$ = $1; }
	| NOT expr		{ $$ = $2; code(NOT); }
	| expr EQ expr		{ $$ = $1; code(EQ); }
	| expr NE expr		{ $$ = $1; code(NE); }
	| expr LT expr		{ $$ = $1; code(LT); }
	| expr LE expr		{ $$ = $1; code(LE); }
	| expr GT expr		{ $$ = $1; code(GT); }
	| expr GE expr		{ $$ = $1; code(GE); }
	| '(' expr ')'		{ $$ = $2; }
	| expr IN expr		{ $$ = $1; code(IN); }
	;
var:	ID
	    {
		int	varno;
		Var	v;

		if ((varno = builtin_var($1))) {
		    $$ = code(varno);
		} else if (cur_method && var_find(cur_method->vars, $1, &varno)) {
		    $$ = code2(GETLVAR, varno);
		    sym_free(cur_obj, $1);
		} else if (cur_obj && cur_obj->parents
       && var_get_global(cur_obj, sym_get(cur_obj, $1)->str, &v) == E_NONE) {
		    $$ = code2(GETGVAR, $1);
		} else {
		    error("Variable not declared:  ",
			sym_get(cur_obj, $1)->str);
		    sym_free(cur_obj, $1);
		}
	    }
	;
const_expr:
	  STRING_CONST
	    { $$.type = STR;  $$.v.str = string_dup(sym_get(cur_obj, $1));
	      sym_free(cur_obj, $1); }
	| NUMBER_CONST
	    { $$.type = NUM;  $$.v.num = $1; }
	| '-' NUMBER_CONST
	    { $$.type = NUM;  $$.v.num = - $2; }
	| object_const
	    { $$.type = OBJ;  $$.v.obj = $1; }
	| ERROR_CONST
	    { $$.type = ERR;  $$.v.err = $1; }
	| list_const
	    { $$.type = LIST;  $$.v.list = $1; }
	| map_const
	    { $$.type = MAP;  $$.v.map = $1; }
	;
list_const:
	  '{' list_of_constants '}'		{ $$ = $2; }
	;
map_const:
	  '[' list_of_constant_pairs ']'	{ $$ = $2; }
	;
list_of_constants:
	  /* NOTHING */
	    { $$ = list_new(0); }
	| const_expr
	    { $$ = list_new(1);  $$->el[0] = $1; }
	| list_of_constants ',' const_expr
	    { $$ = list_append($1, $3, -1); }
	;
list_of_constant_pairs:
	  /* NOTHING */
	    { $$ = map_new(0); }
	| const_expr MAPSTO const_expr
	    { $$ = map_new(0);  map_add($$, $1, $3); }
	| list_of_constant_pairs ',' const_expr MAPSTO const_expr
	    { $$ = map_add($1, $3, $5); }
	;
%%

static void	 (*parse_perror)(const char *s);
static int	 (*parse_getc)(void);
static void	 (*parse_ungetc)(int c);
static int	 nerrors;
static Objid	 programmer;
static int	 parse_start;		/* flag for start of object or method */
static int	 line_start;		/* flag for start of line */
static int	 cpp_output;            /* flag: interpret # at line start */
static char	 progfile_name[MAX_PATH_LEN];
static int	 eoo;			/* end of object flag */
static void	 skip_whitespace(void);

static int
follow(int expect, int ifyes, int ifno)     /* look ahead for >=, etc. */
{
    int c = (*parse_getc)();

    if (c == expect) {
        return ifyes;
    }
    (*parse_ungetc)(c);
    return ifno;
}

struct tt_entry {
    const char	*name;
    int		token;
} tokentable[] = {
    { "if",		IF },
    { "else",		ELSE },
    { "elseif",		ELSEIF },
    { "endif",		ENDIF },
    { "for",		FOR },
    { "while",		WHILE },
    { "do",		DO },
    { "in",		IN },
    { "to",		TO },
    { "endfor",		ENDFOR },
    { "endwhile",	ENDWHILE },
    { "at",		AT },
    { "endat",		ENDAT },
    { "object",		OBJECT },
    { "endobject",	ENDOBJECT },
    { "method",		METHOD },
    { "endmethod",	ENDMETHOD },
    { "verb",		VERB },
    { "ignore",		IGNORE },
    { "parents",	PARENTS },
#if 0
    { "args",		ARGS },
    { "player",		PLAYER },
    { "caller",		CALLER },
    { "this",		THIS },
#endif
    { "return",		T_RETURN },
    { "raise",		T_RAISE },
    { "continue",	CONTINUE },
    { "break",		BREAK },
    { "pass",		PASS },
    { "var",		VAR },
    { "blocked",	T_BLOCKED },
};

static void
get_lineno(void)
{
    int		c, i;

    lineno = 0;
    while ((c = (*parse_getc)()) == ' ')		/* skip leading space */
	;					
    while (isdigit(c)) {
	lineno = 10 * lineno + c - '0';		/* store it, depends on ASCII */
	if ((c = (*parse_getc)()) == EOF) {		/* if EOF, */
	    return;				/* abort */
	}
    }
    while ((c = (*parse_getc)()) != '"') {		/* skip to first " */
	if (c == EOF || c == '\n') {		/* abort if no " before EOL */
	    return;
	}
    }
    for (i = 0; i < MAX_PATH_LEN - 1; i++) {
        if ((c = (*parse_getc)()) == EOF || c == '"' || c == '\n') {
	    break;
	}
	progfile_name[i] = c;
    }
    progfile_name[i] = '\0';
    while (c != '\n' && c != EOF) {
	c = (*parse_getc)();
    }
}

static void
skip_whitespace(void)
{
    int		c;

    do {
	c = (*parse_getc)();
	if (c == '\n') {
	    line_start = 1;
	    lineno++;
	} else if (isspace(c)) {
	    line_start = 0;
	}
    } while (isspace(c));
    (*parse_ungetc)(c);
}

static int
tokenize_number(int c)
{
    int		n = 0;

    do {
	n = n * 10 + c - '0';
	c = (*parse_getc)();
    } while (isdigit(c));
    (*parse_ungetc)(c);
    yylval.num = n;
    return NUMBER_CONST;
}

static int
type_spec(const char *s, Type_spec *t)
{
    if (!cool_strcasecmp(s, "num")) {
	*t = NUM;  return 1;
    } else if (!cool_strcasecmp(s, "str")) {
	*t = STR;  return 1;
    } else if (!cool_strcasecmp(s, "obj")) {
	*t = OBJ;  return 1;
    } else if (!cool_strcasecmp(s, "list")) {
	*t = LIST;  return 1;
    } else if (!cool_strcasecmp(s, "map")) {
	*t = MAP;  return 1;
    } else if (!cool_strcasecmp(s, "err")) {
	*t = ERR;  return 1;
    }
    return 0;
}

static int
tokenize_ident(int  c)
{
    int		i;
    String	*str;
    Error	e;
    Type_spec	t;

    str = string_new(0);
    str = string_catc(str, c);
    while(isalnum(c = (*parse_getc)()) || c == '_') {
	str = string_catc(str, c);
    }
    (*parse_ungetc)(c);

/*
 * check for multi-char alpha tokens (if, for, else, etc.)
 */
    for (i = 0; i < Arraysize(tokentable); i++) {
	if (!cool_strcasecmp(str->str, tokentable[i].name)) {
	    string_free(str);
	    if (tokentable[i].token == ENDOBJECT) {
		eoo = 1;
	    }
	    return tokentable[i].token;
	}
    }
/*
 * check for error names (E_*)
 */
    if (str->str[0] == 'E' && str->str[1] == '_') {
	e = err_name2id(str->str);
	yylval.num = (int) e;
	string_free(str);
	return ERROR_CONST;
    }
/*
 * check for type specifiers 
 */
    if (type_spec(str->str, &t)) {
	string_free(str);
	yylval.num = (int) t;
	return TYPE_SPEC;
    }
/*
 * otherwise, it's just a normal boring identifier
 */
    yylval.num = sym_add(cur_obj, str);
    return ID;
} 

static int
tokenize_string(int c)
{
    String	*str;

    str = string_new(0);
    while(1) {
	c = (*parse_getc)();
	if (c == '"') {
	    break;
	} else if (c == '\\') {		/* blackslash escapes */
	    c = (*parse_getc)();
	    switch(c) {
	      case 'n':			/* newline */
		  /* turn it into a CRLF for braindead clients */
		str = string_cat(str, "\r\n");
		continue;
	      case 't':			/* tab */
		c = '\t'; break;
	      default:			/* default escape does nothing */
		break;			/* this handles \", \\ and \<cr> */
	    } /* switch */
	} else if (c == '\n' || c == EOF) {
	    error("Missing quote", "");
	    break;
	}
	str = string_catc(str, c);
    }
    yylval.num = sym_add(cur_obj, str);
    return STRING_CONST;
}

static int
yylex(void)
{
    int		c;

    if (parse_start) {
	parse_start = 0;
	if (parse_expecting == OBJECT) {
	    return EXPECTING_OBJECT;
	} else if (parse_expecting == METHOD) {
	    return EXPECTING_METHOD;
	} /* if */
    } /* if */

    if (eoo) {			/* end of object, return end of file */
	return EOF;
    }
    skip_whitespace();
    c = (*parse_getc)();

    while (cpp_output && line_start && c == '#') {
	get_lineno();		/* get line number from # directive */
	skip_whitespace();
	c = (*parse_getc)();
    }
    line_start = 0;
    if (isdigit(c)) {
	return tokenize_number(c);
    } else if (isalpha(c) || c == '_')  {
	return tokenize_ident(c);
    } else if (c == '"') {	/* string constants */
	return tokenize_string(c);
    } 
    switch(c) {
      case '>':         return follow('=', GE, GT);
      case '<':         return follow('=', LE, LT);
      case '=':         return follow('=', EQ, follow('>', MAPSTO, '='));
      case '!':         return follow('=', NE, NOT);
      case '&':		return follow('&', AND, '&');
      case '|':		return follow('|', OR, '|');
      case '.':		return follow('.', UPTO, '.');
      default:          return c;
    }
}

static int
obj_result(Object *new, int expecting, int do_init)
{
    char	 buf[80];
    int		 exists, r = 1;

    exists = valid(new->id);

    if (!new) {
	sprintf(buf, "Null compile; end of objects assumed.");
    } else if (nerrors > 0) {
	if (new->id.id >= 0) {
	    sprintf(buf, "%d error(s).  Object #%d not programmed.", nerrors,
		    new->id.id);
	} else {
	    sprintf(buf, "%d error(s).", nerrors);
	}
	free_object(new);
    } else if (expecting >= 0 && !exists) {
	sprintf(buf, "Object #%d does not exist.", new->id.id);
	free_object(new);
    } else if (expecting >= 0 && !can_program(programmer, new->id)) {
	sprintf(buf, "Object #%d:  Permission denied.", new->id.id);
	free_object(new);
    } else {
	assign_object(new->id, new);
	if (exists) {
	    sprintf(buf, "Object #%d reprogrammed.", new->id.id);
	} else {
	    sprintf(buf, "Object #%d programmed.", new->id.id);
	    if (do_init) {
		send_message(-1, 0, 0, sys_obj, sys_obj, new->id,
		    sym_sys(INIT), list_dup(empty_list), 0, new->id);
	    }
	}
	r = 0;
    }
    parse_perror(buf);
    return r;
}

int
compile(Playerid player, int (*f_getc)(void), void (*f_ungetc)(int c),
	void (*f_perror)(const char *s), int expecting, Object *o,
        String *mname, int is_cpp_output, int do_init)
{
    int		c;

    cpp_output = is_cpp_output;
    nerrors = 0;
    lineno = 1;
    line_start = 1;
    eoo = 0;
    programmer.id = player;
    programmer.server = 0;
    progfile_name[0] = '\0';

    parse_getc = f_getc;
    parse_ungetc = f_ungetc;
    parse_perror = f_perror;
    cur_method = 0;

    switch(expecting) {
      case -1:			/* expecting objects, can create new */
      case 0:			/* expecting objects */
	parse_expecting = OBJECT;
	while((c = (*parse_getc)()) != EOF) {
	    (*parse_ungetc)(c);
	    cur_obj = new_object();
	    sym_init(cur_obj);
	    cur_method = 0;
	    parse_start = 1;
	    yyparse();
	    eoo = 0;
	    if (obj_result(cur_obj, expecting, do_init)) {
		break;
	    }
	    skip_whitespace();
	}
	break;
      case 1:			/* expecting a single method */
	parse_start = 1;
	parse_expecting = METHOD;
	cur_obj = o;
	start_method(sym_add(o, string_dup(mname)), 0);
	yyparse();
	if (nerrors) {
	    char	buf[80];
	    sprintf(buf, "%d error(s).  Method not programmed.", nerrors);
	    parse_perror(buf);
	    code_copy(cur_method);
	    free_method(cur_obj, cur_method);
	} else {
	    end_method();
	    cache_put(cur_obj, cur_obj->id.id);
	}
	break;
    }
    return nerrors;
}

static void
error(const char  *s, const char *t)	/* print error message */
{
    String	*str;

    nerrors++;
    str = string_new(0);
    if (progfile_name[0]) {
	str = string_cat(str, progfile_name);
	str = string_cat(str, ":  ");
    }
    str = string_catnum(str, lineno);
    str = string_cat(str, ":  ");
    str = string_cat(str, s);
    if (t) {
	str = string_catc(str, ' ');
	str = string_cat(str, t);
    }
    (*parse_perror)(str->str);
    string_free(str);
}

static void  yyerror(const char *s)
{
    error(s, "");
}

static void
end_method(void)
{
    Method	*m;
 
    if (!cur_method) {
	return;
    }
    code(STOP);
    code_copy(cur_method);
    if (parse_expecting == METHOD) {
	rm_method(cur_obj, sym_get(cur_obj, cur_method->name)->str);
    } else if ((m = find_method(cur_obj,
	    sym_get(cur_obj, cur_method->name)->str))) {
	error("Method multiply defined:  ", sym_get(cur_obj, m->name)->str);
	free_method(cur_obj, cur_method);
	return;
    }
    add_method(cur_obj, cur_method);
}

int
code_assign(int varname, int local_opcode, int global_opcode)
{
    int		varno;
    int		addr = 0;
    Var		v;

    if ((varno = builtin_var(varname))) {
	if (local_opcode == ASGNLVARINDEX) {
	    error("Cannot index assign built-in variable",
		  sym_get(cur_obj, varname)->str);
	} else if (varno == PLAYER) {
	    code(SETPLAYER);
	} else {
	    error("Cannot assign to built-in variable",
		   sym_get(cur_obj, varname)->str);
	}
    } else if (cur_method && var_find(cur_method->vars, varname, &varno)) {
	addr = code2(local_opcode, varno);
	sym_free(cur_obj, varname);
    } else if (cur_obj && cur_obj->parents &&
	       var_get_global(cur_obj,
	      	sym_get(cur_obj, varname)->str, &v) == E_NONE) {
	addr = code2(global_opcode, varname);
    } else {
	error("Variable not declared:  ",
	    sym_get(cur_obj, varname)->str);
	sym_free(cur_obj, varname);
    }
    return addr;
}

static void
do_check_parents(Object *o)
{
    switch(check_parents(programmer, o, o->parents)) {
      case E_NONE:
	return;
      case E_INVIND:
	yyerror("Parent must be local");
	break;
      case E_OBJNF:
	yyerror("Parent does not exist");
	break;
      case E_MAXREC:
	yyerror("Parents list would cause a loop");
	break;
      case E_RANGE:
	yyerror("Object must have at least one parent");
	break;
      case E_PERM:
	yyerror("Cannot use parent; Permission Denied");
	break;
      default:
	yyerror("Parents list invalid");
	break;
    }
}

static void
add_parent(Object *o, Objid newparent)
{
    Var		v;

    v.type = OBJ;
    v.v.obj = newparent;
    if (!o) {
	return;
    } else if (!o->parents) {
	o->parents = list_new(1);
	o->parents->el[0] = v;
    } else {
	o->parents = list_setadd(o->parents, v);
    }
}

static void
start_method(int name, int blocked)
{
    int		e;

    loop_depth = 0;
    cur_method = MALLOC(Method, 1);
    cur_method->ref = 1;
    cur_method->name = name;
    cur_method->blocked = blocked;
    cur_method->ninst = 0;
    cur_method->code = 0;
    cur_method->vars = 0;
    for (e = 0; e < NERRS; e++) {
	cur_method->ehandler[e] = EH_DEFAULT;
    }
    cur_method->next = 0;
}

static int
find_loopvar(int varname)
{
    int		varno = 0;
    Var		dummy;

    if (var_get_global(cur_obj,sym_get(cur_obj, varname)->str, &dummy)
	!= E_VARNF) {
	    yyerror("'for' variable must be local");
	    varno = -1;
    } else if (var_add_local(cur_method, varname, &varno)) {
	sym_free(cur_obj, varname);
    }
    return varno;
}

static void
new_global_var(int name, Type_spec type, Var value)
{
    Var	r;

    if (type != value.type) {
	error("Type mismatch on initializer for variable: ",
	      sym_get(cur_obj, name)->str);
	sym_free(cur_obj, name);
    } /* if */
    if (var_get_global(cur_obj, sym_get(cur_obj, name)->str, &r) == E_NONE) {
	if (type != r.type) {
	    error("Type mismatch on inherited variable: ",
		  sym_get(cur_obj, name)->str);
	    sym_free(cur_obj, name);
	} /* if */
    } /* if */
    if (var_add_global(cur_obj, name, value) != E_NONE) {
	error("Variable multiply defined:  ",
	    sym_get(cur_obj, name)->str);
	sym_free(cur_obj, name);
    } /* if */
} /* new_global_var() */

static int
builtin_var(int varno)
{
    const char	*name = sym_get(cur_obj, varno)->str;

    if (!cool_strcasecmp(name, "player")) {
	return PLAYER;
    } else if (!cool_strcasecmp(name, "caller")) {
	return CALLER;
    } else if (!cool_strcasecmp(name, "args")) {
	return ARGS;
    } else if (!cool_strcasecmp(name, "this")) {
	return THIS;
    } else {
	return 0;
    }
}
