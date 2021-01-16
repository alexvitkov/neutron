#ifndef ERROR_H
#define ERROR_H

#include "common.h"
#include "ds.h"
#include "util.h"
#include "cmdargs.h"

#include <iostream>

// @VOLATILE
// If you add/reorder entries here, update error_names in error.cpp accordingly
enum ErrorCode {
    ERR_UNKNOWN = 0,

    // Tokenizer errors
    ERR_UNBALANCED_BRACKETS,

    // Parser errors
    ERR_UNEXPECTED_TOKEN,
    ERR_ALREADY_DEFINED,
    ERR_NOT_DEFINED,
    ERR_INVALID_EXPRESSION,
    ERR_INVALID_NUMBER_FORMAT,

    // Typer errors
    ERR_INCOMPATIBLE_TYPES,
    ERR_INVALID_RETURN,
    ERR_NOT_AN_LVALUE,
    ERR_NO_SUCH_MEMBER,
    ERR_BAD_FN_CALL,
    ERR_INVALID_TYPE,
    ERR_INVALID_DEREFERENCE,
};

enum ErrorSeverity {
    SEVERITY_FATAL = 0,
};

struct Error {
    ErrorCode code;
    ErrorSeverity severity;
    arr<Token> tokens;

    arr<AST_Node*> nodes;
    arr<AST_Node**> node_ptrs;
};

struct Context;
void print_err(Context& global, Error& err);

#endif // guard
