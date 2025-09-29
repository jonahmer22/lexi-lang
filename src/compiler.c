#include "compiler.h"
#include "main.h"
#include "parser.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPERAND_NONE 0x1F
#define OPERAND_IMMEDIATE 0x1E
#define OPCODE_SHIFT 10
#define DEST_SHIFT 5
#define FIELD_MASK 0x1F

// table and entry for storing labels while compiling
typedef struct{
	char *name;
	size_t address;
	size_t line;
} LabelEntry;

typedef struct{
	LabelEntry *items;
	size_t count;
	size_t capacity;
} LabelTable;

// table and entry for storing patches while compiling
typedef struct{
	char *name;
	size_t index;
	size_t line;
} PatchEntry;

typedef struct{
	PatchEntry *items;
	size_t count;
	size_t capacity;
} PatchTable;

// for reporting compiling errors takes in list of arguements at end for string formatting
static void compilerError(size_t line, const char *fmt, ...){
	// initialize the dynamic args list
	va_list args;
	va_start(args, fmt);

	// print the error
	fprintf(stderr, "[Compiler][Line %zu]: ", line);
	vfprintf(stderr, fmt, args);
	fputs("\n", stderr);

	// cleanup and exit
	va_end(args);
	exit(66);
}

// tests that the table for labels has enough space for a needed amount of entries
static void ensureLabelCapacity(LabelTable *table, size_t needed){
	if(table->capacity >= needed){
		return;	// there already is enough space
	}

	// must not have enough space therefore calculate the space we need to make, with a base of 8
	size_t newCapacity = table->capacity == 0 ? 8 : table->capacity * 2;
	while(newCapacity < needed){
		newCapacity *= 2;
	}

	// copy over values from old one to new one
	LabelEntry *oldItems = table->items;
	LabelEntry *newItems = gcAlloc(sizeof(LabelEntry) * newCapacity);
	if(oldItems != NULL && table->count > 0){
		memcpy(newItems, oldItems, table->count * sizeof(LabelEntry));
	}

	// assign values
	table->items = newItems;
	table->capacity = newCapacity;
}

// makes sure the patch table is big enough
static void ensurePatchCapacity(PatchTable *table, size_t needed){
	if(table->capacity >= needed){
		return;	// already big enough
	}

	// calculate the new size with a base of 8
	size_t newCapacity = table->capacity == 0 ? 8 : table->capacity * 2;
	while(newCapacity < needed){
		newCapacity *= 2;
	}

	// copy over values into a new array
	PatchEntry *oldItems = table->items;
	PatchEntry *newItems = gcAlloc(sizeof(PatchEntry) * newCapacity);
	if(oldItems != NULL && table->count > 0){
		memcpy(newItems, oldItems, table->count * sizeof(PatchEntry));
	}

	// assign values
	table->items = newItems;
	table->capacity = newCapacity;
}

// copies a char array as all uppercase
static char *uppercaseCopy(const char *lexeme, size_t start, size_t end){
	size_t len = end > start ? end - start : 0;	// check bounds are valid
	char *result = gcAlloc(len + 1);	// allocate a result array

	// copy over the values in the array as uppercase
	for(size_t i = 0; i < len; i++){
		result[i] = (char)toupper((unsigned char)lexeme[start + i]);
	}
	result[len] = '\0';	// add EOF to end

	return result;
}

// extracts the label name from a token
static char *copyLabelName(const Token *token, int isDefinition){
	// get data out of token about the label
	const char *lexeme = token->start;
	size_t len = token->len;
	size_t begin = 0;
	size_t end = len;

	// if it's a definition it must have a '@'
	if(isDefinition){
		if(len < 2){	// a label must be at least an @ and a char
			compilerError(token->line, "Invalid label declaration");
		}
		begin = 1;	// skip '@'
		if(len > 0 && lexeme[len - 1] == ':'){
			end = len - 1;
		}
	}

	return uppercaseCopy(lexeme, begin, end);
}

// adds a label to the table
static void addLabel(LabelTable *table, char *name, size_t address, size_t line){
	for(size_t i = 0; i < table->count; i++){	// for every stored label
		if(strcmp(table->items[i].name, name) == 0){	// if the label is the same as the one we want to make
			compilerError(line, "Duplicate label '%s'", name);	// can't have duplicates
		}
	}

	// make sure we have enough space
	ensureLabelCapacity(table, table->count + 1);

	// assign values
	table->items[table->count].name = name;
	table->items[table->count].address = address;
	table->items[table->count].line = line;

	table->count++;
}

