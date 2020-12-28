#ifndef AST_H
#define AST_H

#include "common.h"
#include <iostream>

struct Context;

enum ASTNodeType : u8 {
	AST_NONE = 0,
	AST_PRIMITIVE_TYPE,
    AST_FN,
    AST_VAR,
    AST_BLOCK,
    AST_BINARY_OP,
    AST_RETURN,
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_FLOAT
};

struct Block {
    Context* ctx;
    std::vector<ASTNode*> statements;
};

struct ASTNode {
	ASTNodeType nodetype;
    inline ASTNode(ASTNodeType nt) : nodetype(nt) {}
};

struct ASTType : ASTNode {
    inline ASTType(ASTNodeType nodetype) : ASTNode(nodetype) {}
};

struct TypeList {
    struct Entry {
        const char* name;
        ASTType* type;
    };
    std::vector<Entry> entries;
};

struct ASTPrimitiveType : ASTType {
    PrimitiveTypeKind kind;
    u8 size;
    const char* name;

    inline ASTPrimitiveType(PrimitiveTypeKind kind, u8 size, const char* name)
        : ASTType(AST_PRIMITIVE_TYPE), kind(kind), size(size), name(name) {}
};

struct ASTVar : ASTNode {
    const char* name;
    ASTType* type;
    ASTNode* value;

    inline ASTVar(const char* name, ASTType* type, ASTNode* initial_value)
        : ASTNode(AST_VAR), name(name), type(type), value(initial_value) {};
};

struct ASTBinaryOp : ASTNode {
    TokenType op;
    ASTNode *lhs, *rhs;

    inline ASTBinaryOp(TokenType op, ASTNode* lhs, ASTNode* rhs)
        : ASTNode(AST_BINARY_OP), lhs(lhs), rhs(rhs), op(op) {}
};

struct ASTReturn : ASTNode {
    ASTNode *value;
    inline ASTReturn(ASTNode* value) : ASTNode(AST_RETURN), value(value) {};
};

struct ASTFn : ASTNode {
    const char* name;
    TypeList args;
    Block* block;
    ASTType* rettype;
    inline ASTFn(const char* name) : ASTNode(AST_FN), name(name) {}
};

void print(std::ostream& o, ASTNode* node, bool decl);
void print(std::ostream& o, ASTPrimitiveType* node);
void print(std::ostream& o, ASTFn* node, bool decl);
void print(std::ostream& o, ASTBinaryOp* node);
void print(std::ostream& o, ASTVar* node, bool decl);
void print(std::ostream& o, ASTReturn* node);

#endif // guard
