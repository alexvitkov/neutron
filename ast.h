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
    AST_BINARY_OP,
    AST_RETURN,
    AST_CAST,
    AST_NUMBER,
    AST_IF,
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_BOOL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_TYPE
};

struct Block {
    Context ctx;
    std::vector<ASTNode*> statements;
    inline Block(Context& parent) : ctx(parent) {}
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

struct Loc {
    enum {
        REGISTER,
        MEMORY,
        NODE,
        STACK,
        VALUE,
    } type;
    union {
        u8 reg;
        void* addr;
        ASTNode* node;
        u64 offset;
        u64 value;
    };
};

struct ASTVar : ASTNode {
    const char* name;
    ASTType* type;
    ASTNode* value;
    Loc location;
    i32 argindex;

    inline ASTVar(const char* name, ASTType* type, ASTNode* initial_value, int argindex)
        : ASTNode(AST_VAR), name(name), type(type), value(initial_value), argindex(argindex) {};
};

struct ASTNumber : ASTNode {
    u64 floorabs;
    ASTType* type;

    ASTNumber(u64 floorabs);
};

struct ASTIf : ASTNode {
    ASTNode* condition;
    Block block;

    inline ASTIf(Context& parent_ctx) : block(parent_ctx), ASTNode(AST_IF) {}
};

struct ASTCast : ASTNode {
    ASTType* newtype;
    ASTNode* inner;

    inline ASTCast(ASTType* newtype, ASTNode* inner)
        : ASTNode(AST_CAST), newtype(newtype), inner(inner) {};
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

// TODO too much indireciton here
struct ASTFn : ASTNode {
    const char* name;
    TypeList args;
    Block block;
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

#endif // guard