// finds a label in the table
static int findLabel(const LabelTable *table, const char *name, size_t *addressOut){
	for(size_t i = 0; i < table->count; i++){	// for every label
		if(strcmp(table->items[i].name, name) == 0){	// if the label is the one we are looking for
			if(addressOut != NULL){	// if we want to return the address to be used by caller
				*addressOut = table->items[i].address;
			}

			return 1;	// true
		}
	}

	return 0;	// false
}

// stores a patch in the table
static void recordPatch(PatchTable *patches, char *name, size_t index, size_t line){
	ensurePatchCapacity(patches, patches->count + 1);	// make sure we have the space

	// assign values
	patches->items[patches->count].name = name;
	patches->items[patches->count].index = index;
	patches->items[patches->count].line = line;

	patches->count++;
}

static int encodeWord(Opcode opcode, int destField, int srcField){
	return ((int)opcode << OPCODE_SHIFT) | ((destField & FIELD_MASK) << DEST_SHIFT) | (srcField & FIELD_MASK);
}

// gets register from name in token
static int parseRegister(const Token *token){
	char buffer[8];
	size_t len = token->len;

	// if the register name is to long (shouldn'e be)
	if(len >= sizeof(buffer)){
		compilerError(token->line, "Invalid register name '%s'", token->start);
	}

	// convert to uppercase
	for(size_t i = 0; i < len; i++){
		buffer[i] = (char)toupper((unsigned char)token->start[i]);
	}
	buffer[len] = '\0';	// add back EOF

	// r0 - r7 must start with r and be 2 long
	if(buffer[0] == 'R' && len == 2 && buffer[1] >= '0' && buffer[1] <= '7'){
		return buffer[1] - '0';
	}

	// special registers
	if(strcmp(buffer, "ACC") == 0){
		return REG_ACC;
	}
	if(strcmp(buffer, "SP") == 0){
		return REG_SP;
	}
	if(strcmp(buffer, "PC") == 0){
		return REG_PC;
	}

	// shouldn't get here
	compilerError(token->line, "Unknown register '%s'", token->start);
	return -1;
}

// extract the value out of an immediate
static int32_t parseImmediate(const Token *token){
	// get the string from the token and skip immediate char
	const char *lexeme = token->start;
	const char *cursor = lexeme;
	if(*cursor == '#'){
		cursor++;	// skip immediate char
	}
	char *endptr = NULL;
	int base = 0;

	if(cursor[0] == '0' &&(cursor[1] == 'x' || cursor[1] == 'X')){
		base = 16;	// if the value is a hex then base 16 instead of 10
	}
	long value = strtol(cursor, &endptr, base);

	// if the token is not formatted correctly
	if(endptr == cursor || *endptr != '\0'){
		compilerError(token->line, "Invalid immediate literal '%s'", lexeme);
	}

	// must be a valid number
	if(value < -32768L || value > 0xFFFFL){
		compilerError(token->line, "Immediate literal '%s' out of range", lexeme);
	}

	return (int32_t)value;
}

// extract the address from a token
static uint16_t parseAddress(const Token *token){
	const char *lexeme = token->start;

	// must begin and end with []
	if(token->len < 3 || lexeme[0] != '[' || lexeme[token->len - 1] != ']'){
		compilerError(token->line, "Invalid memory address '%s'", lexeme);
	}

	// copy the value inside the []
	char *copy = gcAlloc(token->len - 1);
	memcpy(copy, lexeme + 1, token->len - 2);
	copy[token->len - 2] = '\0';
	char *endptr = NULL;
	int base = 0;

	// if the value is in hex
	if(copy[0] == '0' &&(copy[1] == 'x' || copy[1] == 'X')){
		base = 16;	// hex is base 16
	}
	long value = strtol(copy, &endptr, base);

	// if invalid token
	if(endptr == copy || *endptr != '\0'){
		compilerError(token->line, "Invalid memory address '%s'", lexeme);
	}
	if(value < 0 || value >= MAXSIZE){	// value must be within range
		compilerError(token->line, "Memory address '%s' out of range", lexeme);
	}

	return(uint16_t)value;
}

