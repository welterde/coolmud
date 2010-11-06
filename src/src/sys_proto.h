/* Copyright (c) 1993 Stephen F. White */

#ifndef SYS_PROTO_H
#define SYS_PROTO_H

/*
 * prototypes for library and system calls
 */
#ifdef PROTO
/* extern char    *index(const char *, char); */
extern FILE    *fopen(const char *filename, const char *type);
extern int	fclose(FILE *);
extern void	unlink(const char *file);
extern int	atoi(const char *);
extern long	random(void);
extern void	srandom(int seed);
extern char    *crypt(const char *key, const char *salt);
extern void	exit(int status), _exit(int status);
extern int	socket(int domain, int type, int protocol);
extern void	perror(const char *s);
extern void	herror(const char *s);
extern int	fprintf(FILE *stream, const char *s, ...);
extern int	fputs(const char *s, FILE *stream);
extern int	fputc(char c, FILE *stream);
extern int	fscanf(FILE *stream, const char *s, ...);
extern int	fseek(FILE *stream, long offset, int ptrname);
extern int	fread(const char *ptr, int size, int nitems, FILE *stream);
extern int	fwrite(const char *ptr, int size, int nitems, FILE *stream);
extern int	fflush(FILE *stream);
extern int	sscanf(const char *s, const char *t, ...);
extern int	ungetc(int c, FILE *stream);
extern int	fgetc(FILE *stream);
extern int	_filbuf(FILE *stream);
extern void	bcopy(const void *from, void *to, int len);
extern int	getdtablesize(void);
extern void	bzero(char *, int);
extern void	perror(const char *);
extern void	close(int);
extern int	setsockopt(int, int, int, char *, int);
extern int	listen(int, int);
extern int	write(int, const char *, int);
extern int	read(int, char *, int);
extern int	fcntl(int, int, int);
extern int	shutdown(int, int);
extern int	kill(int, int);
extern int	getpid(void);
#ifndef toupper
extern int	toupper(int);
#endif /* toupper */
#if HAVE_STRCASECMP
extern int      strcasecmp(const char *s, const char *t);
extern int      strncasecmp(const char *s, const char *t, int n);
#endif /* HAVE_STRCASECMP */
#endif /* PROTO */

#endif /* !SYS_PROTO_H */
