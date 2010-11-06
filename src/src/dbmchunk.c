/*
	Copyright (C) 1991, Marcus J. Ranum. All rights reserved.
*/

/* configure all options BEFORE including system stuff. */
#include	"db_config.h"

#ifdef VMS
#include        <stdio.h>
#include        <types.h>
#include        <file.h>
#include        <unixio.h>
#include        "vms_dbm.h"
#else
#include	<stdio.h>
#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/file.h>
#include	<fcntl.h>

#ifdef	OLDDBM
#include	<dbm.h>
#include	<sys/errno.h>
extern	int	errno;
#else
#include	<ndbm.h>
#endif
#endif VMS


#include	<sys/time.h>
#include	"config.h"
#include	"cool.h"
#include	"proto.h"
#include	"sys_proto.h"
#include	"netio.h"
#include	"db_setup.h"

#ifdef PROTO
int	stat(const char *path, struct stat *buf);
#endif

/* default block size to use for bitmapped chunks */
#define	DDDB_BLOCK	256


/* bitmap growth increment in BLOCKS not bytes (512 is 64 BYTES) */
#define	DDDB_BITBLOCK	512


#define	LOGICAL_BLOCK(off)	(off/bsiz)
#define	BLOCK_OFFSET(block)	(block*bsiz)
#define	BLOCKS_NEEDED(siz)	(siz % bsiz ? (siz / bsiz) + 1 : (siz / bsiz))



/*
dbm-based object storage routines. Somewhat trickier than the default
directory hash. a free space list is maintained as a bitmap of free
blocks, and an object-pointer list is maintained in a dbm database.
*/
struct	hrec	{
	off_t	off;
	int	siz;
};

static void	dddb_mark(off_t lbn, int siz, int taken);

static	const char	*dbfile = DEFAULT_DBMCHUNKFILE;
static	int	bsiz = DDDB_BLOCK;
static	int	db_initted = 0;
static	int	last_free = 0;	/* last known or suspected free block */

#ifdef	OLDDBM
static	int	dbp = 0;
#else
static	DBM	*dbp = (DBM *)0;
#endif

static	FILE	*dbf = (FILE *)0;
static	struct	hrec	hbuf;
static	int	startrav = 0;

static	char	*bitm = (char *)0;
static	int	bitblox = 0;
static	datum	dat;
static	datum	key;

static	void	growbit(int maxblok);

int
dddb_init(int db_must_exist)
{
	char	fnam[MAXPATHLEN];
	struct	stat	sbuf;

	/* now open chunk file */
	sprintf(fnam,"%s.db",dbfile);

	dbf = fopen(fnam, "r+");
	if(dbf == (FILE *)0 && !db_must_exist) {
		dbf = fopen(fnam, "w+");
	}
	if(dbf == (FILE *)0) {
		writelog();
		fprintf(stderr, "db_init:  couldn't open %s", fnam);
		perror(":  ");
		return(1);
	}



	/* open hash table */
#ifdef	OLDDBM
	if((dbp = dbminit(dbfile)) < 0) {
		int	fxp;

		/* dbm (old) is stupid about nonexistent files */
		if(errno != ENOENT) {
			writelog();
			fprintf(stderr, "db_init:  couldn't open %s", dbfile);
			perror(":  ");
			return(1);
		}

		sprintf(fnam,"%s.dir",dbfile);
		fxp = open(fnam,O_CREAT|O_EXCL|O_WRONLY,0600);
		if(fxp < 0) {
			writelog();
			fprintf(stderr, "db_init:  couldn't open %s", fnam);
			perror(":  ");
			return(1);
		}
		(void)close(fxp);

		sprintf(fnam,"%s.pag",dbfile);
		fxp = open(fnam,O_CREAT|O_EXCL|O_WRONLY,0600);
		if(fxp < 0) {
			writelog();
			fprintf(stderr, "db_init:  couldn't open %s", fnam);
			perror(":  ");
			return(1);
		}
		(void)close(fxp);

		/* one MORE try */
		if((dbp = dbminit(dbfile)) < 0) {
			writelog();
			fprintf(stderr, "db_init:  couldn't open %s", dbfile);
			perror(":  ");
			return(1);
		}
	}
#else
	if((dbp = dbm_open(dbfile,O_RDWR|O_CREAT,0600)) == (DBM *)0) {
		writelog();
		fprintf(stderr, "db_init:  couldn't open %s", dbfile);
		perror(":  ");
		return(1);
	}
#endif

	/* determine size of chunk file for allocation bitmap */
	sprintf(fnam,"%s.db",dbfile);
	if(stat(fnam,&sbuf)) {
		writelog();
		fprintf(stderr, "db_init:  cannot stat %s", fnam);
		perror("");
		return(1);
	}

	/* allocate bitmap */
	growbit(LOGICAL_BLOCK(sbuf.st_size) + DDDB_BITBLOCK);


#ifdef	OLDDBM
	key = firstkey();
#else
	key = dbm_firstkey(dbp);
#endif
	while(key.dptr != (char *)0) {
#ifdef	OLDDBM
		dat = fetch(key);
#else
		dat = dbm_fetch(dbp,key);
#endif
		if(dat.dptr == (char *)0) {
			writelog();
			fprintf(stderr, "db_init:  index inconsistent\n");
			return(1);
		}
		bcopy(dat.dptr,&hbuf,sizeof(hbuf));	/* alignment */


		/* mark it as busy in the bitmap */
		dddb_mark(LOGICAL_BLOCK(hbuf.off),hbuf.siz,1);


#ifdef	OLDDBM
		key = nextkey(key);
#else
		key = dbm_nextkey(dbp);
#endif
	}

	db_initted = 1;
	return(0);
}



