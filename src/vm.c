#include "vm.h"
#include "compiler.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPERAND_NONE 0x1F
#define OPERAND_IMMEDIATE 0x1E
#define OPCODE_SHIFT 10
#define DEST_SHIFT 5
#define FIELD_MASK 0x1F
#define IO_PORT 0xFF00

// reports errors to the console and exits, takes in dynamic amount of args which shows args
static void vmError(const char *fmt, ...){
	// init the dynamic args list
	va_list args;
	va_start(args, fmt);

	// print error
	fprintf(stderr, "[VM]: ");
	vfprintf(stderr, fmt, args);
	fputs("\n", stderr);

	// end the dynamic array and exit
	va_end(args);
	exit(68);
}

// fetches a vm word (16 bit value) from the bytecode based on the PC
static inline uint16_t fetchWord(VM *vm){
	if((size_t)vm->registers[REG_PC] >= vm->bytecode->codeLen){
		vmError("Unexpected end of bytecode");	// if trying to fetch another word but hit end
	}

	// get the value from bytecode and increment the PC
	uint16_t value = vm->bytecode->code[vm->registers[REG_PC]];
	vm->registers[REG_PC] = (BITSIZE)((size_t)vm->registers[REG_PC] + 1);

	return value;
}

// returns the address of the value in a register
static inline BITSIZE *requireRegister(VM *vm, int field){
	if(field < 0 || field > REG_ACC){	// checks the index of the register (field) is valid
		vmError("Invalid register index %d", field);
	}

	// return the address of the value in that register
	return &vm->registers[field];
}

// gets a value from the bytecode
static inline uint16_t fetchImmediate(VM *vm){
	return fetchWord(vm);
}

// turns a value from BITSIZE (should be uint) to a signed 16 bit int
static inline int16_t toSigned(BITSIZE value){
	return (int16_t)value;
}

// turns a value from signed int to unsigned 16 bit number
static inline uint16_t toUnsigned(int32_t value){
	return (uint16_t)(value & 0xFFFF);
}

// MOV opcode used to move between registers
static void execMove(VM *vm, int destField, int srcField){
	BITSIZE *dest = requireRegister(vm, destField);	// get the address of the destination registers value

	if(srcField == OPERAND_IMMEDIATE){	// if we have an immediate value
		*dest = fetchImmediate(vm);	// set the destinations value to the value in bytecode
	}
	else{	// regular register MOV
		BITSIZE *src = requireRegister(vm, srcField);	// get address of source
		*dest = *src;	// set the value of dest to the value of src
	}
}

// LD opcode used to get a value from a value in memory
static void execLoad(VM *vm, int destField, int srcField){
	if(srcField != OPERAND_IMMEDIATE){	// if there is no address
		vmError("LD expects an immediate address");
	}

	// fetch the address of the dest register and address from bytecode
	BITSIZE *dest = requireRegister(vm, destField);
	uint16_t addr = fetchImmediate(vm);

	// set the value of the register as the value in memory
	*dest = vm->memory[addr];
}

// ST opcode used to store values in memory
static void execStore(VM *vm, int regField, int srcField){
	if(srcField != OPERAND_IMMEDIATE){	// need an address to store at
		vmError("ST expects an immediate address");
	}

	// get the register and address
	BITSIZE *reg = requireRegister(vm, regField);
	uint16_t addr = fetchImmediate(vm);

	// store the value in the address
	vm->memory[addr] = *reg;

	// if we put a value at a designated IO port (only one rn is 0xFF00 for printing)
	if(addr == IO_PORT){
		putchar((int)(*reg & 0xFF));	// print the value at that address
		fflush(stdout);
	}
}

// PUSH opcode used to push a value onto the stack
static void execPush(VM *vm, int regField){
	BITSIZE *reg = requireRegister(vm, regField);	// get the source register address
	
	// STACK OVERFLOW REFERENCE :O
	if(vm->stackCount >= MAXSIZE){
		vmError("Stack overflow");
	}

	// set the value in the stack at the SP register
	vm->registers[REG_SP] = (BITSIZE)((size_t)vm->registers[REG_SP] - 1);
	vm->memory[vm->registers[REG_SP]] = *reg;	// stack is stored at the top of vm memory

	// increment stack count
	vm->stackCount++;
}

// POP opcode used to get values from the stack
static void execPop(VM *vm, int regField){
	if(vm->stackCount == 0){	// if stack is empty
		vmError("Stack underflow");
	}

	// get the value from the stack
	BITSIZE value = vm->memory[vm->registers[REG_SP]];
	BITSIZE *dest = requireRegister(vm, regField);	// get the address of the register the value is going to 

	*dest = value;	// put the value in the register

	// move the stackpointer and decrement stackCount
	vm->registers[REG_SP] = (BITSIZE)((size_t)vm->registers[REG_SP] + 1);
	vm->stackCount--;
}

