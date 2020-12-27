#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "typer.h"
#include "context.h"

struct Token {
	TokenType type;
	u32 match; // inedex of matching bracket
	u32 length;
	u64 start;
};

bool parse_all_files(Context& global);

#endif // guard
