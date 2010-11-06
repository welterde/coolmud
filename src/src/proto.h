/* Copyright (c) 1993 Stephen F. White */

#ifndef PROTO_H
#define PROTO_H

/*
 * from builtins.c
 */

extern int	bf_lookup(const char *name);
extern const char	*bf_id2name(int id);
extern struct bf_entry	*bf_id2entry(int id);

/* 
 * from db/cache.c
 */

extern Object  *cache_get(int oid);
extern int	cache_put(Object *obj, int oid);
extern int	cache_del(int oid, int flg);
extern int	cache_check(int oid);
extern int	cache_init(void);
extern int	cache_sync(void);
extern void	cache_reset(void);
extern int	cacheconfig(const char *line, int lineno);
extern const char	*cache_stats(void);

/*
 * from code.c
 */

extern void	code_copy(Method *m);
extern void	code_init(void);
extern int	code(Inst);

/*
 * from cool.y
 */

extern int	compile(Playerid player, int (*f_getc)(void), void (*f_ungetc)(int c),
		  void (*f_perror)(const char *s), int expecting, Object *o, String *mname,
		  int is_cpp_output, int do_init);

/*
 * from db/dbmchunk.c
 */

extern int	dddb_init(int db_must_exist);
extern int	dddb_setfile(const char *fil);
extern int	dddb_close(void);

/*
 * from perms.c
 */
extern int	is_wizard(Objid player);
extern int	owns(Objid player, Objid what);
extern int	can_program(Objid player, Objid what);
extern int	can_clone(Objid player, Objid what);

/*
 * from dbsize.c
 */
extern int	size_object(Object *o);

/*
 * from decode.c
 */
extern String 	*decode_method(Object *o, Method *m, int level);
extern String	*decompile_method(String *s, Object *o, Method *m,
		    int opt_lineno, int opt_brackets, int opt_indent,
		    int opt_indent_from, int opt_find_pc, int *pfound_lineno);
extern String	*decompile_object(Object *o);

/*
 * from dispatch.c
 */

extern Objid	 sys_obj;
extern Error	 sys_get_global(const char *name, Var *r);
extern Error	 sys_assign_global(const char *name, Var r);

/*
 * from error.c
 */

extern int		 err_name2id(const char *name);
extern const char 	*err_id2name(Error);
extern const char	*err_id2desc(Error);

/* 
 * from execute.c
 */

extern Error		 call_method(int msgid, int age, int ticks,
			 Objid player, Objid from, Objid to,
			 const char *message, List *args, Objid on);
extern String		*psub(const char *source);

/*
 * from hash.c
 */

extern int	hash(const char *s);
extern int	hash_var(Var v);
extern HashT   *hash_new(int htsize);

/* 
 * from interface.c
 */

extern void	user_send(Objid oid, String *str);
extern void	server_send(Serverid id, String *str);
extern void	run_server(void);

/*
 * from list.c
 */

extern List	*list_new(int size);
#ifdef INLINE
#define list_free(L) if (!--L->ref) { list__free(L); }
#define list_dup(L)  (L->ref++, L)
extern void	list__free(List *list);
#else
extern void	 list_free(List *list);
extern List	*list_dup(List *list);
#endif
extern List	*list_append(List *list, Var value, int pos);
extern List	*list_insert(List *list, Var value, int pos);
extern List	*list_delete(List *list, int pos);
extern List	*list_assign(List *list, Var value, int pos);
extern int	list_ismember(Var value, List *list);
extern List	*list_setadd(List *list, Var value);
extern List	*list_setremove(List *list, Var value);
extern List	*list_subset(List *list, int lower, int upper);
extern String	*list_tostring(String *st, List *l);
extern List	*explode(const char *str, const char *sep, int nwords);
extern List	*list_sort(List *list, int reverse);

/*
 * from map.c
 */

#define  map_new(size)	hash_new(size)
void	 map_free(Map *map);
Map	*map_copy(Map *map);
List	*map_keys(Map *map);
#define  map_dup(M)	((M)->ref++, (M))
int	 map_find(Map *map, Var key, Var *returnval);
Map	*map_add(Map *map, Var key, Var value);
Map	*map_remove(Map *map, Var key);
String	*map_tostring(String *st, Map *map);

/*
 * from message.c
 */

extern const char	*parse_obj(const char *s, Objid *obj);
extern const char	*parse_err(const char *s, Error *e);
extern List		*ps(void);
extern int		cmkill(int pid, Objid who);
extern void		cmkill_all(void);

/*
 * from method.c
 */

extern Method	*find_method(Object *o, const char *name);
extern Method	*find_method_recursive(Object *o, const char *name,
				       Object **where);
