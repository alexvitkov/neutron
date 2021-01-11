#ifndef ERROR_H
#define ERROR_H

#include "common.h"
#include "ds.h"
#include "sourcefile.h"
#include "util.h"

#include <iostream>

// @VOLATILE
// If you add/reorder entries here, update error_names in error.cpp accordingly
enum ErrorCode {
    ERR_UNKNOWN = 0,

    // Tokenizer errors
    ERR_UNRECOGNIZED_CHARACTER = 1,
    ERR_UNBALANCED_BRACKETS = 2,

    // Parser errors
    ERR_UNEXPECTED_TOKEN = 3,
    ERR_ALREADY_DEFINED = 4,
    ERR_NOT_DEFINED = 5,
    ERR_INVALID_EXPRESSION,

    // Typer errors
    ERR_INCOMPATIBLE_TYPES,
    ERR_INVALID_RETURN,
    ERR_NOT_AN_LVALUE,
    ERR_NO_SUCH_MEMBER,
    ERR_BAD_FN_CALL,
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

struct Context;
void print_err(Context& global, Error& err);

#endif // guard
