/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "netio.h"
#include "execute.h"

#ifdef PROTO
time_t	time(time_t *t);
#endif

int		 promiscuous = 0;
int		 corefile = 0;
int		 compiler = 0;
int		 registration = 0;
int		 verify_servers = 0;
int		 sleep_to_refresh = 1;

Objid		 sys_obj;		/* shortcut, d00d */

void
panic(const char *s)
{
    writelog();
    fprintf(stderr, "PANIC: %s\n", s);
    shutdown_server();
}

/*
 * init()
 *
 * Initializes the database and dumpfile, loads and connects to
 * remote servers (if possible).  Also calls SYSOBJ.boot_server() in
 * the newly-loaded database.
 */

int
init(const char *dbfilename, int send_boot, int db_must_exist)
{
    char	 buf[MAX_PATH_LEN];

      /* sys_obj is a global to save shoving SYS_OBJ in a local var each time */
    sys_obj.server = 0;
    sys_obj.id = SYS_OBJ;

      /* read config file */
    sprintf(buf, "%s.cfg", dbfilename);
    writelog();
    fprintf(stderr, "Reading config file from %s\n", buf);
    if (read_config(buf)) {
	writelog();
	fprintf(stderr, "Couldn't read config file\n");
	return -2;
    } /* if */

      /* initialize db and cache */
    writelog();
    fprintf(stderr, "Initializing db\n");
    dddb_setfile(dbfilename);
    if (dddb_init(db_must_exist)) {
	return -4;
    }
    writelog();
    fprintf(stderr, "Initializing cache\n");
    if (cache_init()) {
	return -5;
    }

      /* initialize system symbol table */
    sym_init_sys();

      /* initialize random number generator */
    cool_srandom((int) time((time_t *) 0));

      /* initialize opcode table */
    opcode_init();

      /* call SYS_OBJ.boot_server() */
    if (send_boot) {
	send_message(-1, 0, 0, sys_obj, sys_obj, sys_obj,
		sym_sys(BOOT_SERVER), list_dup(empty_list), 0, sys_obj);
    }
    return 0;
}

void
shutdown_server(void)
{
    writelog();
    fprintf(stderr, "Killing threads\n");
    cmkill_all();
    writelog();
    fprintf(stderr, "Syncing cache\n");
    cache_sync();
    writelog();
    fprintf(stderr, "Closing database\n");
    dddb_close();
}

Error
sys_get_global(const char *var, Var *r)
{
    Object	*sys;

    if (!(sys = retrieve(sys_obj))) {
	writelog();
	fprintf(stderr, "SYS_OBJ:  #%d not found\n", SYS_OBJ);
	return E_OBJNF;
    } else if (var_get_global(sys, var, r) == E_VARNF) {
	writelog();
	fprintf(stderr, "SYS_OBJ:  #%d.%s missing\n", SYS_OBJ, var);
	return E_VARNF;
    } else {
	return E_NONE;
    }
}

Error
sys_assign_global(const char *var, Var value)
{
    Object	*sys;
    String	*name = string_cpy(var);
    Error	 r;

    if (!(sys = retrieve(sys_obj))) {
	writelog();
	fprintf(stderr, "SYS_OBJ:  #%d not found\n", SYS_OBJ);
	r = E_OBJNF;
    } else if (var_assign_global(sys, name, value) == E_TYPE) {
	writelog();
	fprintf(stderr, "SYS_OBJ:  #%d.%s assignment:  type mismatch\n",
	    SYS_OBJ, var);
	r = E_TYPE;
    } else {
	r = E_NONE;
    }
    string_free(name);
    return r;
}

void
disconnect_player(Playerid who)
{
    Objid	 player;

    player.id = who;
    player.server = 0;
    (void) send_message(-1, 0, 0, player, player, player,
		 sym_sys(SYM_DISCONNECT), list_dup(empty_list), 0, player);
}

void
connect_server(Serverid server)
{
    List	*args;

    args = list_new(1);
    args->el[0].type = OBJ;
    args->el[0].v.obj.id = 0;
    args->el[0].v.obj.server = server;
    (void) send_message(-1, 0, 0, sys_obj, sys_obj, sys_obj,
		 sym_sys(CONNECT_SERVER), args, 0, sys_obj);
}

void
disconnect_server(Serverid server)
{
    List	*args;

    args = list_new(1);
    args->el[0].type = OBJ;
    args->el[0].v.obj.id = 0;
    args->el[0].v.obj.server = server;
    (void) send_message(-1, 0, 0, sys_obj, sys_obj, sys_obj,
		  sym_sys(DISCONNECT_SERVER), args, 0, sys_obj);
}