extern void	add_method(Object *o, Method *m);
extern Error	rm_method(Object *o, const char *name);

/*
 * from opcode.c
 */

extern Op_entry	 	*opcodes[];
extern void		 opcode_init(void);
extern void		 opcode_free_symbols(Object *o, Method *m);

/*
 * from storage.c
 */

extern void	 *cool_malloc(unsigned size);
extern void	 cool_free(void *mem);
extern const char	*check_malloc(void);
#define retrieve(OID)	(!(OID).server ? cache_get((OID).id) : 0)
#define valid(OID)	(!(OID).server ? cache_check((OID).id) : 0)
extern Object	*clone(Objid oid);
extern int	 destroy(Objid oid);
extern Object	*new_object(void);
extern void	 assign_object(Objid oid, Object *o);
extern void	 free_object(Object *o);
extern void	 free_method(Object *o, Method *m);

/*
 * from strings.c -- most of this is in string.h, but this one
 * can't go there or it'll break the netio layer
 */

extern String	*string_catobj(String *str, Objid obj, int full);
extern List	*string_list( String *str );

/*
 * from symbol.c
 */

extern String	*system_table[];
#define sym_get(O, N)	((N) < 0 ? string_dup(system_table[-(N) - 1]) \
				   : (O)->symbols[N].s)
#define sym_sys(N)	(string_dup(system_table[N]))
extern void	 sym_init(Object *o);
extern void	 sym_init_sys(void);
extern void	 sym_free(Object *o, int symno);
extern int	 sym_add(Object *o, String *);
extern List	*zero, *one, *empty_list;

/*
 * from utils.c
 */

extern int	 hasparent(Object *o, Objid pid);
extern Error	 check_parents(Objid player, Object *o, List *newparents);
extern void	 ex_error(const char *s);
extern char	*strjoin(const char *s1, const char *s2);
extern char	*str_dup(const char *s);
extern char	*strndup(const char *s, int n);
extern int	 match(const char *template, const char *tomatch, char marker);
extern int	 match_full(const char *template, const char *tomatch,
		 	    char marker);
extern int	 str_in(const char *s1, const char *s2);
extern String	*strsub(const char *source, const char *what, const char *with,
			int case_matters);
extern int	 verb_match(register const char *verb,
			     register const char *word);
extern int	 prep_match(const char *preplist, const char *argstr,
		     const char **dobj, const char **prep, const char **iobj,
		     int *dobjlen, int *preplen);
extern int	 valid_ident(const char *s);
extern const char	*addr_htoa(unsigned long l);
extern int	 count_words(const char *s, const char *sep);
extern int	 strindex(const char *source, const char *what,
			  int case_counts);
extern int	 strrindex(const char *source, const char *what,
			   int case_counts);


/*
 * from var.c
 */

#ifdef INLINE
#define var_dup(V) ( (V).type == STR  ? (V).v.str->ref++, (V) \
		   : (V).type == LIST ? (V).v.list->ref++, (V) \
		   : (V) )
#define var_free(V) { if ((V).type == STR) { string_free((V).v.str); } \
		      else if ((V).type == LIST) { list_free((V).v.list); } }
#else
extern Var	var_dup(Var  v);
extern void	var_free(Var  v);
#endif
extern int	var_compare(Var  v1, Var  v2);
extern int	var_eq(Var  v1, Var  v2);
extern Vardef  *var_find(Vardef *top, int name, int *varno);
extern Vardef  *var_find_local(Method *m, int varno);
extern Vardef  *var_find_local_by_name(Object *o, Method *m, const char *name,
				 int namelen, int *varno);
extern void	var_get_local(Var *vars, int name, Var *r);
extern Error	var_get_global(Object *o, const char *name, Var *r);
extern void	var_assign_local(Var *vars, int varno, Var value);
extern Error	var_assign_global(Object *o, String *name, Var value);
extern String  *var_tostring(String *st, Var v, int quotes);
extern int	var_add_local(Method *m, int name, int *varno);
extern int	var_add_global(Object *o, int name, Var init_value);
extern Error	var_rm_global(Object *o, const char *name);
extern int	var_count_local(Method *m);
extern Var	var_init(int type);
extern void	var_init_local(Object *on, Method *m, Var *vars);
extern void	var_copy_vars(Object *source, Object *dest);

/*
 * from verb.c
 */

extern void	verb_add(Object *o, int verb, int prep, int method);
extern int	verb_rm(Object *o, const char *verb);

/*
 * from servers.c
 */

extern List	*server_list;

#endif /* !PROTO_H */