// This is an accumulation of opcodes: ADD SUB MUL DIV AND OR XOR
static void execArithmetic(VM *vm, Opcode opcode, int regField){
	BITSIZE *operand = requireRegister(vm, regField);	// get the address of register
	int32_t acc = toSigned(vm->registers[REG_ACC]);	// get the value in ACC
	int32_t value = toSigned(*operand);	// convert the value from the register to signed

	// Opcode switch
	switch(opcode){
		case OP_ADD:
			acc += value;
			break;
		case OP_SUB:
			acc -= value;
			break;
		case OP_MUL:
			acc *= value;
			break;
		case OP_DIV:
			if(*operand == 0){	// prevent division by 0
				vmError("Division by zero");
			}
			acc /= value;
			break;
		case OP_AND:
			acc = (int32_t)((uint16_t)vm->registers[REG_ACC] & (uint16_t)*operand);
			break;
		case OP_OR:
			acc = (int32_t)((uint16_t)vm->registers[REG_ACC] | (uint16_t)*operand);
			break;
		case OP_XOR:
			acc = (int32_t)((uint16_t)vm->registers[REG_ACC] ^ (uint16_t)*operand);
			break;
		default:
			vmError("Unsupported arithmetic opcode");	// if the opcode is something that it shouldn't be
	}
	vm->registers[REG_ACC] = toUnsigned(acc);	// move the value result back into ACC
}

// Collection of all jump opcodes: JMP JLZ JEZ JGZ
static void execJump(VM *vm, Opcode opcode, int destField){
	if(destField != OPERAND_IMMEDIATE){	// jump has to have a destination
		vmError("Jump missing immediate target");
	}

	uint16_t target = fetchImmediate(vm);	// get the value from bytecode of where to jump
	int16_t acc = toSigned(vm->registers[REG_ACC]);	// get the value in ACC to test against 0
	bool shouldJump = false;
	switch(opcode){
		case OP_JMP:	// unconditional
			shouldJump = true;
			break;
		case OP_JEZ:
			shouldJump = (vm->registers[REG_ACC] == 0);
			break;
		case OP_JLZ:
			shouldJump = (acc < 0);
			break;
		case OP_JGZ:
			shouldJump = (acc > 0);
			break;
		default:
			vmError("Invalid jump opcode");	// if the opcode matches nothing
	}

	// if we should jump then move the PC to the target
	if(shouldJump){
		if(target >= vm->bytecode->codeLen){	// make sure in range
			vmError("Jump target out of range: %u", target);
		}
		vm->registers[REG_PC] = target;
	}
}

// main run function that starts VM execution
int vmRun(Bytecode *bytecode){
	if(bytecode == NULL){	// must have bytecode
		return -1;
	}

	// make the VM object and set all of the values
	VM vm;
	memset(&vm, 0, sizeof(VM));
	vm.bytecode = bytecode;
	vm.running = 1;
	vm.registers[REG_PC] = 0;
	vm.registers[REG_SP] = 0;
	vm.registers[REG_ACC] = 0;
	memset(vm.memory, 0, sizeof(vm.memory));

	// main execution loop
	while(vm.running){
		if((size_t)vm.registers[REG_PC] >= vm.bytecode->codeLen){
			break;	// make sure the PC does not go out of bounds
		}

		uint16_t word = fetchWord(&vm);	// get the next value from bytecode
		Opcode opcode = (Opcode)((word >> OPCODE_SHIFT) & 0x3F);	// mask off the opcode
		int destField = (int)((word >> DEST_SHIFT) & FIELD_MASK);	// mask off and store destination
		int srcField = (int)(word & FIELD_MASK);	// mask off and store source

		// main switch
		switch(opcode){
			case OP_MOV:
				execMove(&vm, destField, srcField);
				break;
			case OP_LD:
				execLoad(&vm, destField, srcField);
				break;
			case OP_ST:
				execStore(&vm, destField, srcField);
				break;
			case OP_PUSH:
				execPush(&vm, destField);
				break;
			case OP_POP:
				execPop(&vm, destField);
				break;
			case OP_ADD:
			case OP_SUB:
			case OP_MUL:
			case OP_DIV:
			case OP_AND:
			case OP_OR:
			case OP_XOR:
				execArithmetic(&vm, opcode, destField);
				break;
			case OP_INC:
				vm.registers[REG_ACC] = toUnsigned(toSigned(vm.registers[REG_ACC]) + 1);
				break;
			case OP_DEC:
				vm.registers[REG_ACC] = toUnsigned(toSigned(vm.registers[REG_ACC]) - 1);
				break;
			case OP_CLR:
				vm.registers[REG_ACC] = 0;
				break;
			case OP_NOT:
				vm.registers[REG_ACC] =(BITSIZE)(~vm.registers[REG_ACC]);
				break;
			case OP_JMP:
			case OP_JEZ:
			case OP_JLZ:
			case OP_JGZ:
				execJump(&vm, opcode, destField);
				break;
			case OP_PRN:
				putchar((int)(vm.registers[REG_ACC] & 0xFF));
				fflush(stdout);
				break;
			case OP_HLT:
				vm.running = 0;
				break;
			case OP_NOP:
				break;
			default:	// if the opcode is non existent then exit
				vmError("Unknown opcode %d", opcode);
		}
	}
	
	return 0;
}
