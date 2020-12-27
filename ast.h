#ifndef AST_H
#define AST_H

#include "common.h"
#include <iostream>

enum ASTNodeType {
	AST_NONE = 0,
	AST_TYPE,
    AST_FN,
    AST_VAR
};

struct ASTNode {
	ASTNodeType nodetype;
};

struct ASTType {
	ASTNodeType nodetype;
    const char* name;
};

struct ASTVar {
	ASTNodeType nodetype;
    const char* name;
    ASTType* type;
    ASTNode* value;
};

struct TypeList {
    struct Entry {
        const char* name;
        ASTType* value;
    };
    std::vector<Entry> entries;
};

struct Context;

struct ASTFn {
	ASTNodeType nodetype;
    char* name;
    TypeList args;
    Context* ctx;
};

std::ostream& operator<<(std::ostream& o, ASTNode* node);
std::ostream& operator<<(std::ostream& o, ASTType* node);
std::ostream& operator<<(std::ostream& o, ASTFn* node);

#endif // guard
