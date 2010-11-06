/* Copyright (c) 1993 Stephen F. White */

#ifndef NETIO_H
#define NETIO_H

/*
 * these symbols must be provided by the application
 */

extern void	 parse(Playerid player, const char *command, int fd);
extern void	 new_connect(Playerid player, int fd);
extern int	 receive_message(Serverid server, const char *msg);
extern void	 process_queues(struct timeval cur_time,
					struct timeval *timeout);
extern void	 disconnect_player(Playerid who);
extern void	 connect_server(Serverid server);
extern void	 disconnect_server(Serverid server);
extern int	 init(const char *dbfilename, int send_boot, int db_must_exist);
extern void	 shutdown_server(void);
extern void	 panic(const char *);
extern void	*cool_malloc(unsigned size);
extern void	 cool_free(void *);
extern char	*str_dup(const char *);
extern void	 cmdline_compile(const char *dbfile, const char *progfile,
				 int do_init);
extern void	 write_flatfile(const char *dbfile, const char *dumpfile);

/*
 * these symbols are provided by the interface
 */

extern void	tell(Playerid player, const char *msg, char newline );
extern void	tell_fd(int fd, const char *msg, char newline );
extern int	yo(Serverid server, const char *msg);
extern void	set_parse(int fd, Playerid player);
extern int	net_open_connect(Playerid id, const char *hostname, short port);
extern void	disconnect(Playerid who);
extern void	disconnect_fd(int fd);
extern void	writelog(void);
extern int	read_config(const char *filename);
extern const char	*addr_htoa(unsigned long l);
extern int	server_running;	/* if zero, interface should shut down */
extern int	promiscuous;	/* promiscuous mode for server connects */
extern int	verify_servers;	/* verify hostnames of incoming servers */
extern int  sleep_to_refresh;	/* ticks and age refreshed after n seconds */
extern int	registration;	/* prevent player creation */
extern int	corefile;	/* to save a corefile on panic */
extern int	max_age;	/* maximum nesting of methods */
extern int	max_ticks;	/* maximum number of ticks per task */
extern short	yo_port, player_port;	/* yo and player ports */
extern char	*welcome;	/* msg displayed to users on connect */
					/* must be set by the application */


#endif /* !NETIO_H */
