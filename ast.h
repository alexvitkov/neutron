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
    AST_ASSIGNMENT,
    AST_RETURN,
    AST_CAST,
    AST_NUMBER,
    AST_IF,
    AST_WHILE,
    AST_BLOCK,
    AST_FN_CALL,
    AST_STRUCT,
    AST_MEMBER_ACCESS,
    AST_STRUCT_VALUE,
    AST_TEMP_REF,
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

struct ASTValue : ASTNode {
    ASTType* type;
    inline ASTValue(ASTNodeType nodetype, ASTType* type) : ASTNode(nodetype), type(type) {}
};

struct ASTFnCall : ASTNode {
    ASTNode* fn;
    arr<ASTNode*> args;

    inline ASTFnCall(ASTNode* fn) : ASTNode(AST_FN_CALL), fn(fn), args(4) {}
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

struct ASTVar : ASTValue {
    const char* name;
    ASTNode* initial_value;

    // If the variable is a function argument, this is its index
    i32 argindex;

    inline ASTVar(const char* name, ASTType* type, ASTNode* initial_value, int argindex)
        : ASTValue(AST_VAR, type), name(name), initial_value(initial_value), argindex(argindex) {};
};

struct ASTNumber : ASTValue {
    u64 floorabs;

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

struct ASTCast : ASTValue {
    ASTNode* inner;

    inline ASTCast(ASTType* newtype, ASTNode* inner)
        : ASTValue(AST_CAST, newtype), inner(inner) {};
};

struct ASTBinaryOp : ASTValue {
    TokenType op;
    ASTNode *lhs, *rhs;

    inline ASTBinaryOp(TokenType op, ASTNode* lhs, ASTNode* rhs)
        : ASTValue(AST_BINARY_OP, nullptr), lhs(lhs), rhs(rhs), op(op) {}
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

struct ASTStruct : ASTType {
    const char* name;
    TypeList members;
    inline ASTStruct(const char* name) : ASTType(AST_STRUCT), name(name) {}
};

struct ASTMemberAccess : ASTValue {
    ASTNode* lhs;
    int index;
    const char* member_name;
    inline ASTMemberAccess(ASTNode* lhs, const char* member_name) 
        : ASTValue(AST_MEMBER_ACCESS, nullptr), index(-1), lhs(lhs), member_name(member_name) {}
};

struct ASTStructValue : ASTNode {
    ASTStruct* type;

    // This fucker should store the locations of the individual struct elements
    inline ASTStructValue(ASTStruct* type) : ASTNode(AST_STRUCT_VALUE), type(type) {}
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
void print(std::ostream& o, ASTFnCall* node);
void print(std::ostream& o, ASTStruct* node, bool decl);
void print(std::ostream& o, ASTMemberAccess* node);

#endif // guard
