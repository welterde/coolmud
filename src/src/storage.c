/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "netio.h"
#include "servers.h"

static int	dbtop(void);
static void	set_dbtop(int newdbtop);
void		free(MALL_T ptr);
static void	free_vars(Object *o, Vardef *v);
static void	store_malloc(void *where, int size);
static void	delete_malloc(void *where);

Object *
new_object(void)
{
    Object	 *o = MALLOC(Object, 1);

    o->id.server = 0;  o->id.id = -1;
    o->ref = 1;
    o->parents = 0;
    o->locks = 0;
    o->vars = 0;
    o->verbs = 0;
    o->symbols = 0;
    o->methods = 0;
    o->st_size = 0;
    o->methods = 0;
    o->last_search = -1;
    return o;
}

Object *
clone(Objid pid)
{
    Object	*o;
    int		 top;

    if (pid.server != 0 || pid.id < 0) {
	return 0;
    } else {
	top = dbtop() + 1;
	set_dbtop(top);
	o = new_object();
	o->id.server = 0;
	o->id.id = top;
	sym_init(o);
	o->parents = list_new(1);
	o->parents->el[0].type = OBJ;
	o->parents->el[0].v.obj = pid;
	assign_object(o->id, o);
	return o;
    }
}

int
destroy(Objid oid)
{
    if (!valid(oid)) {
	return 1;
    } else {
	cache_del(oid.id, 0);
	return 0;
    }
}

static int
dbtop(void)
{
    Var		top;

    if (sys_get_global("dbtop", &top) != E_NONE) {
	return 0;
    } else if (top.type != NUM) {
	writelog();
	fprintf(stderr, "#%d.dbtop non-numeric\n", SYS_OBJ);
	return 0;
    } else {
	return top.v.num;
    }
}
    
static void
set_dbtop(int newdbtop)
{
    Var		v;

    v.type = NUM;  v.v.num = newdbtop;
    sys_assign_global("dbtop", v);
}

void
assign_object(Objid oid, Object *new)
{
    Object	*old;

    if (oid.server != 0 || oid.id < 0) {
	return;
    }
    if ((old = retrieve(oid))) {
	old->ref++;			/* save old obj till we're done */
	cache_put(new, oid.id);		/* put in new one */
	var_copy_vars(old, new);	/* copy vars from old to new */
	free_object(old);		/* really free old one */
    } else {
	cache_put(new, oid.id);
    }
    if (oid.id > dbtop()) {
	set_dbtop(oid.id);
    }
}

static void
free_vars(Object *o, Vardef *v)
{
    Vardef	*vnext;

    if (!v) {
	return;
    }
    for (; v; v = vnext) {
	vnext = v->next;
	sym_free(o, v->name);
	var_free(v->value);
	FREE(v);
    }
}

void
free_method(Object *o, Method *m)
{
    if (--m->ref) {
	return;
    }
    sym_free(o, m->name);
    free_vars(o, m->vars);
    opcode_free_symbols(o, m);
    if (m->code) {
	FREE(m->code);
    }
    FREE(m);
}

void
free_object(Object *o)
{
    Method	*m, *mnext;
    Verbdef	*vb, *vbnext;
    int		 i, hval;

    if (--o->ref) {	/* if refcount is nonzero, */
	return;		/* don't free object */
    }
    if (o->parents) {
	list_free(o->parents);
    }
    if (o->methods) {
	for (hval = 0; hval < o->methods->size; hval++) {
	    for (m = o->methods->table[hval]; m; m = m->next) {
		mnext = m->next;
		free_method(o, m);
	    }
	}
	FREE(o->methods);
    }
    for (vb = o->verbs; vb; vb = vbnext) {
	vbnext = vb->next;
	sym_free(o, vb->verb);
	if (vb->prep >= 0) {
	    sym_free(o, vb->prep);
	}
	sym_free(o, vb->method);
	FREE(vb);
    }
    if (o->vars) {
	for (hval = 0; hval < o->vars->size; hval++) {
	    free_vars(o, o->vars->table[hval]);
	}
	FREE(o->vars);
    }
    if (o->symbols) {
	for (i = 0; i < o->nsymb; i++) {
	    if (o->symbols[i].ref > 0) {
		string_free(o->symbols[i].s);
	    }
	}
	FREE(o->symbols);
    }
    FREE(o);
}