int
dddb_initted(void)
{
	return(db_initted);
}


int
dddb_setbsiz(int nbsiz)
{
	return(bsiz = nbsiz);
}


int
dddb_setfile(const char *fil)
{
	char	*xp;

	if(db_initted)
		return(1);

	/* KNOWN memory leak. can't help it. it's small */
	xp = (char *)malloc((unsigned)strlen(fil) + 1);
	if(xp == (char *)0)
		return(1);
	(void)strcpy(xp,fil);
	dbfile = xp;
	return(0);
}



int
dddb_close(void)
{
	if(dbf != (FILE *)0) {
		fclose(dbf);
		dbf = (FILE *)0;
	}

#ifndef	OLDDBM
	if(dbp != (DBM *)0) {
		dbm_close(dbp);
		dbp = (DBM *)0;
	}
#endif
	if(bitm != (char *)0) {
		free((MALL_T)bitm);
		bitm = (char *)0;
		bitblox = 0;
	}
	db_initted = 0;
	return(0);
}




/* grow the bitmap to given size */
static	void
growbit(int maxblok)
{
	int	nsiz;
	char	*nbit;

	/* round up to eight and then some */
	nsiz = (maxblok + 8) + (8 - (maxblok % 8));

	if(nsiz <= bitblox)
		return;

	/* this done because some old realloc()s are busted */
	nbit = (char *)malloc((unsigned)(nsiz / 8));
	if(bitm != (char *)0) {
		bcopy(bitm,nbit,(bitblox / 8) - 1);
		free((MALL_T)bitm);
	}
	bitm = nbit;

	if(bitm == (char *)0)
		panic("db_init cannot grow bitmap");

	bzero(bitm + (bitblox / 8),((nsiz / 8) - (bitblox / 8)) - 1);
	bitblox = nsiz - 8;
}



static	void
dddb_mark(off_t lbn, int siz, int taken)
{
	int	bcnt;

	bcnt = BLOCKS_NEEDED(siz);

	/* remember a free block was here */
	if(!taken)
		last_free = lbn;

	while(bcnt--) {
		if(lbn >= bitblox - 32)
			growbit(lbn + DDDB_BITBLOCK);

		if(taken)
			bitm[lbn >> 3] |= (1 << (lbn & 7));
		else
			bitm[lbn >> 3] &= ~(1 << (lbn & 7));
		lbn++;
	}
}





static	int
dddb_alloc(int siz)
{
	int	bcnt;	/* # of blocks to operate on */
	int	lbn;	/* logical block offset */
	int	tbcnt;
	int	slbn;
	int	overthetop = 0;

	lbn = last_free;
	bcnt = siz % bsiz ? (siz / bsiz) + 1 : (siz / bsiz);

	while(1) {
		if(lbn >= bitblox - 32) {
			/* only check here. can't break around the top */
			if(!overthetop) {
				lbn = 0;
				overthetop++;
			} else {
				growbit(lbn + DDDB_BITBLOCK);
			}
		}

		slbn = lbn;
		tbcnt = bcnt;

		while(1) {
			if((bitm[lbn >> 3] & (1 << (lbn & 7))) != 0)
				break;

			/* enough free blocks - mark and done */
			if(--tbcnt == 0) {
				for(tbcnt = slbn; bcnt > 0; tbcnt++, bcnt--)
					bitm[tbcnt >> 3] |= (1 << (tbcnt & 7));

				last_free = lbn;
				return(slbn);
			}

			lbn++;
			if(lbn >= bitblox - 32)
				growbit(lbn + DDDB_BITBLOCK);
		}
		lbn++;
	}
}




