#include "compiler.h"
#include "interpreter.h"
#include "main.h"
#include "parser.h"

#include <stdio.h>
#include <stdbool.h>

int main(int argc, char **argv){
	int stacktop_hint;
	gcInit(&stacktop_hint, false);
	// Code Below this point
	
	// Check for input
	if(argc > 1){
		for(int i = 0; i < argc; i++){
			printf("%s ", argv[i]);
		}
		printf("\n");
	}

	printf("this compiles and runs\n");

	// Code Above this point
	gcDestroy();
	return 0;
}
