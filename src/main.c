#include "main.h"

#include <stdio.h>
#include <stdbool.h>

int main(int argc, char **argv){
	int stacktop_hint;
	gcInit(&stacktop_hint, false);
	// Code Below this point
	
	printf("this compiles and runs\n");

	// Code Above this point
	gcDestroy();
	return 0;
}
