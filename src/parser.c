#include "main.h"
#include "parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// for storing tokens while making raw array to return
// node in arrays
typedef struct TokenNode{
	Token *token;
	struct TokenNode *next;
	struct TokenNode *prev;
} TokenNode;

// parent struct for node
typedef struct TokenArray{
	TokenNode *head;
	TokenNode *tail;
	size_t len;
} TokenArray;

// global tokenArray
static TokenArray tokenArray;

// for reporting errors while parsing, gives line and a message
static void parserError(size_t line, const char *message){
	fprintf(stderr, "[Parser][Line %zu]: %s\n", line, message);
	exit(65);	// 65 will be exit code for parsing error
}

// init array with empty values
static void initArray(void){
	tokenArray.head = NULL;
	tokenArray.tail = NULL;
	tokenArray.len = 0;
}

// adds a token to the end of the list
static void pushToken(Token *token){
	// creates empty node with values of token
	TokenNode *tokenNode = gcAlloc(sizeof(TokenNode));
	tokenNode->token = token;
	tokenNode->next = NULL;
	tokenNode->prev = tokenArray.tail;

	// change prev and next properly
	if(tokenArray.tail != NULL){	// if empty array
		tokenArray.tail->next = tokenNode;
	} else{	// if regular insert
		tokenArray.head = tokenNode;
	}

	// add to end
	tokenArray.tail = tokenNode;
	tokenArray.len++;
}

static bool isAlpha(char c){
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c){
	return c >= '0' && c <= '9';
}

