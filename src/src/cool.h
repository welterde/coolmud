/* Copyright (c) 1993 Stephen F. White */

#ifndef COOL_H
#define COOL_H

#include "string.h"

typedef enum	Type_spec {
    STR, NUM, OBJ, LIST, MAP, ERR, PC
} Type_spec;

typedef struct Var  Var;

typedef struct	Objid {
    short	server;		/* server id (0 == this server) */
    int		id;		/* object id */
} Objid;

typedef struct	List {
    int		 len;		/* length of list */
    int		 mem;		/* memory actually allocated */
    int		 ref;		/* reference count */
    Var		*el;		/* elements of list */
} List;

typedef struct  MapPair MapPair;

typedef enum	Error {
    E_NONE, E_TYPE, E_RANGE, E_INVIND, E_DIV, E_MAXREC,
    E_METHODNF, E_VARNF, E_VERBNF, E_FOR, E_PERM, E_SERVERNF, E_SERVERDN,
    E_OBJNF, E_MESSAGE, E_TIMEOUT, E_STACKUND, E_STACKOVR, E_INTERNAL,
    E_FILE, E_ARGTYPE, E_NARGS, E_TICKS, E_TERM, E_MAPNF
} Error;

#define NERRS 25

/*
 * cannot ignore or catch these errors
 */
#define NOIGNORE(err) (err == E_STACKUND || err == E_STACKOVR \
		    || err == E_INTERNAL || err == E_TICKS)

typedef struct HashT {
    int		size;	/* width of hash table */
    int		num;	/* number of elements in table */
    int		ref;	/* reference count */
    void      **table;	/* have to use void *, for multiple datatypes */
} HashT;

typedef HashT Map;

struct	Var {
    Type_spec	type;
    union {
	int	 num;		/* number */
	Objid	 obj;		/* object id */
	String	*str;		/* string header */
	List	*list;		/* list header */
	Map	*map;		/* map header */
	Error	 err;		/* error */
    } v;
};

struct  MapPair {
    Var		 from;		/* key of map element */
    Var		 to;		/* data of map element */
    MapPair	*next;
};

typedef struct Symbol {
    int		 ref;	/* local object references to symbol */
    String	*s;	/* symbol */
} Symbol;

#define ISTRUE(x)  ((x.type == NUM && x.v.num) \
		||  (x.type == STR && x.v.str->str[0]) \
		||  (x.type == OBJ && x.v.obj.id >= 0) \
		||  (x.type == LIST && x.v.list->len))

typedef int	Inst;
typedef struct Method	Method;
typedef struct Verbdef		Verbdef;
typedef struct Vardef		Vardef;

typedef struct Lock		Lock;

struct Lock {
    String	*name;
    int		 added_by;	/* msgid which added this lock */
    Lock	*next;
};

typedef struct Object {
    Objid	 id;		/* object's id */
    int		 ref;		/* reference count */
    List	*parents;	/* parent list */
    Lock	*locks;
    HashT	*methods;	/* method list, hashed */
    Verbdef	*verbs;
    HashT	*vars;		/* variables, hashed */
    int		 var_hashsize;
    int		 nsymb;		/* number of symbols */
    int		 st_size;	/* memory actually allocated for symbols */
    Symbol	*symbols;	/* symbol table */
    int		 last_search;	/* generational flag, last search on object */
				/* kudos to greg hudson for the idea */
} Object;

enum eh { EH_DEFAULT, EH_IGNORE, EH_CATCH };

struct Method {
    int		 name;			/* index into symbol table */
    int		 ninst;			/* number of instructions in machine */
    Inst	*code;			/* the compiled code */
    enum eh	 ehandler[NERRS];	/* error handler */
    Vardef	*vars;			/* local variables */
    int		 ref;			/* reference count */
    Method	*next;			/* next method in linked list */
    int		 blocked : 1;		/* non-overrideable ("nomask") flag  */
};

struct Verbdef {
    int		 verb;			/* name of verb */
    int		 prep;			/* preposition, or -1 for none */
    int		 method;		/* method to call */
    Verbdef	*next;			/* next verbdef in linked list */
};

struct Vardef {
    int		 name;			/* index into symbol table */
    Var		 value;			/* value */
    Vardef	*next;			/* next vardef in linked list */
};

enum state	{ RUNNING,	/* program is running */
		   STOPPED,	/* program stopped by return or end of code */
		   HALTED,	/* program halted by method called */
		   RAISED,	/* program stopped by raise() */
		   BLOCKED };	/* program blocked on event */

typedef void (*PFV)(void);

typedef enum Sys_sym {
    BLANK, INIT, PARSE, BOOT_SERVER, CONNECT_SERVER, DISCONNECT_SERVER,
    SYM_CONNECT, SYM_DISCONNECT, RAISE, RETURN
} Sys_sym;

enum arg_type { none, NUM_ARG, ID_ARG, STR_ARG, VARNO_ARG };

struct bf_entry {
    int         num;
    const char  *name;
    int         minargs;
    int         maxargs;
};

typedef struct Op_entry {
    int		opcode;
    const char  name[MAX_TOKEN_LEN];
    PFV		func;
    int		nargs;
    enum arg_type	arg_type[3];
    struct bf_entry	*builtin;
} Op_entry;

#endif /* !COOL_H */