struct mem_tbl {
    void	*where;
    int		size;
    struct mem_tbl	*next;
};

struct mem_tbl *sm_head;

static void
store_malloc(void *where, int size)
{
    struct mem_tbl	*s;
    s = (struct mem_tbl *) malloc(sizeof (struct mem_tbl));
    s->where = where;
    s->size = size;
    s->next = sm_head;
    sm_head = s;
}

static void
delete_malloc(void *where)
{
    struct mem_tbl	*s, *t;

    if (where == sm_head->where) {
	s = sm_head;
	sm_head = sm_head->next;
	free((void *) s);
	return;
    }
    for (s = sm_head; s->next; s = s->next) {
	if (s->next->where == where) {
	    t = s->next;
	    s->next = s->next->next;
	    free((void *) t);
	    return;
	}
    }
    writelog();
    fprintf(stderr, "MALLOC:  non-malloc'ed memory freed at 0x%x!\n",
	(int) where);
    kill(getpid(), SIGQUIT);
}

static int	chunks = 0;

const char
*check_malloc(void)
{
    struct mem_tbl	*s;
    static char		 buf[80];
    int			 total = 0;

#ifdef DEBUG_MALLOC
    for (s = sm_head; s; s = s->next) {
	total += s->size;
    }
    sprintf(buf, "%d bytes used in %d chunks.", total, chunks);
#else
    sprintf(buf, "%d chunks of memory allocated.", chunks);
#endif
    return buf;
}

void *
cool_malloc(unsigned  size)
{
    void *	memptr;

    memptr = (void *) malloc(size);
    if (!memptr) {
	writelog();
	fprintf(stderr, "MALLOC (size %d bytes) failed!\n", size);
	kill(getpid(), SIGQUIT);
    }
#ifdef DEBUG_MALLOC
    store_malloc(memptr, size);
#endif
    chunks++;
    return memptr;
}

void
cool_free(void *ptr)
{
#ifdef DEBUG_MALLOC
    delete_malloc(ptr);
#endif
    free((void *) ptr);
    chunks--;
}

void  write_object(FILE *f, Objid oid);
static char	*written;

void
write_flatfile(const char *dbfile, const char *dumpfile)
{
    FILE	*f;
    Objid	 oid;
    int		 top;

    init(dbfile, 0, 1);
    top = dbtop();
    if (!strcmp(dumpfile, "-")) {
	f = stdout;
    } else {
	f = fopen(dumpfile, "w");
	if (!f) {
	    writelog();
	    perror(dumpfile);
	    return;
	} /* if */
    } /* if */

    written = MALLOC(char, top + 1);
    if (!written) return;
    oid.server = 0;
    for (oid.id = 0; oid.id <= top; oid.id++) written[oid.id] = 0;

    for (oid.id = 0; oid.id <= top; oid.id++) {
	write_object(f, oid);
	cache_reset();
    } /* for */

    FREE(written);
    fclose(f);
    shutdown_server();
} /* write_flatfile */

void
write_object(FILE *f, Objid oid)
{
    String	*listing;
    int		 i;
    Object	*o = retrieve(oid);

    if (!o || written[oid.id]) return;
    for (i = 0; i < o->parents->len; i++) {
	if (o->parents->el[i].v.obj.id > oid.id) {
	    write_object(f, o->parents->el[i].v.obj);
	} /* if */
    } /* for */
    listing = decompile_object(o);
    listing = string_strip_cr(listing);
    fputs(listing->str, f);
    string_free(listing);
    written[oid.id] = 1;
}
