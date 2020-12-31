#ifndef AST_H
#define AST_H

#include "common.h"
#include "context.h"
#include "ds.h"
#include <iostream>

struct Context;

enum ASTNodeType : u8 {
	AST_NONE = 0,
	AST_PRIMITIVE_TYPE,
    AST_FN,
    AST_VAR,
    AST_BINARY_OP,
    AST_RETURN,
    AST_CAST,
    AST_NUMBER,
    AST_IF,
    AST_WHILE,
    AST_BLOCK,
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_BOOL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_TYPE
};

struct ASTNode {
	ASTNodeType nodetype;
    inline ASTNode(ASTNodeType nt) : nodetype(nt) {}
};

struct ASTBlock : ASTNode {
    Context ctx;
    arr<ASTNode*> statements;
    inline ASTBlock(Context& parent) : ASTNode(AST_BLOCK), ctx(&parent) {}
};

struct ASTType : ASTNode {
    inline ASTType(ASTNodeType nodetype) : ASTNode(nodetype) {}
};

struct TypeList {
    struct Entry {
        const char* name;
        ASTType* type;
    };
    arr<Entry> entries;
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
    ASTNode* initial_value;
    u64 location[2];

    // If the variable is a function argument, this is its index
    i32 argindex;

    inline ASTVar(const char* name, ASTType* type, ASTNode* initial_value, int argindex)
        : ASTNode(AST_VAR), name(name), type(type), initial_value(initial_value), argindex(argindex) {};
};

struct ASTNumber : ASTNode {
    u64 floorabs;
    ASTType* type;

    ASTNumber(u64 floorabs);
};

struct ASTIf : ASTNode {
    ASTNode* condition;
    ASTBlock block;

    inline ASTIf(Context& parent_ctx) : ASTNode(AST_IF), block(parent_ctx) {}
};

struct ASTWhile : ASTNode {
    ASTNode* condition;
    ASTBlock block;

    inline ASTWhile(Context& parent_ctx) : ASTNode(AST_WHILE), block(parent_ctx) {}
};

struct ASTCast : ASTNode {
    ASTType* newtype;
    ASTNode* inner;

    inline ASTCast(ASTType* newtype, ASTNode* inner)
        : ASTNode(AST_CAST), newtype(newtype), inner(inner) {};
};

struct ASTBinaryOp : ASTNode {
    TokenType op;
    ASTType* type;
    ASTNode *lhs, *rhs;

    inline ASTBinaryOp(TokenType op, ASTNode* lhs, ASTNode* rhs)
        : ASTNode(AST_BINARY_OP), lhs(lhs), rhs(rhs), op(op), type(nullptr) {}
};

struct ASTReturn : ASTNode {
    ASTNode *value;
    inline ASTReturn(ASTNode* value) : ASTNode(AST_RETURN), value(value) {};
};

struct ASTFn : ASTNode {
    const char* name;
    TypeList args;
    ASTBlock block;
    inline ASTFn(Context& parent_ctx, const char* name) : block(parent_ctx), ASTNode(AST_FN), name(name) {}
};

void print(std::ostream& o, ASTNode* node, bool decl);
void print(std::ostream& o, ASTPrimitiveType* node);
void print(std::ostream& o, ASTFn* node, bool decl);
void print(std::ostream& o, ASTBinaryOp* node, bool brackets);
void print(std::ostream& o, ASTVar* node, bool decl);
void print(std::ostream& o, ASTReturn* node);
void print(std::ostream& o, ASTCast* node);
void print(std::ostream& o, ASTNumber* node);
void print(std::ostream& o, ASTIf* node);
void print(std::ostream& o, ASTWhile* node);
void print(std::ostream& o, ASTBlock* bl);

#endif // guard
