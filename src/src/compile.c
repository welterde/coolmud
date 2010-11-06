/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <sys/time.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "netio.h"
#include "servers.h"
#include "execute.h"

/*
 * broken-out compiler for COOL
 */

FILE	*progfile;

static void	compile_error(const char *s);
static int	compile_getc(void);
static void	compile_ungetc(int c);

void
cmdline_compile(const char *dbfile, const char *pfile, int do_init)
{
    int		 	 n;
    struct timeval	 cur_time;

    if (!strcmp(pfile, "-")) {
	progfile = stdin;
    } else {
	progfile = fopen(pfile, "r");
	if (!progfile) {
	    perror(pfile);
	    exit(1);
	}
    }

    if (init(dbfile, 0, 0)) {
	exit(2);
    }

    n = compile(0, compile_getc, compile_ungetc, compile_error, -1, 0, 0,
		1, do_init);
    if (n) {
	fprintf(stderr, "%d errors, output file not written.\n", n);
	exit(4);
    }
    gettimeofday(&cur_time, 0);
    (void) process_queues(cur_time, &cur_time);	/* to handle init() messages */
    shutdown_server();
    fclose(progfile);
}
    
static void
compile_error(const char *s)
{
    fprintf(stderr, "%s\n", s);
}

static int
compile_getc(void)
{
    return getc(progfile);
}

static void
compile_ungetc(int c)
{
    ungetc(c, progfile);
}
