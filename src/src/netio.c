/* Copyright (c) 1993 Stephen F. White */

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "config.h"
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include "string.h"
#include "netio.h"
#include "buf.h"
#include "netio_private.h"
#include "sys_proto.h"
#include "socket_proto.h"
#include "servers.h"

int	server_running;

Player	*players = 0;

short		 yo_port;
short		 player_port = PLAYER_PORT;
int		 yo_sock;
char		*progfname;
char		*dumpfname;
int		 max_age = MAX_AGE;
int		 max_ticks = MAX_TICKS;

static int	 player_sock;
static int	 nfds;
static int	 compiler = 0;		/* flag: command line compiler mode */
static int	 dumper = 0;		/* flag: dump flatfile mode */
static int	 do_init = 0;		/* flag: call init() in new objects */

static void	 init_fdsets(fd_set *inputfds, fd_set *outputfds,
			     struct timeval *timeout);
static void	 process_fdsets(fd_set *inputfds, fd_set *outputfds);
static void	 init_signals(void);
static void	 close_sockets(void);
static int	 server_socket(int port, int type);
static void	 run_server(void);
static void	 server_input(void);
static int	 player_input(Player *p);
static Player	*new_player(void);
static Player	*init_player(int psock);
static void	 set_nonblock(int fd);
static int	 do_options(int  argc, char  *argv[]);
static long	 safe_gethostbyname(const char *hostname, int timeout);
static const char	*shutdown_message = "*** The server has been shut down. ***\r\n";

typedef void  (*PFV)(int sig);

void		 sighandler(int sig, int code);

static int
do_options(int  argc, char  *argv[])
{
    int		 i, j, done_option, newargc = 0;

    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    done_option = 0;
	    for (j = 1; j < strlen(argv[i]) && !done_option; j++) {
		switch (argv[i][j]) {
		  case 'p':
		    promiscuous = 1;
		    break;
		  case 'c':
		    corefile = 1;
		    break;
		  case 'i':
		    do_init = 1;
		    break;
		  case 'f':
		    compiler = 1;
		    if (i >= argc - 1) {
			return 0;
		    }
		    progfname = argv[++i];
		    done_option = 1;
		    break;
		  case 'd':
		    dumper = 1;
		    if (i >= argc - 1) {
			return 0;
		    }
		    dumpfname = argv[++i];
		    done_option = 1;
		    break;
		  default:
		    fprintf(stderr, "Unknown option:  -%c\n", argv[i][j]);
		    return 0;
		} /* for i */
	    } /* if */
	} else {
	    argv[newargc++] = argv[i];
	} /* if */
    } /* for j */
    return newargc;
} /* do_options() */

void
main(int  argc, char  *argv[])
{
    argc = do_options(argc, argv);
    if (argc != 2) {
	fprintf (stderr, "Usage: %s [-pc -f progfname -d dumpfname] dbfile\n",
		argv[0]);
	exit (1);
    }
    if (compiler) {
	cmdline_compile(argv[1], progfname, do_init);
	exit(0);
    } else if (dumper) {
	write_flatfile(argv[1], dumpfname);
	exit(0);
    }
    if (init(argv[1], 1, 1)) {
	exit(2);
    }
    writelog();
    fprintf(stderr, "Initializing sockets.\n");
    player_sock = server_socket(player_port, SOCK_STREAM);
    yo_sock = server_socket(yo_port, SOCK_DGRAM);
    if (player_sock < 0 || yo_sock < 0) {
	exit(3);
    }
    nfds = yo_sock + 1;
    writelog();
    fprintf(stderr, "Connecting to remote servers.\n");
    connect_to_servers();
    init_signals();

/*
 * do it
 */
    writelog();
    fprintf(stderr, "Server initialized.\n");

    run_server();		/* there 'tis */

    close_sockets();
    shutdown_server();
    writelog();
    fprintf(stderr, "Server exiting.\n");
    exit (0);
}

#ifdef PROTO
extern void		gettimeofday(struct timeval *, struct timezone *);
#endif /* PROTO */
struct timeval		cur_time;		/* grabbed at start of loop */
static int		totfds;

