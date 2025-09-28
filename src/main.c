#include "compiler.h"
#include "interpreter.h"
#include "main.h"
#include "parser.h"

#include <stdio.h>
#include <stdbool.h>

int main(int argc, char **argv){
	// int stacktop_hint;
	// gcInit(&stacktop_hint, false);
	// Code Below this point
	
	// start of execution
	// in future different modes can be added based on arguements
	// 	- "-v" for visualization of cpu state
	// 	- no args for a REPL like thing
	// 	- more args for multiple files
	switch(argc){	// based on input
		case 2:	// if we have an arguement
			printf("Running the file \"%s\"\n", argv[1]);
			
			// need to execute parser
			Token *tokenStream = parser(argv[1]);
			
			// need to execute compiler from output of parser
			Bytecode *bytecode = compiler(tokenStream);

			// need to execute interpreter on bytecode from compiler
			interpreter(bytecode);

			break;
		default:
			printf("Usage: ./lexi-lang <source_file>\n");
			break;
	}

	// Code Above this point
	// gcDestroy();	// for right now the GC is entirely useless might get added back later
	return 0;
}
