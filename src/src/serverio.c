/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <ctype.h>

#include "config.h"
#include "string.h"
#include "netio.h"
#include "buf.h"
#include "netio_private.h"
#include "servers_private.h"
#include "servers.h"
#include "sys_proto.h"
#include "socket_proto.h"

static Server *promiscuous_connect(struct sockaddr_in *from,
		const char *name);

static int
send_to_server(Server *s, const char *buf)
{
    struct sockaddr_in	addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(s->port);
    addr.sin_addr.s_addr = htonl(s->addr);

    if (sendto(yo_sock, buf, strlen(buf), 0, (struct sockaddr *) &addr,
	    sizeof(addr)) < 0) {
	return -1;
    } else {
	return 0;
    }
}

void
connect_to_servers(void)
{
    char	 	buf[128];
    Server	       *s;

    s = serv_id2server(0);
    sprintf(buf, "*connect %s\n", s->name);
    for (s = servers; s; s = s->next) {
	if (s->id == 0) {	/* skip local server */
	    continue;
	}
	if (send_to_server(s, buf)) {
	    writelog();
	    fprintf(stderr, "Couldn't send connect msg to server ");
	    perror(s->name);
	}
    }
}

void
disconnect_from_servers(void)
{
    char	 	buf[128];
    Server	       *s;

    s = serv_id2server(0);
    sprintf(buf, "*disconnect %s\n", s->name);
    for (s = servers; s; s = s->next) {
	if (!s->id) {		/* skip local server */
	    continue;
	}
	if (s->id && s->connected) {
	    if (send_to_server(s, buf)) {
		writelog();
		fprintf(stderr, "Couldn't send disconnect msg to server ");
		perror(s->name);
	    }
	}
    }
}

int
yo(Serverid server, const char *msg)
{
    Server			*s;

    if (!(s = serv_id2server(server))) {
	return -1;
    } else if (!s->connected) {
	return -2;
    } else {
	if (send_to_server(s, msg)) {
	    return -3;
	} else {
	    return 0;
	}
    }
}

Server *
promiscuous_connect(struct sockaddr_in *from, const char *name)
{
    Server	*s = 0;

    if (promiscuous) {
	s = serv_add(from, name);
	if (!s) {
	    writelog();
	    fprintf(stderr, "INVALID SERVER %s(%d), ignored\n",
		    addr_htoa(ntohl(from->sin_addr.s_addr)),
		    ntohs(from->sin_port));
	}
    } else {
	writelog();
	fprintf(stderr, "UNKNOWN SERVER %s(%d), refused\n",
		addr_htoa(ntohl(from->sin_addr.s_addr)), htons(from->sin_port));
    }
    return s;
}

static void
meta_command(struct sockaddr_in *from, char *command)
{
    Server	*s;
    char	*cmd, *name, *dummy;

    parse_connect(command, &cmd, &name, &dummy);
    s = serv_name2server(name); 
    if (!strcmp(cmd + 1, "connect")) {
	char		buf[128];
	Server	       *local;

	if (!s) {
	    s = promiscuous_connect(from, name);
	    if (!s) {
		return;
	    }
	} else if (verify_servers && !verify_server(s, from)) {
	    return;
	}
	s->connected = 1;
	s->last_msgid = -1;
	writelog();
	fprintf(stderr, "Server %s (%s %d) connected\n", s->name, s->hostname,
		s->port);
	local = serv_id2server(0);
	sprintf(buf, "*connectok %s\n", local->name);
	sendto(yo_sock, buf, strlen(buf), 0, (struct sockaddr *) from,
		sizeof(*from));
	connect_server(s->id);
    } else if (!strcmp(cmd + 1, "connectok")) {
	if (!s) {
	    s = promiscuous_connect(from, name);
	    if (!s) {
		return;
	    }
	} else if (verify_servers && !verify_server(s, from)) {
	    return;
	}
	s->connected = 1;
	s->last_msgid = -1;
	writelog();
	fprintf(stderr, "Server %s (%s %d) connected ok\n", s->name,
		s->hostname, s->port);
	connect_server(s->id);
    } else if (!strcmp(cmd + 1, "disconnect")) {
	if (!s) {
	    return;
	}
	s->connected = 0;
	writelog();
	fprintf(stderr, "Server %s (%s %d) disconnected\n", s->name,
		s->hostname, s->port);
	disconnect_server(s->id);
    } else {
	if (!s) {
	    return;
	}
	writelog();
	fprintf(stderr, "Unknown metacommand \"%s\" from server %s\n",
		command, s->name);
    }
}

void
server_command(struct sockaddr_in *from, char *cmd)
{
    Server	*s;
    
    if (cmd[0] == '*') {
	meta_command(from, cmd); 
    } else {
	s = serv_addr2server(from);
	  /* quietly ignore messages from unconnected servers */
	if (s && s->connected) {
	    receive_message(s->id, cmd);
	}
    }
}
