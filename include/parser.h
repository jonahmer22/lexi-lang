#ifndef PARSER_H
#define PARSER_H

typedef enum TokenType{
	TOKEN_OP = 0,	// first token on line unless no tokens match and followed by `:` then it's a label
	TOKEN_REG,	// must start with `R` or `r` followed by a number 0 - 7 or special keywords `SP`, `PC`, or `ACC`
	TOKEN_IMMD,	// first character of token is `#` then a integer value (max size depends on vm settings)
	TOKEN_ADDR,	// first character of token is `[` then address in hex or integer followed by `]` close
	TOKEN_LABEL	// a string of characters not recognized as a OP followed by a `:`
} TokenType;

typedef struct Token{
	// type label
	TokenType type;

	// string of the token
	char *start;
	char *end;
	size_t len;

	// relevant metadata
	size_t line;
} Token;

#endif
