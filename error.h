#ifndef ERROR_H
#define ERROR_H

#include "common.h"
#include "ds.h"
#include "util.h"
#include "cmdargs.h"

#include <iostream>

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
    ERR_INVALID_ASSIGNMENT,
    ERR_RETURN_TYPE_MISSING,
    ERR_RETURN_TYPE_INVALID,
    ERR_NOT_AN_LVALUE,
    ERR_NO_SUCH_MEMBER,
    ERR_BAD_FN_CALL,
    ERR_INVALID_TYPE,
    ERR_INVALID_DEREFERENCE,
    ERR_INVALID_CAST,
};

enum ErrorSeverity {
    SEVERITY_FATAL = 0,
};

struct ArgumentErr {
    const char* arg_name;
    AST_Node** arg_type_ptr;
    u64 arg_index;
};

struct Error {
    ErrorCode code;
    ErrorSeverity severity;
    arr<Token> tokens;

    arr<AST_Node*> nodes;
    arr<AST_Node**> node_ptrs;
    arr<ArgumentErr> args;
};

struct Context;
void print_err(Context& global, Error& err);

#endif // guard
