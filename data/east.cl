#include "common.clh"
#include "boot.clh"

object EASTERN_ROOM
    parents ROOM;

    name = "Eastern Room";
    desc = "You hear the chime of the sitar and realize you are in the most excellent Eastern Room.";
    exits = { EXIT_WEST };

    method init
	this.add_owner(WIZARD);
    endmethod
endobject

object EXIT_WEST
    parents EXIT;

    method init
	name = "w;west";
	leave = "You saunter west.";
	oleave = "%n saunters west.";
	oarrive = "%n comes in from the east.";
	source = EASTERN_ROOM;
	dest = WESTERN_ROOM;
	this.add_owner(WIZARD);
    endmethod
endobject

object HAMMOCK
    parents	ROOM, CONTAINER;

    str		outside_desc;		/* what people see from outside */

    verb "enter go hammock" = enter_verb;
    verb "out back exit leave" = exit_verb;

    method init
	name = "In The Hammock";
	desc = "You're nestled snugly a big, warm hammock.  There is comfy material surrounding you on all sides, through which you still make out what's going on outside.";
	outside_desc = "It's all warm and cozy-looking.";
	pass() to THING;
	this.add_owner(WIZARD);
	this.moveto(EASTERN_ROOM);
    endmethod

    method enter_verb
	if (args[1] != "hammock" && !this.match(args[2]))
	    return 1;
	elseif (player.location != location)
	    return 1;
	elseif (!player.moveto(this, "You enter the hammock.",
		"%n clambers into the hammock.", "", "%n enters the hammock."))
	    player.tell("You can't enter the hammock.");
	endif
    endmethod

    method exit_verb
	if (player.location != this)
	    return 1;
	elseif (!player.moveto(location, "You leave the comfy hammock.",
	    "%n clambers out of the hammock.", "",
	    "%n clambers out of the hammock, blinking in the bright light."));
	    player.tell("You can't leave!");
	endif
    endmethod

    method sdesc
	return "You see a warm, comfy hammock here.";
    endmethod

    method match
	return pass(args[1]) to THING;
    endmethod

    method look
	if (player.location == this)
	    pass() to ROOM;
	elseif (outside_desc)
	    player.tell(outside_desc);
	else
	    player.tell("You see nothing special.");
	endif
    endmethod

    method tell
	  /* echo all room msgs to hammock's contents */
	this.announce("Outside:  " + args[1]);
    endmethod
endobject
