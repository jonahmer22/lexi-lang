#ifndef VM_H
#define VM_H

#include "main.h"

// forward declarations
typedef struct Bytecode Bytecode;

typedef struct VM{
	Bytecode *bytecode;

	BITSIZE registers[REG_ACC + 1];
	BITSIZE memory[MAXSIZE];
	
	size_t stackCount;
	int running;
} VM;

int vmRun(Bytecode *bytecode);

#endif