static bool isHexDigit(char c){
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// checks if a lexeme is a register name
static bool isRegisterName(const char *lexeme, size_t len){
	if(len == 2){	// r0 - r7 and sp anc pc are all 2 char long
		char first = (char)toupper((unsigned char)lexeme[0]);
		char second = (char)toupper((unsigned char)lexeme[1]);
		if(first == 'R' && second >= '0' && second <= '7'){
			return true;
		}
		if(first == 'S' && second == 'P'){
			return true;
		}
		if(first == 'P' && second == 'C'){
			return true;
		}
	}

	// only other register is acc (which is 3 long)
	if(len == 3){
		if(toupper((unsigned char)lexeme[0]) == 'A' &&
		    toupper((unsigned char)lexeme[1]) == 'C' &&
		    toupper((unsigned char)lexeme[2]) == 'C'){
			return true;
		}
	}

	return false;
}

// creates a token based on the contents of the char
static Token *createToken(const char *start, const char *end, TokenType type, size_t line){
	// allocate the token
	Token *token = gcAlloc(sizeof(Token));
	size_t length = 0;

	if(start != NULL && end != NULL && end >= start){
		length = (size_t)(end - start);
	}

	// initialize the lexeme for token
	char *lexeme = NULL;
	if(length > 0){
		lexeme = gcAlloc(length + 1);
		memcpy(lexeme, start, length);
		lexeme[length] = '\0';
	} else{
		lexeme = gcAlloc(1);
		lexeme[0] = '\0';
	}

	// add all the info into the token
	token->type = type;
	token->start = lexeme;
	token->end = lexeme + length;
	token->len = length;
	token->line = line;

	return token;
}

// do I really need to explain
// moves the cursor around whitespace and tells calling function when on a new line via firstToken
static void skipWhitespaceAndComments(const char **cursor, size_t *line, bool *firstToken){
	for(;;){
		char ch = **cursor;
		if(ch == '\0'){
			return;
		}
		if(ch == ';'){
			while(**cursor != '\0' && **cursor != '\n'){
				(*cursor)++;
			}
			continue;
		}
		if(ch == '\n'){
			(*cursor)++;
			(*line)++;
			*firstToken = true;
			continue;
		}
		if(ch == '\r' || ch == '\t' || ch == '\v' || ch == '\f' || ch == ' ' || ch == ','){
			(*cursor)++;
			continue;
		}
		if(isspace((unsigned char)ch)){
			(*cursor)++;
			continue;
		}
		return;
	}
}

// returns the TokenType (op, register, label)
static TokenType resolveIdentifierType(const char *start, size_t len, bool isFirstToken){
	if(isFirstToken){	// the first token is always an operation
		return TOKEN_OP;
	}
	if(isRegisterName(start, len)){	// r0 - r7 and sp pc acc
		return TOKEN_REG;
	}

	// if it isn't one of the above it must be a label
	return TOKEN_LABEL;
}

// scans for token in source, returns a pointer to a completed token
static Token *scanToken(const char **cursor, size_t *line, bool *firstToken){
	const char *start = *cursor;
	TokenType type = TOKEN_OP;
	bool keepFirstToken = false;

	char ch = **cursor;
	if(ch == '@'){	// labels start with @
		(*cursor)++;
		// get to the end of the label name (ends with ':')
		while(**cursor != '\0' && **cursor != '\n' && **cursor != ':'){
			(*cursor)++;
		}

		if(**cursor != ':'){	// if we didn't find a ':' then that's an error
			parserError(*line, "Label declarations must end with a colon");
		}

		(*cursor)++;	// move cursor past this lexeme
		type = TOKEN_LABEL;	// set type to be created later
		keepFirstToken = true;
	}
	else if(ch == '['){	// memory addresses start with '['
		(*cursor)++;
		while(**cursor != '\0' && **cursor != ']'){	// addresses must end with ']' ex: [0xFF00]
			if(**cursor == '\n'){	// there wasn't a ']' on the same line
				parserError(*line, "Unterminated memory address literal");
			}

			// if all is normal keep going to the end of the lexeme
			(*cursor)++;
		}

		if(**cursor != ']'){	// if we didn't find a ']'
			parserError(*line, "Memory address literal missing closing bracket");
		}

		(*cursor)++;	// advance cursor past this lexeme
		type = TOKEN_ADDR;	// set token type
	}
	else if(ch == '#'){	// Immediate values start with a #
		(*cursor)++;
		if(**cursor == '-' || **cursor == '+'){
			(*cursor)++;	// move past signage if there
		}

		// create a temp cursor to parse the number
		const char *numberCursor = *cursor;
		if(numberCursor[0] == '0' && (numberCursor[1] == 'x' || numberCursor[1] == 'X')){	// if it's a hex number
			// go past the '0x' part and then parse to the end of the digit
			(*cursor) += 2;
			while(isHexDigit(**cursor)){
				(*cursor)++;
			}
		}
		else{	// should be a integer value instead
			if(!isDigit(**cursor)){	// if not a number
				parserError(*line, "Immediate literal missing digits");
			}
			// move to the end of the digit
			while(isDigit(**cursor)){
				(*cursor)++;
			}
		}

		// set token type
		type = TOKEN_IMMD;
	}
	else if(isAlpha(ch)){	// if the lexeme starts with a character
		(*cursor)++;

		// get to it's end
		while(isAlpha(**cursor) || isDigit(**cursor)){
			(*cursor)++;
		}

		// find out what it is, should be either register, label, or opcode
		type = resolveIdentifierType(start,(size_t)(*cursor - start), *firstToken);
	}
	else if(isDigit(ch) || ch == '-'){	// if cursor is on a number
		(*cursor)++;

		// create a temp cursor to parse the number
		const char *numberCursor = *cursor;
		if(numberCursor[0] == '0' &&(numberCursor[1] == 'x' || numberCursor[1] == 'X')){	// if it's a hex number
			(*cursor) += 2;	// go past '0x'
			while(isHexDigit(**cursor)){
				(*cursor)++;	// get to the end of the number
			}
		}
		else{	// it is a base 10 number instead of hex
			if(!isDigit(**cursor)){	// if we hit a character that isn't a number
				parserError(*line, "Invalid numeric literal");
			}

			// get to the end of the number
			while(isDigit(**cursor)){
				(*cursor)++;
			}
		}

		// if it's the first value it is an op, if not it's an immediate value
		type = *firstToken ? TOKEN_OP : TOKEN_IMMD;
	}
	else{	// anything not handled by the other sections
		(*cursor)++;
		type = *firstToken ? TOKEN_OP : TOKEN_LABEL;	// first value in the line is a op, otherwise since it's unidentified it must be an label
	}

	// create a token based on the data that we have about it
	Token *token = createToken(start, *cursor, type, *line);
	if(keepFirstToken){
		*firstToken = true;
	} else{
		*firstToken = false;
	}

	return token;
}

// Reads a file in and returns a raw char array
static char *readFile(const char *path){
	FILE *file = fopen(path, "rb");
	if(file == NULL){	// check the file actually exists
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	// find the end of the file for bounds
	if(fseek(file, 0L, SEEK_END) != 0){
		fprintf(stderr, "Failed to seek file \"%s\".\n", path);
		exit(74);
	}

	// get the size of the file from seek
	long position = ftell(file);
	if(position < 0){
		fprintf(stderr, "Failed to determine size of \"%s\".\n", path);
		exit(74);
	}
	rewind(file);	// go back to start to start to save char *

	// allocate a char buffer based on the size gotten from above
	size_t fileSize = (size_t)position;
	char *buffer = malloc(fileSize + 1);	// using malloc instead of the gc for this since it's lifetime is relatively short and needs to be 100% safe
	if(buffer == NULL){
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}

	// make sure that we are reading the right number of bytes from the file (no shenanigans going on)
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if(bytesRead != fileSize){
		fprintf(stderr, "Couldn't read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytesRead] = '\0';	// add a EOF to the end of the buffer
	if(fclose(file) != 0){	// no longer need the file
		fprintf(stderr, "Failed to close file \"%s\".\n", path);
	}

	return buffer;
}

// takes in a file path and returns a stream of tokens based on the contents
Token *parser(char *pathToFile){
	initArray();	// need an array to store tokens in as they come dynamically
	char *fileContent = readFile(pathToFile);	// get the file in a char buffer
	const char *cursor = fileContent;	// make a cursor at the start of the buffer
	size_t line = 1;	// first line is 1, not 0
	bool firstTokenInLine = true;	// used to determine if it's an op (or lable if starts with @)

	// go over the entire file
	while(true){
		skipWhitespaceAndComments(&cursor, &line, &firstTokenInLine);	// no need to look over spacing
		if(*cursor == '\0'){
			break;	// break at the end of the file
		}

		// get tokens and push them to the array
		Token *token = scanToken(&cursor, &line, &firstTokenInLine);
		pushToken(token);
	}

	// create a end token, used later for compiling
	Token *endToken = createToken(NULL, NULL, TOKEN_END, line);
	pushToken(endToken);

	// turn the array that we made into a raw c list to be passed to other functions (compiler)
	Token *tokenList = gcAlloc(sizeof(Token) * tokenArray.len);
	TokenNode *node = tokenArray.head;
	size_t index = 0;
	while(node != NULL){
		tokenList[index++] = *(node->token);
		node = node->next;
	}

	// cleanup and return
	free(fileContent);	// freeing that buffer from readFile made with malloc
	initArray();	// not actually initing just sets everything to null and 0
	
	return tokenList;
}
