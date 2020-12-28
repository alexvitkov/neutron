#ifndef AST_H
#define AST_H

#include "common.h"
#include <iostream>

enum ASTNodeType {
	AST_NONE = 0,
	AST_TYPE,
    AST_FN,
    AST_VAR,
    AST_BLOCK,
    AST_BINARY_OP,
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

struct ASTBinaryOp {
	ASTNodeType nodetype;
    TokenType op;
    ASTNode *lhs, *rhs;
};

struct Block {
    Context* ctx;
    std::vector<ASTNode*> statements;
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
    Block* block;
};

void print(std::ostream& o, ASTNode* node, bool decl);
void print(std::ostream& o, ASTType* node, bool decl);
void print(std::ostream& o, ASTFn* node, bool decl);
void print(std::ostream& o, ASTBinaryOp* node);
void print(std::ostream& o, ASTVar* node, bool decl);

#endif // guard
