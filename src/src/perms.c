/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>
#include "config.h"
#include "cool.h"
#include "proto.h"

int
owns(Objid player, Objid what)
{
    Var		p, owners;
    Object     *o;

    if (!valid(player) || !(o = retrieve(what))) {
	return 0;
    }
    p.type = OBJ; p.v.obj = player;

    if (var_get_global(o, "owners", &owners) != E_NONE) {
	return 1;
    } else if (owners.type != LIST) {
	return 1;
    } else if (list_ismember(p, owners.v.list)) {
	return 1;
    }
    return 0;
}

int
is_wizard(Objid player)
{
    Var    	p, wizards;

    p.type = OBJ; p.v.obj = player;
    if (player.server == 0 && player.id == SYS_OBJ) {
	return 1;		/* SYS_OBJ is always a wizard :) */
    } else if (!valid(player)) {
	return 0;
    } else if (var_get_global(retrieve(sys_obj), "wizards", &wizards)
		!= E_NONE) {
	return 0;
    } else if (wizards.type != LIST) {
	return 0;
    } else if (list_ismember(p, wizards.v.list)) {
	return 1;
    }
    return 0;
}

/* Permissions should all be IN-DB!!
 * Except can_program, because without that someone could take control
 * of their objects out of the hands of wizards.
 * Robin Powell, May 14, 1998
 *
 */
int
can_program(Objid player, Objid what)
{
    return (owns(player, what) || is_wizard(player));
}

/*
 * Can't see a need for this to not be in-DB.
 *
int
can_clone(Objid player, Objid what)
{
    Var		public;

    if (owns(player, what) || is_wizard(player)) {
	return 1;
    } else if (var_get_global(retrieve(what), "public", &public) != E_NONE) {
	return 0;
    } else {
	return ISTRUE(public);
    }
}
 *
 */

int
can_clone( Objid player, Objid what)
{
    return 1;
}