static void
run_server(void)
{
    fd_set		inputfds, outputfds;
    struct timeval	timeout;

  /* reserve 9 fd's: 3 for stdio, 2 for server sockets, 3 for db, 1 for misc */
    totfds = getdtablesize() - 9;
    
    server_running = 1;
    while (server_running) {
	gettimeofday(&cur_time, 0);
	timeout.tv_sec = 60; timeout.tv_usec = 0;

	queue_player_commands(cur_time, &timeout);
	process_queues(cur_time, &timeout);

	if (!server_running) {
	    break;
	}

	init_fdsets(&inputfds, &outputfds, &timeout);

	if (select(nfds, &inputfds, &outputfds, (fd_set *) 0, &timeout) < 0) {
	    if (errno != EINTR) {
		writelog();
		perror("select failed");
		server_running = 0;
	    }
	} else {
	    gettimeofday(&cur_time, 0);
	    process_fdsets(&inputfds, &outputfds);
	}
    } /* while */
}	

static void
init_signals(void)
{
    signal(SIGINT, (PFV) sighandler);
    signal(SIGQUIT, (PFV) sighandler);
    signal(SIGILL, (PFV) sighandler);
    signal(SIGTRAP, (PFV) sighandler);
    signal(SIGIOT, (PFV) sighandler);
    signal(SIGFPE, (PFV) sighandler);
#ifndef linux
    signal(SIGBUS, (PFV) sighandler);
    signal(SIGEMT, (PFV) sighandler);
    signal(SIGSYS, (PFV) sighandler);
#endif /* !linux */
    signal(SIGSEGV, (PFV) sighandler);
    signal(SIGPIPE, SIG_IGN);	
    signal(SIGTERM, (PFV) sighandler);
    signal(SIGURG, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
    signal(SIGWINCH, (PFV) SIG_IGN);
}

void
sighandler(int sig, int code)
{
    char	message[80];
    int		i;

/*
 * restore all signals to default handling
 */
    for (i = 1; i < NSIG; i++) {
	signal(i, SIG_DFL);
    }

    sprintf (message, "Caught signal %d, code %d", sig, code);
    panic(message);
    close_sockets();
    if (corefile) {
	writelog();
	fprintf(stderr, "Dumping corefile.\n");
	(void) abort();
    }
    writelog();
    fprintf(stderr, "Server aborting.\n");
    _exit (10);
}

static void
init_fdsets(fd_set *inputfds, fd_set *outputfds, struct timeval *timeout)
{
    Player	*p;

    FD_ZERO(inputfds);
    FD_ZERO(outputfds);
      /* only wait for new players if there are free descriptors */
    if (nfds < totfds) {
	FD_SET(player_sock, inputfds);
    }

    FD_SET(yo_sock, inputfds);	/* always wait for UDP input */
      /*
       * always wait for input from players; wait for output if there's some
       * output to send
       */
    for (p = players; p; p = p->next) {
	FD_SET(p->fd, inputfds);
	if (p->input.head && p->quota >= 0) {
	    timeout->tv_sec = timeout->tv_usec = 0;
	}
	if (p->output.head || (p->outbound && !p->connected)) {
	    FD_SET(p->fd, outputfds);
	}
    }
}

static void
process_fdsets(fd_set *inputfds, fd_set *outputfds)
{
    Player	*p, *pnext;

    if (FD_ISSET(yo_sock, inputfds)) {		/* if there's UDP yo input */
	server_input();				/* handle it */
    }
    if (FD_ISSET(player_sock, inputfds)) {	/* new player wants connect */
	if ((p = new_player()) && p->fd >= nfds) {
	    nfds = p->fd + 1;
	}
    }
    for (p = players; p; p = pnext) {		/* for all players */
	pnext = p->next;
	if (FD_ISSET(p->fd, outputfds)) {	/* socket ready for writing */
	    if (p->outbound && !p->connected) {	/* if it's outbound connect, */
		p->connected = 1;		/* it just connected */
		fprintf(stderr, "Outbound connect #%d, fd %d to %s(%d) eventually succeeded\n",
		        p->id, p->fd, addr_htoa(p->addr), p->port);
		new_connect(p->id, p->fd);
	    } else {
		send_output(p->fd, &p->output);	/* player output, send it */
	    }
	}
	if (FD_ISSET(p->fd, inputfds)) {	/* player has input pending */
	    if (player_input(p) && !p->input.head) {	/* handle it; if conn dies, */
		if (p->connected) {
		    disconnect_player(p->id);
		}
		remove_player(p);		/* remove player */
	    }
	}
    }
}

static void
server_input(void)
{
    char	 buf[UDP_BLOCK_SIZE + 1];
    int		 got;
    struct sockaddr_in	from;
    int		 fromlen;

    fromlen = sizeof(from);
    got = recvfrom(yo_sock, buf, UDP_BLOCK_SIZE, 0, (struct sockaddr *) &from,
		&fromlen);
    if (got <= 0) {
	return;
    }
    buf[got] = '\0';
    server_command(&from, buf);
}

static Player *
new_player(void)
{
    int			psock;
    struct sockaddr_in	from;
    int			fromlen;
    Player		*p;

    fromlen = sizeof(from);
    psock = accept(player_sock, (struct sockaddr *) &from, &fromlen);
    if (psock < 0) {
	return 0;
    } else {
	p = init_player(psock);
	p->connected = 0;
	p->addr = ntohl(from.sin_addr.s_addr);
	p->port = ntohs(from.sin_port);
	writelog();
	fprintf(stderr, "Accepted player from %s(%d)\n",
		 addr_htoa(p->addr), p->port);
        new_connect(p->id, p->fd);
	return p;
    }
}

void
remove_player(Player *p)
{
    if (p->connected) {
	writelog();
	fprintf(stderr, "Player #%d disconnected from fd %d\n", p->id, p->fd);
    } else {
	writelog();
	fprintf(stderr, "Player disconnected from fd %d, never connected\n",
		p->fd);
    }
    buf_free(&p->input);
    buf_free(&p->output);
    shutdown(p->fd, 2);
    close(p->fd);
    if (p->prev) {
	p->prev->next = p->next;
    } else {
	players = p->next;
    }
    if (p->next) {
	p->next->prev = p->prev;
    }
    FREE(p);
}

static Player *
init_player(int psock)
{
    Player	*newp = MALLOC(Player, 1);

    set_nonblock(psock);
    newp->fd = psock;
    newp->connected = 0;
    newp->id = SYS_OBJ;
    newp->outbound = 0;
    buf_init(&newp->input);
    buf_init(&newp->output);
    newp->dangling_input = 0;
    newp->quota = MAX_CMDS * MSEC_PER_CMD;
    if (players) {
	players->prev = newp;
    }
    newp->next = players;
    newp->prev = 0;
    players = newp;
   
    return newp;
}

static int
server_socket(int port, int type)
{
    struct sockaddr_in	to;
    int			servsock, opt;

    servsock = socket(AF_INET, type, 0);
    if (servsock < 0) {
	writelog();
	perror("Couldn't make server socket");
	return -1;
    }
    set_nonblock(servsock);
    opt = 1;
    if (setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &opt, sizeof(opt)) < 0) {
	writelog();
	perror("Couldn't set socket options");
	return -2;
    }
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = INADDR_ANY;
    to.sin_port = htons(port);
    if (bind(servsock, (struct sockaddr *) &to, sizeof(to)) < 0) {
	writelog();
	fprintf(stderr, "%s port %d:  ",
	    (type == SOCK_STREAM ? "TCP" : "UDP"), port);
	perror("couldn't bind()");
	close(servsock);
	return -3;
    }
    if (type == SOCK_STREAM) {
	if (listen(servsock, 5) < 0) {
	    perror ("Couldn't listen()");
	    return -4;
	}
    }
    return servsock;
}

