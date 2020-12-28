#ifndef AST_H
#define AST_H

#include "common.h"
#include <iostream>

enum ASTNodeType {
	AST_NONE = 0,
	AST_PRIMITIVE_TYPE,
    AST_FN,
    AST_VAR,
    AST_BLOCK,
    AST_BINARY_OP,
    AST_RETURN,
};

struct ASTNode {
	ASTNodeType nodetype;
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_FLOAT
};

struct ASTPrimitiveType {
	ASTNodeType nodetype;
    PrimitiveTypeKind kind;
    u8 size;
    const char* name;
};

struct ASTVar {
    ASTNodeType nodetype;
    const char* name;
    ASTNode* type;
    ASTNode* value;
};

struct ASTBinaryOp {
    ASTNodeType nodetype;
    TokenType op;
    ASTNode *lhs, *rhs;
};

struct ASTReturn {
    ASTNodeType nodetype;
    ASTNode *value;
};

struct Block {
    Context* ctx;
    std::vector<ASTNode*> statements;
};

struct TypeList {
    struct Entry {
        const char* name;
        ASTNode* type;
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
void print(std::ostream& o, ASTPrimitiveType* node);
void print(std::ostream& o, ASTFn* node, bool decl);
void print(std::ostream& o, ASTBinaryOp* node);
void print(std::ostream& o, ASTVar* node, bool decl);
void print(std::ostream& o, ASTReturn* node);

#endif // guard
