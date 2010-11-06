/* Copyright (c) 1993 Stephen F. White */

#ifndef SERVERS_H
#define SERVERS_H

extern Serverid		 serv_name2id(const char *name);
extern const char	*serv_id2name(Serverid server);
extern void		 serv_id2entry(Serverid server, char *buf);
extern int		 serv_discardmsg(Serverid server, int msgid);

#endif /* !SERVERS_H */
