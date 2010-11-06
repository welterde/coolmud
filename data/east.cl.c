#include "boot.clh"
#include "common.clh"

object WESTERN_ROOM
    parents ROOM;

    name = "Western Room";
    desc = "Y'all are in the western room, cowboy.";
    exits = { EXIT_EAST };

    method init
	this.add_owner(WIZARD);
    endmethod
endobject

object EXIT_EAST
    parents EXIT;

    name = "e;east";
    leave = "You go east.";
    oleave = "%n goes east.";
    oarrive = "%n comes in from the west.";
    source = WESTERN_ROOM;
    dest = EASTERN_ROOM;

    method init
	this.add_owner(WIZARD);
    endmethod

endobject

object PUNCHING_BAG
    parents THING;

    home = WESTERN_ROOM;
    name = "a punching bag";
    desc = "A punching bag with a picture of Brian Mulroney taped to it.";

    verb 	"hit punch smack whomp sock" = hit;

    method init
	this.moveto(home);
	this.add_owner(WIZARD);
    endmethod

    method sdesc
	return ("There is " + name + " here, just waiting to be hit.");
    endmethod

    method hit
	if (!this.match(args[2]))
	    return 1;
	endif
	player.tell("You sock the punching bag on da nose!  How satisfying.");
	location.announce(player.name +
			     " socks the punching bag on da nose!", {player});
	at (time() + 5)
	    location.announce("The punching bag winds up..", {});
	endat
	at (time() + 10)
	    if (player.location == location)
		player.tell("The punching bag whomps you on da shnozz!");
		location.announce("The punching bag whomps "
			    + player.name + " on da shnozz!", {player});
	    else
		location.announce("The punching bag appears confused, as "
				+ player.name + " has left the room.", {});
	    endif
	endat
    endmethod

endobject

object PAPER_BAG
    parents CONTAINER;

    name = "a brown paper bag";
    desc = "A very ordinary-looking brown paper bag.";
    home = WESTERN_ROOM;

    method init
	this.add_owner(WIZARD);
	this.moveto(home);
    endmethod	/* init */
endobject /* PAPER_BAG */
