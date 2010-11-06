/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"

struct e_entry {
    Error	err;
    const char *name;
    const char *desc;
} errors[NERRS] = {
    { E_NONE, "E_NONE", "No error" },
    { E_TYPE, "E_TYPE", "Type mismatch" },
    { E_ARGTYPE, "E_ARGTYPE", "Argument type mismatch" },
    { E_NARGS, "E_NARGS", "Incorrect number of arguments" },
    { E_RANGE, "E_RANGE", "Range error" },
    { E_INVIND, "E_INVIND", "Invalid indirection" },
    { E_DIV, "E_DIV", "Division by zero" },
    { E_MAXREC, "E_MAXREC", "Maximum recursion exceeded" },
    { E_METHODNF, "E_METHODNF", "Method not found" },
    { E_VARNF, "E_VARNF", "Variable not found" },
    { E_VERBNF, "E_VERBNF", "Verb not found" },
    { E_FOR, "E_FOR", "For variable not a list" },
    { E_SERVERNF, "E_SERVERNF", "Server not found" },
    { E_SERVERDN, "E_SERVERDN", "Server down" },
    { E_OBJNF, "E_OBJNF", "Object not found" },
    { E_MESSAGE, "E_MESSAGE", "Message unparseable" },
    { E_TIMEOUT, "E_TIMEOUT", "Timed out" },
    { E_STACKOVR, "E_STACKOVR", "Stack overflow" },
    { E_STACKUND, "E_STACKUND", "Stack underflow" },
    { E_PERM, "E_PERM", "Permission denied" },
    { E_INTERNAL, "E_INTERNAL", "Internal error" },
    { E_FILE, "E_FILE", "File not found" },
    { E_TICKS, "E_TICKS", "Task ran out of ticks" },
    { E_TERM, "E_TERM", "Task terminated" },
    { E_MAPNF, "E_MAPNF", "Mapping not found" },
};

const char *
err_id2desc(Error e)
{
    int		i;

    for (i = 0; i < Arraysize(errors); i++) {
	if (errors[i].err == e) {
	    return errors[i].desc;
	}
    }
    return "Unknown Error";
}

const char *
err_id2name(Error e)
{
    int		i;

    for (i = 0; i < Arraysize(errors); i++) {
	if (errors[i].err == e) {
	    return errors[i].name;
	}
    }
    return "UNKNOWN_ERROR";
}

int
err_name2id(const char *name)
{
    int		i;

    for (i = 0; i < Arraysize(errors); i++) {
	if (!cool_strcasecmp(errors[i].name, name)) {
	    return errors[i].err;
	}
    }
    return -1;
}