void
send_output(int fd, Buf *output)
{
    Line	*line, *next;
    int		len, written;

    for (line = output->head; line; line = next) {
	next = line->next;
	len = line->str->len - (line->ptr - line->str->str);
	written = write(fd, line->ptr, len);
	if (written < 0) {	/* error, including EWOULDBLOCK */
	    break;
	} else if (written < len) {
	    line->ptr += written;
	    break;
	} else {
	    buf_delhead(output);
	}
    }
}

static void
set_nonblock(int fd)
{
    char	buf[128];
    int		flags;

    /* AIX can't cope with using fcntl() for setting things nonblocking */
#ifdef _ALL_SOURCE
    flags = 1;
    ioctl(fd,FIONBIO,&flags);
#else /* !_ALL_SOURCE */

    flags = fcntl(fd, F_GETFL);
#ifndef linux
    if (flags < 0 || fcntl(fd, F_SETFL, flags | FNDELAY) == -1) {
#else /* linux */
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
#endif /* linux */
	sprintf(buf, "Couldn't set descriptor %d nonblocking", fd);
	writelog();
	perror(buf);
    }
#endif /* _ALL_SOURCE */

}

static String *
grab_input(int fd, char **dangling_input)
{
    char	 buf[BLOCK_SIZE + 1];
    int		 got;
    String	*str;

    got = read (fd, buf, BLOCK_SIZE); 
    if (got <= 0) {
	return 0;
    }
    str = string_new(0);
    if (*dangling_input) {		/* some input left from last time */
	str = string_cat(str, *dangling_input);	/* prepend it */
	FREE(*dangling_input);
	*dangling_input = 0;
    }
    do {
	buf[got] = '\0';
	str = string_cat(str, buf);
	got = read (fd, buf, BLOCK_SIZE); 
    } while (got > 0);
    return str;
}

