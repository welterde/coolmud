/*
	Copyright (C) 1991, Marcus J. Ranum. All rights reserved.
*/

/* configure all options BEFORE including system stuff. */
#include	"db_config.h"

#include	<stdio.h>
#ifdef	NOSYSTYPES_H
#include	<types.h>
#else
#include	<sys/types.h>
#endif

#include	<sys/time.h>
#include	"config.h"
#include	"cool.h"
#include	"proto.h"
#include	"sys_proto.h"
#include	"netio.h"
#include	"db_setup.h"

#ifdef PROTO
extern time_t		 time(time_t *tloc);
#endif


/*
This is by far the most complex and kinky code in UnterMUD. You should
never need to mess with anything in here - if you value your sanity.
*/


typedef	struct	cache	{
	int	oid;
	Object	*op;
	int	flg;
	struct	cache	*nxt;
	struct	cache	*prv;
} Cache;


/* flag states */
#define	C_NOFLG	00
#define	C_DIRTY	01
#define	C_DEAD	02


typedef	struct	{
	Cache	*ahead;
	Cache	*ohead;
	Cache	*atail;
	Cache	*otail;
} CacheLst;

/* initial settings for cache sizes */
static	int	cwidth = CACHE_WIDTH;
static	int	cdepth = CACHE_DEPTH;


/* ntbfw - main cache pointer and list of things to kill off */
static	CacheLst	*sys_c;

static	int	cache_initted = 0;

/* cache stats gathering stuff. you don't like it? comment it out */
static	time_t	cs_ltime;
static	int	cs_writes = 0;	/* total writes */
static	int	cs_reads = 0;	/* total reads */
static	int	cs_dbreads = 0;	/* total read-throughs */
static	int	cs_dbwrites = 0;	/* total write-throughs */
static	int	cs_dels = 0;		/* total deletes */
static	int	cs_checks = 0;	/* total checks */
static	int	cs_rhits = 0;	/* total reads filled from cache */
static	int	cs_ahits = 0;	/* total reads filled active cache */
static	int	cs_whits = 0;	/* total writes to dirty cache */
static	int	cs_fails = 0;	/* attempts to grab nonexistent */
static	int	cs_resets = 0;	/* total cache resets */
static	int	cs_syncs = 0;	/* total cache syncs */
static	int	cs_objects = 0;	/* total cache size */



int
cache_init(void)
{
	int	x;
	int	y;
	Cache	*np;
	static const char	*ncmsg = "cache_init: cannot allocate cache";

	if(cache_initted || sys_c != (CacheLst *)0)
		return(0);

	sys_c = (CacheLst *)malloc((unsigned)cwidth * sizeof(CacheLst));
	if(sys_c == (CacheLst *)0) {
		writelog();
		perror(ncmsg);
		return(-1);
	}

	for(x = 0; x < cwidth; x++) {
		sys_c[x].ahead = sys_c[x].ohead = (Cache *)0;
		sys_c[x].atail = sys_c[x].otail = (Cache *)0;

		for(y = 0; y < cdepth; y++) {

			np = (Cache *)malloc(sizeof(Cache));
			if(np == (Cache *)0) {
				writelog();
				perror(ncmsg);
				return(-1);
			}

			if((np->nxt = sys_c[x].ohead) != (Cache *)0)
				np->nxt->prv = np;
			else
				sys_c[x].otail = np;

			np->prv = (Cache *)0;
			np->flg = C_NOFLG;
			np->oid = -1;
			np->op = (Object *)0;

			sys_c[x].ohead = np;
			cs_objects++;
		}
	}

	/* mark caching system live */
	cache_initted++;

	time(&cs_ltime);

	return(0);
}





void
cache_reset(void)
{
	int	x;

	if(!cache_initted)
		return;

	/* unchain and rechain each active list at head of old chain */
	for(x = 0; x < cwidth; x++) {
		if(sys_c[x].ahead != (Cache *)0) {
			sys_c[x].atail->nxt = sys_c[x].ohead;

			if(sys_c[x].ohead != (Cache *)0)
				sys_c[x].ohead->prv = sys_c[x].atail;
			if (sys_c[x].otail == (Cache *)0)
				sys_c[x].otail = sys_c[x].atail;

			sys_c[x].ohead = sys_c[x].ahead;
			sys_c[x].ahead = (Cache *)0;
		}
	}
	cs_resets++;
}