Object	*
dddb_get(int oid)
{
	Object		*ret;

	if(!db_initted)
		return((Object *)0);

	key.dptr = (char *) &oid;
	key.dsize = sizeof(oid);
#ifdef	OLDDBM
	dat = fetch(key);
#else
	dat = dbm_fetch(dbp,key);
#endif

	if(dat.dptr == (char *)0)
		return((Object *)0);
	bcopy(dat.dptr,&hbuf,sizeof(hbuf));

	/* seek to location */
	if(fseek(dbf,(long)hbuf.off,0))
		return((Object *)0);

	/* if the file is badly formatted, ret == Object * 0 */
	if((ret = unpack_object(dbf)) == (Object *)0) {
		writelog();
		fprintf(stderr, "db_get: cannot decode #%d\n", oid);
	}
	return(ret);
}




int
dddb_put(Object *obj, int oid)
{
	int	nsiz;
#ifdef SFW_DEBUG
	long	oldpos, actual_size;
#endif

	if(!db_initted)
		return(1);

	nsiz = size_object(obj);

	key.dptr = (char *) &oid;
	key.dsize = sizeof(oid);

#ifdef	OLDDBM
	dat = fetch(key);
#else
	dat = dbm_fetch(dbp,key);
#endif

	if(dat.dptr != (char *)0) {

		bcopy(dat.dptr,&hbuf,sizeof(hbuf));	/* align */

		if(BLOCKS_NEEDED(nsiz) > BLOCKS_NEEDED(hbuf.siz)) {

#ifdef	DBMCHUNK_DEBUG
printf("put: #%d old %d < %d - free %d\n",oid,hbuf.siz,nsiz,hbuf.off);
#endif
			/* mark free in bitmap */
			dddb_mark(LOGICAL_BLOCK(hbuf.off),hbuf.siz,0);

			hbuf.off = BLOCK_OFFSET(dddb_alloc(nsiz));
			hbuf.siz = nsiz;
#ifdef	DBMCHUNK_DEBUG
printf("put: #%d moved to offset %d, size %d\n",oid,hbuf.off,hbuf.siz);
#endif
		} else {
			hbuf.siz = nsiz;
#ifdef	DBMCHUNK_DEBUG
printf("put: #%d replaced within offset %d, size %d\n",oid,hbuf.off,hbuf.siz);
#endif
		}
	} else {
		hbuf.off = BLOCK_OFFSET(dddb_alloc(nsiz));
		hbuf.siz = nsiz;
#ifdef	DBMCHUNK_DEBUG
printf("put: #%d (new) at offset %d, size %d\n",oid,hbuf.off,hbuf.siz);
#endif
	}


	/* make table entry */
	dat.dptr = (char *)&hbuf;
	dat.dsize = sizeof(hbuf);

#ifdef	OLDDBM
	if(store(key,dat)) {
		writelog();
		fprintf(stderr, "db_put: can't store #%d\n", oid);
		return(1);
	}
#else
	if(dbm_store(dbp,key,dat,DBM_REPLACE)) {
		writelog();
		fprintf(stderr, "db_put: can't dbm_store #%d\n", oid);
		return(1);
	}
#endif

#ifdef	DBMCHUNK_DEBUG
printf("#%d offset %d size %d\n",oid,hbuf.off,hbuf.siz);
#endif
	if(fseek(dbf,(long)hbuf.off,0)) {
		writelog();
		fprintf(stderr, "db_put: can't seek #%d", oid);
		perror("");
		return(1);
	}
#ifdef SFW_DEBUG
	oldpos = ftell(dbf);
#endif
	if(pack_object(obj,dbf) != 0 || fflush(dbf) != 0) {
		writelog();
		fprintf(stderr, "db_put: can't save #%d\n", oid);
		return(1);
	}
#ifdef SFW_DEBUG
	actual_size = ftell(dbf) - oldpos;
	if (actual_size != hbuf.siz) {
	    writelog();
	    fprintf(stderr, "dbput #%d:  size %d, actually written %ld\n", oid,
			    hbuf.siz, actual_size);
	} /* if */
#endif
	return(0);
}




