#ifndef ERROR_H
#define ERROR_H

#include "common.h"
#include "context.h"
#include "ds.h"

enum ErrorCode {
    ERR_UNKNOWN = 0,

    // Tokenizer errors
    ERR_UNRECOGNIZED_CHARACTER,
    ERR_UNBALANCED_BRACKETS,

    // Parser errors
    ERR_UNEXPECTED_TOKEN,
    ERR_ALREADY_DEFINED,
    ERR_NOT_DEFINED,
    ERR_INVALID_EXPRESSION,

    // Typer errors
    ERR_INCOMPATIBLE_TYPES,
    ERR_INVALID_RETURN,
};

enum ErrorSeverity {
    SEVERITY_FATAL = 0,
};

struct Error {
    ErrorCode code;
    ErrorSeverity severity;
    arr<Token> tokens;
    arr<ASTNode*> nodes;
};

void print(Context& global, Error& err);

#endif // guard