/*
search the cache for an object, and if it is not found, thaw it.
this code is probably a little bigger than it needs be because I
had fun and unrolled all the pointer juggling inline.
*/
Object	*
cache_get(int oid)
{
	Cache		*cp;
	int		hv = 0;
	Object		*ret;
	static const char	*nmmesg = "cache_get:  cannot allocate memory";

	/* firewall */
	if(!cache_initted) {
#ifdef	CACHE_VERBOSE
		writelog();
		fprintf(stderr, "cache_get:  cache not initted\n");
#endif
		return((Object *)0);
	}

#ifdef	CACHE_DEBUG
	printf("get #%d\n",oid);
#endif

	if (oid < 0) 
	    return((Object *)0);
	cs_reads++;

	hv = objid_hash(oid,cwidth);

	/* search active chain first */
	for(cp = sys_c[hv].ahead; cp != (Cache *)0; cp = cp->nxt) {
		if(!(cp->flg & C_DEAD) && cp->oid == oid) {
			cs_rhits++;
			cs_ahits++;
#ifdef	CACHE_DEBUG
			printf("return #%d active cache %d\n",cp->oid,cp->op);
#endif
			return(cp->op);
		}
	}

	/* search in-active chain second */
	for(cp = sys_c[hv].ohead; cp != (Cache *)0; cp = cp->nxt) {
		if(!(cp->flg & C_DEAD) && cp->oid == oid) {

			/* dechain from in-active chain */
			if(cp->nxt == (Cache *)0)
				sys_c[hv].otail = cp->prv;
			else
				cp->nxt->prv = cp->prv;
			if(cp->prv == (Cache *)0)
				sys_c[hv].ohead = cp->nxt;
			else
				cp->prv->nxt = cp->nxt;

			/* insert at head of active chain */
			cp->nxt = sys_c[hv].ahead;
			if(sys_c[hv].ahead == (Cache *)0)
				sys_c[hv].atail = cp;
			cp->prv = (Cache *)0;
			if(cp->nxt != (Cache *)0)
				cp->nxt->prv = cp;
			sys_c[hv].ahead = cp;

			/* done */
			cs_rhits++;
#ifdef	CACHE_DEBUG
			printf("return #%d old cache %d\n",cp->oid,cp->op);
#endif
			return(cp->op);
		}
	}

	/* DARN IT - at this point we have a certified, type-A cache miss */

	/* thaw the object from wherever. */
	if((ret = DB_GET(oid)) == (Object *)0) {
		cs_fails++;
#ifdef	CACHE_DEBUG
		printf("#%d not in db\n",oid);
#endif
		return((Object *)0);
	}
	cs_dbreads++;

	/* if there are no old cache object holders left, allocate one */
	if(sys_c[hv].otail == (Cache *)0) {

		if((cp = (Cache *)malloc(sizeof(Cache))) == (Cache *)0)
			panic(nmmesg);
		cp->oid = oid;

		cp->flg = C_NOFLG;

		/* linkit at head of active chain */
		cp->nxt = sys_c[hv].ahead;
		if(sys_c[hv].ahead == (Cache *)0)
			sys_c[hv].atail = cp;
		cp->prv = (Cache *)0;
		if(cp->nxt != (Cache *)0)
			cp->nxt->prv = cp;
		sys_c[hv].ahead = cp;
		cs_objects++;
#ifdef	CACHE_DEBUG
		printf("return #%d loaded into cache %d\n",cp->oid,cp->op);
#endif
		return(cp->op = ret);
	}

	/* unlink old cache chain tail */
	cp = sys_c[hv].otail;
	if(cp->prv != (Cache *)0) {
		sys_c[hv].otail = cp->prv;
		cp->prv->nxt = cp->nxt;
	} else	/* took last one */
		sys_c[hv].ohead = sys_c[hv].otail = (Cache *)0;

	/* if there is a dirty object still in memory, write it */
	if((cp->flg & C_DIRTY) && cp->oid >= 0 && cp->op != (Object *)0) {
#ifdef	CACHE_DEBUG
		printf("clean #%d from cache %d\n",cp->oid,cp->op);
#endif
		if(DB_PUT(cp->op,cp->oid))
			return((Object *)0);
		cs_dbwrites++;
	}

	/* free object's data */
	if(cp->op != (Object *)0)
		free_object(cp->op);
	cp->op = ret;
	cp->oid = oid;
	cp->flg = C_NOFLG;

	/* relink at head of active chain */
	cp->nxt = sys_c[hv].ahead;
	if(sys_c[hv].ahead == (Cache *)0)
		sys_c[hv].atail = cp;
	cp->prv = (Cache *)0;
	if(cp->nxt != (Cache *)0)
		cp->nxt->prv = cp;
	sys_c[hv].ahead = cp;

#ifdef	CACHE_DEBUG
	printf("return #%d loaded into cache %d\n",cp->oid,cp->op);
#endif
	return(ret);
}




