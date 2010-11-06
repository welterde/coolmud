/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "socket_proto.h"
#include "buf.h"
#include "netio.h"
#include "servers.h"
#include "servers_private.h"

#ifdef PROTO
extern struct hostent	*gethostbyname(const char *name);
extern struct hostent	*gethostbyaddr(void *addr, int len, int type);
extern time_t		 time(time_t *t);
extern char		*ctime(time_t *t);
#endif /* PROTO */

Server	*servers = 0;
List	*server_list;

static int	 serverconfig(const char *line, int lineno);
static void	 serv_addtolist(Serverid id);
static void	 serv_addtocfg(Server *new);
static char	 configfilename[MAX_PATH_LEN];

static void
serv_addtocfg(Server *new)
{
    FILE	*f;

    if (!(f = fopen(configfilename, "a"))) {
	writelog();
	perror(configfilename);
    } else {
	fprintf(f, "server %s %s %d\n", new->name, new->hostname, new->port);
	fclose(f);
    }
}

#define MATCHES(A, B) (!cool_strncasecmp(A, B, strlen(B)))

int
read_config(const char *filename)
{
    char		 line[512];
    int			 lineno = 0;
    FILE		*configfile;
    int			 ok;

    strcpy(configfilename, filename);
    if (!(configfile = fopen(filename, "r"))) {
	writelog();
	perror(filename);
	return -1;
    }
    servers = 0;
    server_list = list_new(0);
    while (!feof(configfile)) {			/* while there's more input */
	if (!fgets(line, sizeof(line), configfile)) {	/* get a line */
	    break;
	}
	lineno++;	ok = 1;
	switch (line[0]) {
	  case '#':  case '\n':  case '\0':
	  /* if it starts with '#', or is blank, skip it */
	    break;
	  case 'c':	 /* cache, corefile */
	    if (MATCHES(line, "cache ")) {
		if (cacheconfig(line + 6, lineno)) {
		    fclose(configfile);
		    return -3;
		}
	    } else if (MATCHES(line, "corefile")) {
		corefile = 1;
	    } else {
		ok = 0;
	    }
	    break;
	  case 's':	 /* server */
	    if (MATCHES(line, "server ")) {
	        if (serverconfig(line + 7, lineno)) {
		    fclose(configfile);
		    return -2;
		}
	    } else if (MATCHES(line, "sleep_to_refresh ")) {
		sleep_to_refresh = atoi(line + 17);
	    } else {
		ok = 0;
	    }
	    break;
	  case 'm':	/* max_age, max_ticks */
	    if (MATCHES(line, "max_age ")) {
		max_age = atoi(line + 8);
	    } else if (MATCHES(line, "max_ticks ")) {
		max_ticks = atoi(line + 10);
	    } else {
		ok = 0;
	    }
	    break;
	  case 'p':	/* player_port, promiscuous */
	    if (MATCHES(line, "player_port ")) {
		player_port = atoi(line + 12);
	    } else if (MATCHES(line, "promiscuous")) {
		promiscuous = 1;
	    } else {
		ok = 0;
	    }
	    break;
	  case 'r':	/* registration */
	    if (MATCHES(line, "registration")) {
		registration = 1;
	    } else {
		ok = 0;
	    }
	    break;
	  case 'v':	/* verify_servers */
	    if (MATCHES(line, "verify_servers")) {
		verify_servers = 1;
		continue;
	    } else {
		ok = 0;
	    }
	    break;
	  default:
	    ok = 0;
	}
	  /* unknown config command */
	if (!ok) {
	    writelog();
	    fprintf(stderr, "Couldn't read config file, line %d\n", lineno);
	    fclose(configfile);
	    return -4;
	}
    }
    if (!servers) {
	writelog();
	fprintf(stderr, "Must be at least one servers entry in config file\n");
	fclose(configfile);
	return -5;
    }
    yo_port = servers->port;
    fclose(configfile);
    return 0;
}

static int
serverconfig(const char *line, int lineno)
{
    Server		*s;
    struct hostent	*h;
    static Serverid	 sid = 0;
    static Server	*prev = 0;
    char		 name[21], hostname[41];
    short		 port;

    if (sscanf(line, "%20s %40s %hd", name, hostname, &port) != 3) {
	writelog();
	fprintf(stderr, "Bad server entry in config file, line %d\n", lineno);
	return -1;
    } else if (!(h = gethostbyname(hostname))) {
	writelog();
	fprintf(stderr, "Host '%s' not found, line %d\n", hostname, lineno);
	return -2;
    } else if (h->h_addrtype != AF_INET || h->h_length != 4) {
	writelog();
	fprintf(stderr, "Host '%s' not an Internet host, line %d\n",
		hostname, lineno);
	return -3;
    } else {
	s = MALLOC(Server, 1);
	strcpy(s->name, name);
	strcpy(s->hostname, hostname);
	s->port = port;
	s->addr = ntohl(*( (unsigned long *) h->h_addr_list[0]));
	s->id = sid++;
	s->last_msgid = -1;
	s->connected = 0;
	s->next = 0;
	if (prev) {
	    prev->next = s;
	} else {
	    servers = s;
	}
	prev = s;
	serv_addtolist(s->id);
    }
    return 0;
}