static int
player_input(Player *p)
{
    String	*str, *cmd;
    char	*q, *r;

    if (!(str = grab_input(p->fd, &p->dangling_input))) {
	return -1;
    }
    for (q = r = str->str; r < str->str + str->len; r++) {
	if (*r == '\n') {		/* save command */
	    *q = '\0';
	    cmd = string_new(0);
	    cmd = string_cat(cmd, str->str);
	    buf_add(&p->input, cmd);
	    q = str->str;		/* restart at beginning of buf */
	} else if (*r == '\t') {	/* change tabs to spaces */
	    *q++ = ' ';
	} else if (*r == '\b') {	/* Handle visible ^h's */
	    q--;
	} else if (isascii (*r) && isprint (*r)) {	/* store printable */
	    *q++ = *r;
	}
    }
    if(q > str->str) {			/* there's half a command here */
	*q = '\0';			/* terminate it */
	p->dangling_input = str_dup(str->str);	/* copy it */
    }
    string_free(str);
    return 0;
}

static void
close_sockets(void)
{
    Player	*p, *pnext;

    for (p = players; p; p = pnext) {
	pnext = p->next;
	write(p->fd, shutdown_message, strlen(shutdown_message));
	(void) shutdown(p->fd, 2);
	close(p->fd);
    }
    close(player_sock);
    writelog();
    fprintf(stderr, "Disconnecting from remote servers.\n");
    disconnect_from_servers();
    close(yo_sock);
}

/* 
 * safe_gethostbyname(hostname, timeout)
 *
 * Do a gethostbyname(), with a timer set to expire in _timeout_ seconds
 */

long
safe_gethostbyname(const char *hostname, int timeout)
{
    struct hostent      *h = 0;

    alarm(timeout);
    h = gethostbyname(hostname);
    if (h) { 
	return ntohl(*( (unsigned long *) h->h_addr_list[0]));
    } else {
	return 0;
    }
}

/*
 * net_open_connect(id, hostname, port)
 */

int
net_open_connect(Playerid id, const char *hostname, short port)
{
    long	addr = safe_gethostbyname(hostname, OUTBOUND_TIMEOUT);
    int		newsock;
    struct sockaddr_in	dest;
    Player	*p, *pnext;
    int		err;

    if (!addr) {
	return -1;
    }
    for (p = players; p; p = pnext) {
        pnext = p->next;
        if (p->id == id) {
            disconnect_player(p->id);
            remove_player(p);
	}
    }
    newsock = socket(AF_INET, SOCK_STREAM, 0);
    if (newsock < 0) {
	return -2;
    }
    if (newsock >= nfds) {
	nfds = newsock + 1;
    }
    p = init_player(newsock);
    p->addr = addr;
    p->port = port;
    p->id = id;
    p->outbound = 1;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(addr);
    dest.sin_port = htons(port);
    writelog();
    fprintf(stderr, "Outbound connect #%d, fd %d to %s/%s(%d)",
	     id, newsock, hostname, addr_htoa(p->addr), p->port);
    err = connect(newsock, (struct sockaddr *) &dest, sizeof(dest));
    if (err < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
	perror("failed:  ");
	return -3;
    } else if (err < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
	p->connected = 0;
	fprintf(stderr, " in progress\n");
	return -4;
    } else {
	p->connected = 1;
	fprintf(stderr, " succeeded\n");
	new_connect(p->id, p->fd);
	return newsock;
    }
}