/*
put an object back into the cache. this is complicated by the
fact that when a function calls this with an object, the object
is *already* in the cache, since calling functions operate on
pointers to the cached objects, and may or may not be actively
modifying them. in other words, by the time the object is handed
to cache_put, it has probably already been modified, and the cached
version probably already reflects those modifications!

so - we do a couple of things: we make sure that the cached
object is actually there, and set its dirty bit. if we can't
find it - either we have a (major) programming error, or the
*name* of the object has been changed, or the object is a totally
new creation someone made and is inserting into the world.

in the case of totally new creations, we simply accept the pointer
to the object and add it into our environment. freeing it becomes
the responsibility of the cache code. DO NOT HAND A POINTER TO
CACHE_PUT AND THEN FREE IT YOURSELF!!!!

There are other sticky issues about changing the object pointers
of MUDs and their names. This is left as an exercise for the
reader.
*/
int
cache_put(Object *obj, int oid)
{
	Cache	*cp;
	int	hv = 0;
	static const char	*nmmesg = "cache_put: cannot allocate memory ";

	/* firewall */
	if(obj == (Object *)0 || oid < 0 || !cache_initted) {
#ifdef	CACHE_VERBOSE
		writelog();
		fprintf(stderr, "cache_put:  NULL object/id - programmer error\n");
#endif
		return(1);
	}

	cs_writes++;

	/* generate hash */
	hv = objid_hash(oid,cwidth);

	/* step one, search active chain, and if we find the obj, dirty it */
	for(cp = sys_c[hv].ahead; cp != (Cache *)0; cp = cp->nxt) {
		if(cp->oid == oid) {

			/* already dirty? */
			if(cp->flg & C_DIRTY)
				cs_whits++;

			if (cp->flg & C_DEAD) {
			    fprintf(stderr, "found dead on active chain, 0x%08lx 0x%08lx\n", (long) cp->op, (long) obj);
			}
			/* OWIE! - bait & switch */
			if(obj != cp->op) {
#ifdef	CACHE_DEBUG
				printf("cache_put bait&switch #%d 0x%08lx\n",cp->oid,(long) cp->op);
#endif
				if(cp->op != (Object *)0)
					free_object(cp->op);
				cp->op = obj;
			}

#ifdef	CACHE_DEBUG
			printf("cache_put #%d %d\n",cp->oid,cp->op);
#endif
			cp->flg |= C_DIRTY;
			return(0);
		}
	}

	/* step two, search in-active chain */
	for(cp = sys_c[hv].ohead; cp != (Cache *)0; cp = cp->nxt) {
		if(cp->oid == oid) {

			/* already dirty? */
			if(cp->flg & C_DIRTY)
				cs_whits++;

			/* OWIE! - bait & switch */
			if(obj != cp->op) {
#ifdef	CACHE_DEBUG
				printf("cache_put bait&switch #%d %d\n",cp->oid,cp->op);
#endif
				if(cp->op != (Object *)0)
					free_object(cp->op);
				cp->op = obj;
			}

#ifdef	CACHE_DEBUG
			printf("cache_put #%d %d\n",cp->oid,cp->op);
#endif
			cp->flg |= C_DIRTY;
			return(0);
		}
	}

	/* a totally new object. make a slot in the cache for it. */
	if((cp = sys_c[hv].otail) == (Cache *)0) {

		/* no room on the chain!! chain must grow! */
		if((cp = (Cache *)malloc(sizeof(Cache))) == (Cache *)0)
			panic(nmmesg);
		cp->oid = -1;
		cp->op = (Object *)0;
		cs_objects++;
	} else {

		/* there is a dirty object still in memory? write. */
		if((cp->flg & C_DIRTY) && cp->oid >= 0 && cp->op != (Object *)0) {

#ifdef	CACHE_DEBUG
			printf("cache_put #%d clean-out %d\n",cp->oid,cp->op);
#endif
			/* if this fails we are a hurting puppy. */
			if(DB_PUT(cp->op,cp->oid))
				return(1);
			cs_dbwrites++;
		}

		/* unlink from inactive chain */
		if(cp->prv != (Cache *)0) {
			sys_c[hv].otail = cp->prv;
			cp->prv->nxt = cp->nxt;
		} else {
			sys_c[hv].ohead = sys_c[hv].otail = (Cache *)0;
		}
	}


	/* point it at the new object */
	if(cp->op != (Object *)0)
		free_object(cp->op);
	cp->op = obj;

	cp->oid = oid;

	/* mark new dodad as dirty */
	cp->flg = C_NOFLG | C_DIRTY;


	/* link at head of active chain */
	cp->nxt = sys_c[hv].ahead;
	if(sys_c[hv].ahead == (Cache *)0)
		sys_c[hv].atail = cp;
	cp->prv = (Cache *)0;
	if(cp->nxt != (Cache *)0)
		cp->nxt->prv = cp;
	sys_c[hv].ahead = cp;


#ifdef	CACHE_DEBUG
	printf("cache_put #%d new in cache %d\n",cp->oid,cp->op);
#endif

	/* e finito ! */
	return(0);
}




