/* Copyright (c) 1993 Stephen F. White */

#ifndef BUF_H
#define BUF_H

#include "string.h"

typedef struct Line Line;

struct Line {
    String	*str;
    const char	*ptr;
    Line	*next;
};

typedef struct Buf {
    Line	*head, *tail;
} Buf;

extern void	buf_add(Buf *buf, String *str);
extern void	buf_delhead(Buf *buf);
extern void	buf_init(Buf *buf);
extern void	buf_free(Buf *buf);

#endif /* !BUF_H */
