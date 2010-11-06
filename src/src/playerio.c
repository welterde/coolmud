#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "config.h"
#include "sys_proto.h"
#include "string.h"
#include "netio.h"
#include "buf.h"
#include "netio_private.h"
#include "servers.h"

static struct timeval	msec2timeval(int msec);
static void	player_command(Player *p, char *command );
static void	player_output(Player *p, const char *msg, char newline );

static void
player_output(Player *p, const char *msg, char newline )
{
    String	*str;

    str = string_cpy(msg);
    if( newline )
	str = string_cat(str, "\r\n");
    buf_add(&p->output, str);
}

void
tell(Playerid player, const char *msg, char newline )
{
    Player	*p;

    for(p = players; p; p = p->next) {
	if (p->id == player) {
	    player_output(p, msg, newline );
	    break;
	}
    }
}

void
tell_fd(int fd, const char *msg, char newline)
{
    Player	*p;

    for(p = players; p; p = p->next) {
	if (p->fd == fd) {
	    player_output(p, msg, newline );
	    break;
	}
    }
}

static void
player_disconnect(Player *p)
{
    /* make one last-ditch attempt to flush output buffer */
    send_output(p->fd, &p->output);
    disconnect_player(p->id);
    remove_player(p);
}

void
disconnect(Playerid who)
{
    Player	*p;

    for(p = players; p; p = p->next) {
	if (p->id == who) {
	    player_disconnect(p);
	}
    }
}

void
disconnect_fd(int fd)
{
    Player	*p;

    for(p = players; p; p = p->next) {
	if (p->fd == fd) {
	    player_disconnect(p);
	}
    }
}

void
player_command(Player *p, char *command)
{
    while(isspace(*command)) {
	command++;
    }
    parse(p->id, command, p->fd);
}

#define MSEC(T) (T.tv_sec * 1000 + T.tv_usec / 1000)

static struct timeval
msec2timeval(int msec)
{
    struct timeval	r;

    r.tv_sec = msec / 1000;
    r.tv_usec = (msec % 1000 ) * 1000;
    return r;
}

void
parse_connect (char *msg, char **command, char **user, char **pass)
{
    while (*msg && isascii(*msg) && isspace (*msg))     /* skip whitespace */
        msg++;
    *command = msg;
    while (*msg && isascii(*msg) && !isspace (*msg))    /* skip command */
        msg++;
    if (*msg) {
        *msg++ = '\0';                                  /* terminate cmd */
    }
    while (*msg && isascii(*msg) && isspace (*msg))     /* skip whitespace */
        msg++;
    *user = msg;
    while (*msg && isascii(*msg) && !isspace (*msg))    /* skip user */
        msg++;
    if (*msg) {
        *msg++ = '\0';                                  /* terminate user */
    }
    while (*msg && isascii(*msg) && isspace (*msg))     /* skip whitespace */
        msg++;
    *pass = msg;
    while (*msg && isascii(*msg) && !isspace (*msg))    /* skip password */
        msg++;
    *msg = '\0';                                        /* terminate pass */
}

void
queue_player_commands(struct timeval cur_time, struct timeval *timeout)
{
    Player		*p;
    int			msec_since_last;
    static struct timeval	last = {0, 0};
    struct timeval	new;

    msec_since_last = MSEC(cur_time) - MSEC(last);
    last = cur_time;
    for (p = players; p; p = p->next) {
	p->quota += msec_since_last;
	if (p->quota > MAX_CMDS * MSEC_PER_CMD) {
	    p->quota = MAX_CMDS * MSEC_PER_CMD;
	}
	if (p->input.head && p->quota >= 0) {
	    player_command(p, p->input.head->str->str);
	    buf_delhead(&p->input);
	    p->quota -= MSEC_PER_CMD;
	}
	if (p->input.head && p->quota < 0) {
	      /* return when quota is positive */
	    new = msec2timeval(-p->quota);
	    *timeout = (timercmp(&new, timeout, <)) ? new : *timeout;
	}
    }
}

void
set_parse(int fd, Playerid player)
{
    Player	*p, *pnext;

    for (p = players; p; p = pnext) {
	pnext = p->next;
	if (p->id == player && p->fd != fd) {
	    /* write(p->fd, DISCONNECT_MSG, strlen (DISCONNECT_MSG)); */
	    disconnect_player(p->id);
	    remove_player(p);
	} else if (p->fd == fd) {
	    p->connected = 1;
	    p->id = player;
	}
    }
}
