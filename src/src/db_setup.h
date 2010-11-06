/* This is the setup file for the db layer. */
/* Ripped wholesale from UnterMUD, by mjr */

#ifndef DB_SETUP_H
#define DB_SETUP_H

/* forward decl malloc */
#ifdef PROTO
extern	MALL_T	malloc(unsigned size);
extern	MALL_T	realloc(unsigned size, MALL_T where);
extern  void	free(MALL_T where);

#ifdef	SIG_IS_VOID
typedef	void	(*sig_t)();
#else
typedef	int	(*sig_t)();
#endif

#endif /* PROTO */

/* this stuff must be provided by the MUD */
#if 0
extern void		free_object();
#endif
extern int		pack_object(Object *o, FILE *f);
extern Object	       *unpack_object(FILE *f);
#define objid_hash(OID, CWIDTH)		((OID) % (CWIDTH))

/* this stuff is provided by the db routines */
/* cache access/update functions (from 'cache.c') */

extern	int		cache_init(void);
extern	int		cache_sync(void);
extern	int		cache_put(Object *obj, int oid);
extern	int		cache_check(int oid);
extern	void		cache_reset(void);

/* DBM-based db routines (from 'DB/dbmchunk.c') */

extern	int		dddb_backup(char *out);
extern	int		dddb_check(int oid);
extern	int		dddb_close(void);
extern	int		dddb_del(int oid, int flg);
extern	Object		*dddb_get(int oid);
extern	int		dddb_init(int db_must_exist);
extern	int		dddb_initted(void);
extern	int		dddb_put(Object *obj, int oid);
extern	int		dddb_setbsiz(int nbsiz);
extern	int		dddb_setfile(const char *fil);
extern	int		dddb_travend(void);
extern	int		dddb_traverse(void);
extern	int		dddb_travstart(void);

#endif /* !DB_SETUP_H */
