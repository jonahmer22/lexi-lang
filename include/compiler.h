#ifndef COMPILER_H
#define COMPILER_H

#include "main.h"

// forward declarations
typedef struct Token Token;

typedef struct Bytecode{
	BITSIZE code[MAXSIZE];

	BITSIZE codeLen;
	size_t maxSize;	// used to store only MAXSIZE, this can be used to retrieve the BITSIZE if running from an output binary file in the future
} Bytecode;

Bytecode *compiler(Token *tokens);

#endif
