all:
	gcc -Wall -Wextra -I ./include/ ./src/* -o lexi-lang

clean:
	rm lexi-lang
