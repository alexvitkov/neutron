#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ast.h"

#include <string>

struct Error {
	enum {
		TOKENIZER,
		PARSER,
	} kind;

	enum {
		WARNING,
		ERROR,
	} severity;

	const char* message;

	void print();
};

struct Context {
	std::unordered_map<std::string, ASTNode*> defines;
	std::vector<Error> errors;
    Context* global;
    Context* parent;

	bool ok();
    bool is_global();

    ASTNode* resolve(const char* name);
    bool define(const char* name, ASTNode* value);
};

#endif // guard