const char *
serv_id2name(Serverid  id)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (s->id >= 0 && s->id == id) {
	    return s->name;
	}
    }
    return "";
}

Serverid
serv_name2id(const char *name)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (!cool_strcasecmp(s->name, name)) {
	    return s->id;
	}
    }
    return -1;
}

void
serv_id2entry(Serverid server, char *buf)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (s->id == server) {
	    sprintf(buf, "%s %s %d\n", s->name, s->hostname, s->port);
	    return;
	}
    }
    buf[0] = '\0';
}

extern int	h_errno;

Server *
serv_add(struct sockaddr_in *from, const char *name)
{
    Server	*s, *prev = 0, *new;
    struct hostent	*h;
    int		 newid = 0;

    if (serv_addr2server(from) || serv_name2id(name) >= 0) {
	return 0;
    }
    for (s = servers; s; s = s->next) {
	prev = s;
	if (s->id >= newid) {
	    newid = s->id + 1;
	}
    }

    new = MALLOC(Server, 1);

    new->addr = ntohl(from->sin_addr.s_addr);
    new->port = ntohs(from->sin_port);
    strncpy(new->name, name, 30);
    if (!(h = gethostbyaddr((void *) &(from->sin_addr.s_addr), 
			    sizeof(from->sin_addr.s_addr), AF_INET))) {
	writelog();
	fprintf(stderr, "%s", addr_htoa(new->addr));
#if HAVE_HERROR
	herror( (char *) 0 );
#else
	fprintf(stderr, ":  gethostbyaddr() failed\n");
#endif
	FREE(new);
	return 0;
    }
    strncpy(new->hostname, h->h_name, 40);
    new->last_msgid = -1;
    writelog();

    fprintf(stderr, "SERVER %s (%s %d) added successfully.\n",
	    new->name, new->hostname, new->port);
    new->id = newid;
    serv_addtolist(newid);
    serv_addtocfg(new);

    if (prev) {
	prev->next = new;
    } else {
	servers = new;
    }

    return new;
}

static void
serv_addtolist(Serverid id)
{
    Var		 el;

    el.type = OBJ;
    el.v.obj.id = 0;	/* add #0@remoteserver */
    el.v.obj.server = id;
    server_list = list_setadd(server_list, el);
}

void
writelog(void)
{
    long	t = time ((time_t *) 0);
    char	*s = ctime((time_t *) &t);
    s[19] = '\0';		/* remove newline */

    fprintf(stderr, "%s:  ", s + 4);
}

Server
*serv_addr2server(struct sockaddr_in *sock)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (s->addr == ntohl(sock->sin_addr.s_addr)
	 && s->port == ntohs(sock->sin_port)) {
	    return s;
	}
    }
    return 0;
} 

Server
*serv_id2server(Serverid server)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (s->id == server) {
	    return s;
	}
    }
    return 0;
}

Server *
serv_name2server(const char *name)
{
    Server	*s;

    for (s = servers; s; s = s->next) {
	if (!cool_strcasecmp(s->name, name)) {
	    return s;
	}
    }
    return 0;
}

int
verify_server(Server *s, struct sockaddr_in *from)
{
    struct hostent	*h;

    if (!(h = gethostbyaddr((void *) &from->sin_addr.s_addr,
			    sizeof(from->sin_addr.s_addr), AF_INET))) {
	writelog();
	fprintf(stderr, "Failed verify host %s as %s (%s %d)",
	    addr_htoa(ntohl(from->sin_addr.s_addr)), s->name, s->hostname,
	    s->port);
#if HAVE_HERROR
	herror( (char *) 0 );
#else
	fprintf(stderr, ":  gethostbyaddr() failed\n");
#endif
	return 0;
    } else if (cool_strcasecmp(h->h_name, s->hostname)
	    || ntohs(from->sin_port) != s->port) {
	writelog();
	fprintf(stderr, "Host %s %d tried to connect as server %s (%s %d)\n",
	  h->h_name, ntohs(from->sin_port), s->name, s->hostname, s->port);
	return 0;
    } else {
	return 1;
    }
}

/*
 * serv_discardmsg()
 *
 * Returns non-zero if the system should discard a received message.
 */

int
serv_discardmsg(Serverid server, int msgid)
{
    Server	*s = serv_id2server(server);

    if (!s) {
	return -1;
    }
    if (msgid > s->last_msgid) {
	s->last_msgid = msgid;
	return 0;
    } else {
	return 1;
    }
}