// parses opcode from token
static Opcode parseOpcode(const Token *token){
	char buffer[8];
	size_t len = token->len;
	if(len >= sizeof(buffer)){	// opcode shouldn't be over 8 characters long
		compilerError(token->line, "Invalid opcode '%s'", token->start);
	}

	// get the string out of the token
	for(size_t i = 0; i < len; i++){
		buffer[i] = (char)toupper((unsigned char)token->start[i]);
	}
	buffer[len] = '\0';

	// TODO: switch to a switch instead
	if(strcmp(buffer, "MOV") == 0) return OP_MOV;
	if(strcmp(buffer, "LD") == 0) return OP_LD;
	if(strcmp(buffer, "ST") == 0) return OP_ST;
	if(strcmp(buffer, "PUSH") == 0) return OP_PUSH;
	if(strcmp(buffer, "POP") == 0) return OP_POP;
	if(strcmp(buffer, "ADD") == 0) return OP_ADD;
	if(strcmp(buffer, "SUB") == 0) return OP_SUB;
	if(strcmp(buffer, "MUL") == 0) return OP_MUL;
	if(strcmp(buffer, "DIV") == 0) return OP_DIV;
	if(strcmp(buffer, "INC") == 0) return OP_INC;
	if(strcmp(buffer, "DEC") == 0) return OP_DEC;
	if(strcmp(buffer, "CLR") == 0) return OP_CLR;
	if(strcmp(buffer, "AND") == 0) return OP_AND;
	if(strcmp(buffer, "OR") == 0) return OP_OR;
	if(strcmp(buffer, "XOR") == 0) return OP_XOR;
	if(strcmp(buffer, "NOT") == 0) return OP_NOT;
	if(strcmp(buffer, "JMP") == 0) return OP_JMP;
	if(strcmp(buffer, "JEZ") == 0) return OP_JEZ;
	if(strcmp(buffer, "JLZ") == 0) return OP_JLZ;
	if(strcmp(buffer, "JGZ") == 0) return OP_JGZ;
	if(strcmp(buffer, "PRN") == 0) return OP_PRN;
	if(strcmp(buffer, "HLT") == 0) return OP_HLT;
	if(strcmp(buffer, "NOP") == 0) return OP_NOP;

	// shouldn't get here
	compilerError(token->line, "Unknown opcode '%s'", token->start);
	return OP_NOP;
}

// emits a word into bytecode
static void emitWord(Bytecode *bytecode, uint16_t value){
	if(bytecode->codeLen >= MAXSIZE){	// must be within size accessable by PC
		fprintf(stderr, "[Compiler]: Bytecode size exceeds maximum of %d words\n", MAXSIZE);
		exit(67);
	}

	bytecode->code[bytecode->codeLen++] = value;
}

// compiles a word based on a token
static void compileInstruction(const Token *opToken, Token **operands, size_t operandCount, Bytecode *bytecode, PatchTable *patches, const LabelTable *labels){
	// get the opcode and line
	Opcode opcode = parseOpcode(opToken);
	size_t line = opToken->line;

	// execute based on opcode
	switch(opcode){
		case OP_MOV:{
			if(operandCount != 2){
				compilerError(line, "MOV expects 2 operands");
			}
			if(operands[0]->type != TOKEN_REG){
				compilerError(operands[0]->line, "MOV destination must be a register");
			}

			int destReg = parseRegister(operands[0]);
			if(operands[1]->type == TOKEN_REG){
				int srcReg = parseRegister(operands[1]);
				emitWord(bytecode,(uint16_t)encodeWord(opcode, destReg, srcReg));
			}
			else if(operands[1]->type == TOKEN_IMMD){
				int32_t value = parseImmediate(operands[1]);
				emitWord(bytecode,(uint16_t)encodeWord(opcode, destReg, OPERAND_IMMEDIATE));
				emitWord(bytecode,(uint16_t)(value & 0xFFFF));
			}
			else{
				compilerError(operands[1]->line, "MOV source must be register or immediate");
			}

			break;
		}
		case OP_LD:{
			if(operandCount != 2){
				compilerError(line, "LD expects 2 operands");
			}
			if(operands[0]->type != TOKEN_REG || operands[1]->type != TOKEN_ADDR){
				compilerError(line, "LD syntax is 'LD <reg>, [addr]'");
			}

			int destReg = parseRegister(operands[0]);
			uint16_t addr = parseAddress(operands[1]);
			emitWord(bytecode,(uint16_t)encodeWord(opcode, destReg, OPERAND_IMMEDIATE));
			emitWord(bytecode, addr);

			break;
		}
		case OP_ST:{
			if(operandCount != 2){
				compilerError(line, "ST expects 2 operands");
			}
			if(operands[0]->type != TOKEN_REG || operands[1]->type != TOKEN_ADDR){
				compilerError(line, "ST syntax is 'ST <reg>, [addr]'");
			}

			int srcReg = parseRegister(operands[0]);
			uint16_t addr = parseAddress(operands[1]);
			emitWord(bytecode,(uint16_t)encodeWord(opcode, srcReg, OPERAND_IMMEDIATE));
			emitWord(bytecode, addr);

			break;
		}
		case OP_PUSH:
		case OP_POP:
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
		case OP_AND:
		case OP_OR:
		case OP_XOR:
		case OP_PRN:{
			if(operandCount != 1){
				compilerError(line, "Instruction expects 1 operand");
			}
			if(operands[0]->type != TOKEN_REG){
				compilerError(operands[0]->line, "Operand must be a register");
			}

			int reg = parseRegister(operands[0]);
			if(opcode == OP_PRN && reg != REG_ACC){
				compilerError(operands[0]->line, "PRN expects ACC register");
			}
			emitWord(bytecode,(uint16_t)encodeWord(opcode, reg, OPERAND_NONE));

			break;
		}
		case OP_INC:
		case OP_DEC:
		case OP_CLR:
		case OP_NOT:
		case OP_HLT:
		case OP_NOP:{
			if(operandCount != 0){
				compilerError(line, "Instruction does not take operands");
			}
			
			emitWord(bytecode,(uint16_t)encodeWord(opcode, OPERAND_NONE, OPERAND_NONE));

			break;
		}
		case OP_JMP:
		case OP_JEZ:
		case OP_JLZ:
		case OP_JGZ:{
			if(operandCount != 1){
				compilerError(line, "Jump instruction expects 1 operand");
			}
			if(operands[0]->type != TOKEN_LABEL){
				compilerError(operands[0]->line, "Jump target must be a label");
			}

			char *labelName = copyLabelName(operands[0], 0);
			emitWord(bytecode,(uint16_t)encodeWord(opcode, OPERAND_IMMEDIATE, OPERAND_NONE));
			size_t patchIndex = bytecode->codeLen;
			emitWord(bytecode, 0);

			recordPatch(patches, labelName, patchIndex, operands[0]->line);

			break;
		}
		default:
			compilerError(line, "Unhandled opcode");
	}
	(void)labels;	// currently not used directly inside switch
}

