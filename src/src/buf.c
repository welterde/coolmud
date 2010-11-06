/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include <sys/time.h>

#include "config.h"
#include "buf.h"
#include "netio.h"

void
buf_add(Buf *buf, String *str)
{
    Line	*new;

    new = MALLOC(Line, 1);
    new->str = str;
    new->ptr = str->str;
    new->next = 0;
    if (!buf->tail) {
	buf->tail = new;
	buf->head = new;
    } else {
	buf->tail->next = new;
	buf->tail = new;
    }
}

void
buf_delhead(Buf *buf)
{
    Line	*old = buf->head;

    if (!old) {
	return;
    } else {
	buf->head = old->next;
	if (!old->next) {		/* if nothing in queue, */
	    buf->tail = 0;		/* reset tail as well */
	}
	string_free(old->str);
	FREE(old);
    }
}

void
buf_init(Buf *buf)
{
    buf->head = 0;
    buf->tail = 0;
}

void
buf_free(Buf *buf)
{
    Line	*line, *next;

    for (line = buf->head; line; line = next) {
	next = line->next;
	string_free(line->str);
	FREE(line);
    }
    buf_init(buf);
}
