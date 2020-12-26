#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "context.h"

struct Token {
	TokenType type;
	u64 length;
	u64 start;
};

void parse(int sf, Context* c);

#endif // guard
