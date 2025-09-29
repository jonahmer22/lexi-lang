#ifndef MAIN_H
#define MAIN_H

// entirely useless for right now, might get added back later
#include "../deps/ReMem/ReMem.h"

#include <stdint.h>
#include <stddef.h>

// i am aware that this is not an accurate name for what this is, i just can't think of a better one
#define BITSIZE uint16_t	// it is not recomended to increase this above 16 bit, in the initial version this does not shrink or grow with size, arrays are made on the stack at the full range size of this type
#define MAXSIZE 65536

// opcodes that bytecode will consist of
typedef enum Opcode{
	OP_MOV = 0,	// takes in 2 arguements, dest_reg <- source_reg or immd_value
	OP_LD,		// takes in 2 arguements, dest_reg <- source_addr as an memory address
	OP_ST,		// takes in 2 arguements, source_reg -> dest_reg the order here is opposite normal
	OP_PUSH,	// takes in 1 arguement, source_reg where the value being added to the stack is stored
	OP_POP,		// takes in 1 arguement, dest_reg where the value form the top of the stack will go
	OP_ADD,		// takes in 1 arguement, source_reg which the accumulator will be incemented by
	OP_SUB,		// takes in 1 arguement, source_reg which the accumulator will be decremented by
	OP_MUL,		// takes in 1 arguement, source_reg which the accumulator will be multiplied by
	OP_DIV,		// takes in 1 arguement, source_reg which the accumulator will be divided by
	OP_INC,		// no arguements, only increments accumulator
	OP_DEC,		// no arguements, only decrements accumulator
	OP_CLR,		// no arguements, only sets accumulator to 0
	OP_AND,		// takes in 1 arguement, source_reg which the accumulator will be bitwise "anded" with
	OP_OR,		// takes in 1 arguement, source_reg which the accumulator will be bitwise "ored" with
	OP_XOR,		// takes in 1 arguement, source_reg which the accumulator will be bitwise "xored" with
	OP_NOT,		// no arguements, only bitwise "nots" the accumulator
	OP_JMP,		// takes in 1 arguement, label which will be unconditionally jumped to
	OP_JEZ,		// takes in 1 arguement, label which will be jumped to if accumulator = 0
	OP_JLZ,		// takes in 1 arguement, label which will be jumped to if accumulator < 0
	OP_JGZ,		// takes in 1 arguement, label which will be jumped to if accumulator > 0
	OP_PRN,		// takes in 1 arguement, source_reg which will be moved to [0xFF00] and be printed
	OP_HLT,		// no arguements, only halts the cpu no way to undo this so just use it to exit
	OP_NOP		// no arguements, what do you want me to tell you it just does nothing
} Opcode;

// registers will be stored as a value of this enum
typedef enum Registers{
	REG_0 = 0,	// first of 8 general purpose registers
	REG_1,		// second
	REG_2,		// third
	REG_3,		// fourth
	REG_4,		// fifth
	REG_5,		// sixth
	REG_6,		// seventh
	REG_7,		// eigth, the sizes of these registers and data cells in memory are determined by above macro
	REG_SP,		// stack pointer, points to the top of the stack
	REG_PC,		// program counter, current index in bytecode "NOTE" this register is still BITSIZE so program size may be limited
	REG_ACC		// accumulator, used for arithmatic operations and as a "result" register
} Registers;

#endif
