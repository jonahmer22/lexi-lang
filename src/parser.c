#include "main.h"
#include "parser.h"

#include <stdbool.h>

static bool isAlpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c){
    return c >= '0' && c <= '9';
}


