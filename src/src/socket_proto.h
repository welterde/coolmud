/* Copyright (c) 1993 Stephen F. White */

#ifndef SOCKET_PROTO_H
#define SOCKET_PROTO_H

/*
 * Prototypes for system socket calls.
 */

#ifdef PROTO
extern int	select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int	accept(int, struct sockaddr *, int *);
extern int	bind(int, struct sockaddr *, int);
extern int	connect(int, struct sockaddr *, int);
extern int	sendto(int, const char *, int, int, struct sockaddr *, int);
extern int	recvfrom(int, char *, int, int, struct sockaddr *, int *);
extern char	*inet_ntoa(struct in_addr  in);
#endif /* PROTO */

#endif /* !SOCKET_PROTO_H */
