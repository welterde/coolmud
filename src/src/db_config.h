#ifndef	INCL_CONFIG_H

/* This file is the configuration file for the db layer. */
/* Ripped wholesale from UnterMUD, by mjr */

/* Read this file ALL THE WAY THROUGH to make sure everything's set up right */

/*---------------Operating System/Environment specific stuff-----------------*/
/*
	Some clues:
The default setup should work fine right out of the box for most
BSD-like machines.

System V machines may need some tweaking and hacking.

VMS machines want NOSYSFILE_H and NOSYSTYPES_H and a few others

HPUX wants:
	#include	<unistd.h>
	#define FOPEN_BINARY_RW "rb+"
	#define REGEX
	#define regcomp regcmp
	#define DIRENT

SCO SYS V.3.2.2 wants:
	#define	FOPEN_BINARY_RW	"r+"
	#define MAXPATHLEN 300
	#define	REGEX
	#define	DIRENT
	#define	OLDDBM

*/




/* some machines don't grok <sys/file.h> and want <file.h> (like VMS) */
/*	#define	NOSYSFILE_H			*/


/* some machines don't grok <sys/types.h> and want <types.h> (like VMS) */
/*	#define	NOSYSTYPES_H			*/


/* some machines want unistd.h */
/*	#include <unistd.h>			*/


/* if your machine has malloc() return a void *, define this option */
#define	MALL_IS_VOID


/*
some machines stdio use different parameters to tell fopen to open
a file for creation and binary read/write. that is why they call it
the "standard" I/O library, I guess. for HP/UX, you need to have
this "rb+", and make sure the file already exists before use. bleah
*/
#define	FOPEN_BINARY_RW	"a+"


/* is VMS the only machine that requires this? */
/*	noshare extern char *sys_errlist[];	*/


/* VMS permits no '@' in pathnames */
/*	#define	NO_AT_IN_PATHS			*/


/* some machines (VMS) have no MAXPATHLEN	*/
/*	#define MAXPATHLEN 300			*/


/* if your signal() returns a pointer to void function define this */
#define	SIG_IS_VOID


/* if you have regcomp() and regex() instead of re_comp() and re_exec() */
/*	#define	REGEX				*/


/* if readdir() uses struct dirents instead of struct directs */
/*	#define	DIRENT				*/


/*
if your machine has no getdtablesize, but has something similar, fake it.
HP/UX uses sysconf
*/

/* VMS seems to have no unlink(2) call, and uses delete(?) */
/*	#define	unlink(path)	delete(path)	*/

/* vanilla BSD 4.3 has no vfprintf */
/*	#define NO_VFPRINTF	*/



/*-------------------Mud Server Configuration Parameters---------------------*/
/* Most of these options you shouldn't need to change. Read them anyway. */

/* do not modify this, please - IT MAY NOT HAVE TABS OR SPACES!!! */

/* default (runtime-resettable) cache parameters */
#define	CACHE_DEPTH	23
#define	CACHE_WIDTH	7

/* enable this if you want cache warnings if objects are missing */
#define	CACHE_VERBOSE 

/*
These options can all be turned on at once, if desired, since they only
affect the compilation of which database routines should go in the database
library (used by loaddb/dumpdb)
*/
/* turn this on if you have either dbm or ndbm */
#define	DB_DBMFILE
/*
turn this on if you have dbm, but no ndbm - MAY REQUIRE CHANGES to
umud/Makefile and DM/Makefile, if dbm is not in libc (it usually isn't)
*/
/*	#define	OLDDBM				*/

/* name to use for the default dbm database file */
#define	DEFAULT_DBMCHUNKFILE	"dbm_db"

/*
  ----------PICK ONE AND ONLY ONE OF THE OPTIONS FOR STORAGE-----------
this control which database routines get linked into the MUD server itself

NOTE: only the dbmchunk option is currently available. -- sfw

*/
/* #define	STORAGE_IS_HASHDIR */
#define	STORAGE_IS_DBMCHUNK
/* #define	STORAGE_IS_GDBMCHUNK */

/* these macros cause correct linkage with storage layer - don't touch */
#ifdef	STORAGE_IS_DBMCHUNK
#define	DB_INIT(flg)	dddb_init(flg)
#define	DB_CLOSE()	dddb_close()
#define	DB_SYNC()	dddb_sync()
#define	DB_GET(n)	dddb_get(n)
#define	DB_PUT(o,n)	dddb_put(o,n)
#define	DB_CHECK(s)	dddb_check(s)
#define	DB_DEL(n,f)	dddb_del(n,f)
#define	DB_BACKUP(f)	dddb_backup(f)
#define	DB_TRAVSTART()	dddb_travstart()
#define	DB_TRAVERSE(b)	dddb_traverse(b)
#define	DB_TRAVEND()	dddb_travend()
#define	CMD__DBCONFIG	cmd__dddbconfig
#endif

/* various debug flags - turn all on for a WILD time */
/*
#define	DBMCHUNK_DEBUG
#define	HASHDIR_DEBUG
#define	CRON_DEBUG
#define	VMSNET_DEBUG
#define	XACT_DEBUG
#define	XMITBSD_DEBUG
#define	CACHE_DEBUG
#define	ALLOC_DEBUG
*/

#define	INCL_CONFIG_H
#endif

