/* Copyright (c) 1993 Stephen F. White */

#ifndef CODE_H
#define CODE_H

extern Inst    *machine;
extern int	progi, machine_size;

#define code2(a, b) code(a); code(b);
#define code3(a, b, c) code(a); code(b); code(c);
#define code4(a, b, c, d) code(a); code(b); code(c); code(d);

#endif /* !CODE_H */
