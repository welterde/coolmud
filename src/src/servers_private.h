/* Copyright (c) 1993 Stephen F. White */

#ifndef SERVERS_PRIVATE_H
#define SERVERS_PRIVATE_H

typedef struct Server Server;

struct Server {
    Serverid		 id;
    char	         name[30];
    char	         hostname[40];
    unsigned long	 addr;
    unsigned short	 port;
    int			 connected : 1;		/* connected flag */
    int			 last_msgid;		/* id of the last msg rec'd */
    Server		*next;
};

extern Server	*servers;

extern Server	*serv_addr2server(struct sockaddr_in *sock);
extern Server	*serv_name2server(const char *name);
extern Server	*serv_id2server(Serverid server);
extern Server	*serv_add(struct sockaddr_in *sock, const char *name);
extern int	 verify_server(Server *s, struct sockaddr_in *from);

#endif /* !SERVERS_PRIVATE_H */