int
cache_sync(void)
{
	int	x;
	Cache	*cp;

	cs_syncs++;

	if(!cache_initted)
		return(1);

	for(x = 0; x < cwidth; x++) {
		for(cp = sys_c[x].ahead; cp != (Cache *)0; cp = cp->nxt) {
			if(cp->flg & C_DIRTY) {
#ifdef	CACHE_DEBUG
				printf("sync #%d\n",cp->oid);
#endif
				if(DB_PUT(cp->op,cp->oid))
					return(1);
				cs_dbwrites++;
				cp->flg &= ~C_DIRTY;
			}
		}
		for(cp = sys_c[x].ohead; cp != (Cache *)0; cp = cp->nxt) {
			if(cp->flg & C_DIRTY) {
#ifdef	CACHE_DEBUG
				printf("sync #%d\n",cp->oid);
#endif
				if(DB_PUT(cp->op,cp->oid))
					return(1);
				cs_dbwrites++;
				cp->flg &= ~C_DIRTY;
			}
		}
	}
	return(0);
}




/*
probe the cache and the database (if needed) for the existence of an
object. return nonzero if the object is in cache or database
*/
int
cache_check(int oid)
{
	Cache	*cp;
	int	hv = 0;

	if(oid < 0 || !cache_initted)
		return(0);

	cs_checks++;

	hv = objid_hash(oid,cwidth);

	for(cp = sys_c[hv].ahead; cp != (Cache *)0; cp = cp->nxt)
		if(cp->oid >= 0 && !(cp->flg & C_DEAD) && cp->oid == oid)
			return(1);

	for(cp = sys_c[hv].ohead; cp != (Cache *)0; cp = cp->nxt)
		if(cp->oid >= 0 && !(cp->flg & C_DEAD) && cp->oid == oid)
			return(1);

	/* no ? */
	return(DB_CHECK(oid));
}




/*
 * delete something from the cache/db
 */
