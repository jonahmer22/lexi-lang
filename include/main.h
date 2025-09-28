#ifndef MAIN_H
#define MAIN_H

#include "../deps/ReMem/ReMem.h"

typedef enum Opcode{
	OP_MOV = 0,
	OP_LD,
	OP_ST,
	OP_PUSH,
	OP_POP,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_INC,
	OP_DEC,
	OP_CLR,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_NOT,
	OP_JMP,
	OP_JEZ,
	OP_JLZ,
	OP_JGZ,
	OP_PRN,
	OP_HLT,
	OP_NOP
} Opcode;

typedef enum Registers{
	REG_0 = 0,
	REG_1,
	REG_2,
	REG_3,
	REG_4,
	REG_5,
	REG_6,
	REG_7,
	REG_SP,
	REG_PC,
	REG_ACC
} Registers;

#endif