// 
static void patchLabels(Bytecode *bytecode, const LabelTable *labels, const PatchTable *patches){
	for(size_t i = 0; i < patches->count; i++){	// for every patch
		const PatchEntry *patch = &patches->items[i];
		size_t address = 0;

		if(!findLabel(labels, patch->name, &address)){	// if we can't find a label for the patch
			compilerError(patch->line, "Undefined label '%s'", patch->name);
		}
		if(address >= MAXSIZE){	// if the address is out of range
			compilerError(patch->line, "Label '%s' address out of range", patch->name);
		}

		// insert the address into bytecode
		bytecode->code[patch->index] = (uint16_t)address;
	}
}

// main compiler function compiles bytecode based off of a stream of tokens
Bytecode *compiler(Token *tokens){
	if(tokens == NULL){
		return NULL;	// need to have tokens to compile
	}

	// initialize bytecode
	Bytecode *bytecode = gcAlloc(sizeof(Bytecode));
	bytecode->codeLen = 0;
	bytecode->maxSize = MAXSIZE;

	// make empty label and patches
	LabelTable labels = {0};
	PatchTable patches = {0};

	// while we have tokens until the TOKEN_END
	size_t index = 0;
	while(tokens[index].type != TOKEN_END){
		size_t line = tokens[index].line;

		// handle label declarations that start the line
		while(tokens[index].type == TOKEN_LABEL && tokens[index].start[0] == '@' && tokens[index].line == line){
			char *labelName = copyLabelName(&tokens[index], 1);
			addLabel(&labels, labelName, bytecode->codeLen, tokens[index].line);
			index++;

			if(tokens[index].type == TOKEN_END){
				break;	// we reached the end
			}
		}
		if(tokens[index].type == TOKEN_END){
			break;	// reached the end
		}
		if(tokens[index].line != line){
			continue;	// if we aren't on the right line move onto next loop iteration
		}
		if(tokens[index].type != TOKEN_OP){
			compilerError(tokens[index].line, "Unexpected token '%s'", tokens[index].start);
		}
		Token *opToken = &tokens[index];
		index++;

		Token *operands[3];
		size_t operandCount = 0;
		while(tokens[index].type != TOKEN_END && tokens[index].line == line){
			if(operandCount >= 3){	// make sure each line has no more than 3 operands or arguements
				compilerError(tokens[index].line, "Too many operands");
			}

			operands[operandCount++] = &tokens[index];
			index++;
		}

		// compile the instruction
		compileInstruction(opToken, operands, operandCount, bytecode, &patches, &labels);
	}

	// go back and patch through all labels inserting the correct addresses
	patchLabels(bytecode, &labels, &patches);

	// return the final bytecode
	return bytecode;
}