int
cache_del(int oid, int flg)
{
	Cache	*cp;
	int	hv = 0;

	if(oid < 0 || !cache_initted)
		return(0);

	cs_dels++;

	hv = objid_hash(oid,cwidth);

	/* mark dead in cache */
	for(cp = sys_c[hv].ahead; cp != (Cache *)0; cp = cp->nxt) {
		if(cp->oid == oid) {
#ifdef	CACHE_DEBUG
			fprintf(stderr, "killed active #%d at 0x%08lx\n", cp->oid, (long) cp->op);
#endif
			cp->flg = C_DEAD;
			break;
		}
	}

	for(cp = sys_c[hv].ohead; cp != (Cache *)0; cp = cp->nxt) {
		if(cp->oid == oid) {
#ifdef	CACHE_DEBUG
			fprintf(stderr, "killed inactive #%d at 0x%08lx\n", cp->oid, (long) cp->op);
#endif
			cp->flg = C_DEAD;
			break;
		}
	}

	return(DB_DEL(oid,flg));
}


int
cacheconfig(const char *line, int lineno)
{
	/* configure cache depth */
    if(!strncmp(line, "depth ", 6)) {
	if(cache_initted) {
	    writelog();
	    fprintf(stderr, "Line %d:  cache depth already fixed.\n", lineno);
	    return(1);
	}
	if((cdepth = atoi(line + 6)) <= 0) {
	    writelog();
	    fprintf(stderr, "Line %d:  Bad cache depth value.\n", lineno);
	    return(1);
	}
	writelog();
	fprintf(stderr, "Cache depth starts at %d slots.\n", cdepth);
	return(0);
    }

    /* configure cache width */
    if(!strncmp(line, "width ", 6)) {
	if(cache_initted) {
	    writelog();
	    fprintf(stderr, "Line %d:  Cache width already fixed.\n", lineno);
	    return(1);
	}
	if((cwidth = atoi(line + 6)) <= 0) {
	    writelog();
	    fprintf(stderr, "Line %d:  Bad cache width value.\n", lineno);
	    return(1);
	}
	writelog();
	fprintf(stderr, "Cache width is %d slots.\n",cwidth);
	return(0);
    }
    return(0);
}

const char *
cache_stats(void)
{
    static char	bigbuf[1024];
    char	vuf[128];

    bigbuf[0] = '\0';

    sprintf(vuf,"cache stats, last %ld sec.:\n", time(0) - cs_ltime);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  writes: %-5d       reads: %-5d\n",cs_writes,cs_reads);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  dbwrites: %-5d     dbreads: %-5d\n",cs_dbwrites,cs_dbreads);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  read hits: %-5d    active hits: %-5d\n",cs_rhits,cs_ahits);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  write hits to dirty cache: %-5d\n",cs_whits);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  deletes: %-5d      checks: %-5d\n",cs_dels,cs_checks);
    strcat(bigbuf, vuf);

    sprintf(vuf,"  resets: %-5d       syncs: %-5d     objects: %-5d\n",
	    cs_resets,cs_syncs,cs_objects);
    strcat(bigbuf, vuf);

    return bigbuf;
}
#if 0
		if(argc > 2 && !strcmp(argv[2],"clear")) {
			time(&cs_ltime);
			cs_writes = 0;
			cs_reads = 0;
			cs_dbreads = 0;
			cs_dbwrites = 0;
			cs_rhits = 0;
			cs_ahits = 0;
			cs_whits = 0;
			cs_fails = 0;
			cs_dels = 0;
			cs_checks = 0;
			cs_resets = 0;
			cs_syncs = 0;
		}
		return(0);
	}

	if(!strcmp(argv[1],"help")) {
		say(who,argv[0]," depth number-of-slots\n",(char *)0);
		say(who,argv[0]," width number-of-slots\n",(char *)0);
		say(who,argv[0]," sync\n",(char *)0);
		say(who,argv[0]," stats [clear]\n",(char *)0);
		return(0);
	}

	writelog();
	fprintf(stderr, "I don't understand %s\n",argv[1]);
	return(0);
#endif /* 0 */
