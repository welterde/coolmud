/* Copyright (c) 1993 Stephen F. White */

#include <stdio.h>

#include "config.h"
#include "cool.h"
#include "proto.h"
#include "sys_proto.h"
#include "code.h"

Inst	*machine;
int	 progi, machine_size;

int
code(int inst)
{
    Inst	*new_machine;
    int		 i;

    if (!machine || !machine_size) {
	code_init();
    }
    if (progi >= machine_size) {
	new_machine = MALLOC(Inst, machine_size *= 2);
	for (i = 0; i < progi; i++) {
	    new_machine[i] = machine[i];
	}
	FREE(machine);
	machine = new_machine;
    }
    machine[progi++] = inst;
    return progi - 1;
}

void
code_copy(Method *m)
{
    int		 i;

    if (progi) {
	m->code = MALLOC(Inst, progi);
	for (i = 0; i < progi; i++) {
	    m->code[i] = machine[i];
	}
    } else {
	m->code = 0;
    }
    m->ninst = progi;
    if (machine) {
	FREE(machine);
    }
    machine_size = 0;
    machine = 0;
    progi = 0;
}

void
code_init(void)
{
    machine = MALLOC(Inst, machine_size = CODE_INIT_SIZE);
    progi = 0;
}