int
dddb_check(int oid)
{

	if(!db_initted)
		return(0);

	key.dptr = (char *) &oid;
	key.dsize = sizeof(oid);
#ifdef	OLDDBM
	dat = fetch(key);
#else
	dat = dbm_fetch(dbp,key);
#endif

	if(dat.dptr == (char *)0)
		return(0);
	return(1);
}




int
dddb_del(int oid, int flg)
{

	if(!db_initted)
		return(-1);

	key.dptr = (char *) &oid;
	key.dsize = sizeof(oid);
#ifdef	OLDDBM
	dat = fetch(key);
#else
	dat = dbm_fetch(dbp,key);
#endif


	/* not there? */
	if(dat.dptr == (char *)0)
		return(0);
	bcopy(dat.dptr,&hbuf,sizeof(hbuf));

	/* drop key from db */
#ifdef	OLDDBM
	if(delete(key)) {
#else
	if(dbm_delete(dbp,key)) {
#endif
		writelog();
		fprintf(stderr, "db_del: can't delete key #%d\n", oid);
		return(1);
	}

	/* mark free space in bitmap */
	dddb_mark(LOGICAL_BLOCK(hbuf.off),hbuf.siz,0);
#ifdef	DBMCHUNK_DEBUG
printf("del: #%d free offset %d, size %d\n",oid,hbuf.off,hbuf.siz);
#endif

	/* mark object dead in file */
	if(fseek(dbf,(long)hbuf.off,0)) {
		writelog();
		fprintf(stderr, "db_del: can't seek #%d", oid);
		perror("");
	} else {
		fprintf(dbf,"delobj");
		fflush(dbf);
	}

	return(0);
}



int
dddb_travstart(void)
{
	startrav = 0;
	return(0);
}



int
dddb_traverse(void)
{
	if(!db_initted)
		return(-1);

	if(!startrav) {
#ifdef	OLDDBM
		key = firstkey();
#else
		key = dbm_firstkey(dbp);
#endif
		startrav = 1;
	} else
#ifdef	OLDDBM
		key = nextkey(key);
#else
		key = dbm_nextkey(dbp);
#endif
	if(key.dptr == (char *)0)
		return(-1);
	return *((int *) key.dptr);
}



int
dddb_travend(void)
{
	startrav = 0;
	return(0);
}





int
dddb_backup(char *out)
{
	FILE	*ou;
	char	vuf[BUFSIZ];
	int	tor;

	if(!db_initted)
		return(1);

	if((ou = fopen(out,"w")) == (FILE *)0) {
		writelog();
		fprintf(stderr, "db_backup:  couldn't open %s", out);
		perror("");
		return(1);
	}


#ifdef	OLDDBM
	key = firstkey();
#else
	key = dbm_firstkey(dbp);
#endif
	while(key.dptr != (char *)0) {
#ifdef	OLDDBM
		dat = fetch(key);
#else
		dat = dbm_fetch(dbp,key);
#endif
		if(dat.dptr == (char *)0) {
			writelog();
			fprintf(stderr, "db_init:  index inconsistent\n");
			continue;
		}

		bcopy(dat.dptr,&hbuf,sizeof(hbuf));

		if(fseek(dbf,(long)hbuf.off,0)) {
			writelog();
			fprintf(stderr, "db_backup: can't seek #%d\n",
			    *((int *) key.dptr));
			perror("");
		} else {

			/* copy out the object, sans interpretation */
			while(hbuf.siz > 0) {
				if((tor = hbuf.siz) > sizeof(vuf))
					tor = sizeof(vuf);
				if(fread(vuf,sizeof(char),tor,dbf) <= 0) {
					writelog();
					fprintf(stderr, "db_backup:  can't read #%d", *((int *) key.dptr));
					break;
				}

				/* what are we gonna do if it fails, anyhow? */
				fwrite(vuf,sizeof(char),tor,ou);
				hbuf.siz -= tor;
			}
		}

#ifdef	OLDDBM
		key = nextkey(key);
#else
		key = dbm_nextkey(dbp);
#endif
	}

	fclose(ou);
	return(0);
}

