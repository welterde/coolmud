/* Copyright (c) 1993 Stephen F. White */

#ifndef STRING_H
#define STRING_H

typedef struct  String {
    int		 len;		/* length of string */
    int		 mem;		/* memory allocated */
    int		 ref;		/* reference count */
    char	*str;		/* string itself */
} String;

#ifdef INLINE
#define string_free(S) if (!--(S)->ref) { FREE(S); }
#define string_dup(S) ((S)->ref++, (S))
#else /* !INLINE */
void	 string_free(String *s);
String	*string_dup(String *s);
#endif /* !INLINE */

extern String	*string_new(int len);
extern String	*string_cpy(const char *s);
extern String	*string_ncpy(const char *s, int len);
extern String	*string_cat(String *str, const char *s);
extern String	*string_catc(String *str, char c);
extern String	*string_catnum(String *str, int n);
extern String	*string_indent_cat(String *str, int indent, const char *s);
extern String	*string_backslash(String *str, const char *s);
extern String	*string_output(String *str);
extern String	*string_fixquote(String *str);
extern String	*string_pad(String *str, int padlen, char tok);
extern String	*string_prepad(String *str, int padlen, char tok);
extern String	*string_strip_cr(String *str);

#endif /* !STRING_H */
