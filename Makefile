all:
	gcc -Wall -Wextra -I ./include ./deps/ReMem/ReMem.c ./deps/ReMem/arena/arena.c ./src/*.c -o lexi-lang

clean:
	rm lexi-lang
