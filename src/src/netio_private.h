/* Copyright (c) 1993 Stephen F. White */

#ifndef NETIO_PRIVATE_H
#define NETIO_PRIVATE_H

extern int	errno;
extern int	server_running;
extern int	yo_sock;

typedef struct Player Player;

struct Player {
    Playerid			id;
    int				connected : 1;
    int				fd;
    unsigned long		addr;
    unsigned short		port;
    Buf				input;
    Buf				output;
    char		       *dangling_input;
    int				quota;
    int				outbound;
    Player		       *prev;
    Player		       *next;
};

extern Player	*players;

/*
 * from netio.c
 */
extern void	remove_player(Player *p);
extern void     send_output(int fd, Buf *output);

/*
 * from serverio.c
 */
extern void	connect_to_servers(void);
extern void	disconnect_from_servers(void);
extern void	server_command(struct sockaddr_in *from, char *cmd);

/*
 * from playerio.c
 */

extern void	queue_player_commands(struct timeval cur_time,
						struct timeval *timeout);
extern void	parse_connect (char *msg, char **command, char **user,
				char **pass);
#endif /* !NETIO_PRIVATE_H */
