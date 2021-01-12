#ifndef AST_H
#define AST_H

#include "common.h"
#include "context.h"
#include "ds.h"
#include <iostream>

struct Context;

enum AST_NodeType : u8 {
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
    AST_TEMP_REF,
    AST_UNRESOLVED_ID,
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_BOOL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_TYPE
};

struct AST_Node {
	AST_NodeType nodetype;
    inline AST_Node(AST_NodeType nt) : nodetype(nt) {}
};

struct AST_Block : AST_Node {
    Context ctx;
    arr<AST_Node*> statements;
    inline AST_Block(Context& parent) : AST_Node(AST_BLOCK), ctx(&parent) {}
};

struct AST_Type : AST_Node {
    inline AST_Type(AST_NodeType nodetype) : AST_Node(nodetype) {}
};

struct AST_UnresolvedId : AST_Node {
    Context& ctx;
    const char* name;
    inline AST_UnresolvedId(const char* typeName, Context& ctx) 
        : AST_Node(AST_UNRESOLVED_ID), name(typeName), ctx(ctx) {}
};

struct AST_Value : AST_Node {
    union {
        const char* typeName;
        AST_Type* type;
    };
    inline AST_Value(AST_NodeType nodetype, AST_Type* type) : AST_Node(nodetype), type(type) {}
};

struct AST_FnCall : AST_Node {
    AST_Node* fn;
    arr<AST_Node*> args;

    inline AST_FnCall(AST_Node* fn) : AST_Node(AST_FN_CALL), fn(fn), args(4) {}
};

struct AST_PrimitiveType : AST_Type {
    PrimitiveTypeKind kind;
    u8 size;
    const char* name;

    inline AST_PrimitiveType(PrimitiveTypeKind kind, u8 size, const char* name)
        : AST_Type(AST_PRIMITIVE_TYPE), kind(kind), size(size), name(name) {}
};

struct AST_Var : AST_Value {
    const char* name;
    AST_Node* initial_value;

    // If the variable is a function argument, this is its index
    i32 argindex;

    inline AST_Var(const char* name, AST_Type* type, AST_Node* initial_value, int argindex)
        : AST_Value(AST_VAR, type), name(name), initial_value(initial_value), argindex(argindex) {};
};

struct AST_Number : AST_Value {
    u64 floorabs;

    AST_Number(u64 floorabs);
};

struct AST_If : AST_Node {
    AST_Node* condition;
    AST_Block block;

    inline AST_If(Context& parent_ctx) : AST_Node(AST_IF), block(parent_ctx) {}
};

struct AST_While : AST_Node {
    AST_Node* condition;
    AST_Block block;

    inline AST_While(Context& parent_ctx) : AST_Node(AST_WHILE), block(parent_ctx) {}
};

struct AST_Cast : AST_Value {
    AST_Node* inner;

    inline AST_Cast(AST_Type* targetType, AST_Node* inner)
        : AST_Value(AST_CAST, targetType), inner(inner) {};
};

struct AST_BinaryOp : AST_Value {
    TokenType op;
    AST_Node *lhs, *rhs;

    inline AST_BinaryOp(TokenType op, AST_Node* lhs, AST_Node* rhs)
        : AST_Value(AST_BINARY_OP, nullptr), lhs(lhs), rhs(rhs), op(op) {}
};

struct AST_Return : AST_Node {
    AST_Node *value;
    inline AST_Return(AST_Node* value) : AST_Node(AST_RETURN), value(value) {};
};

struct NamedType {
    const char* name;
    AST_Type* type;
};

struct AST_Fn : AST_Node {
    const char* name;
    arr<NamedType> args;
    AST_Block block;

    inline AST_Fn(Context& parent_ctx, const char* name) 
        : AST_Node(AST_FN), block(parent_ctx), name(name) {}
};

struct AST_Struct : AST_Type {
    const char* name;
    arr<NamedType> members;
    inline AST_Struct(const char* name) : AST_Type(AST_STRUCT), name(name) {}
};

struct AST_MemberAccess : AST_Value {
    AST_Node* lhs;
    int index;
    const char* member_name;
    inline AST_MemberAccess(AST_Node* lhs, const char* member_name) 
        : AST_Value(AST_MEMBER_ACCESS, nullptr), index(-1), lhs(lhs), member_name(member_name) {}
};

void print(std::ostream& o, AST_Node* node, bool decl);
void print(std::ostream& o, AST_PrimitiveType* node);
void print(std::ostream& o, AST_Fn* node, bool decl);
void print(std::ostream& o, AST_BinaryOp* node, bool brackets);
void print(std::ostream& o, AST_Var* node, bool decl);
void print(std::ostream& o, AST_Return* node);
void print(std::ostream& o, AST_Cast* node);
void print(std::ostream& o, AST_Number* node);
void print(std::ostream& o, AST_If* node);
void print(std::ostream& o, AST_While* node);
void print(std::ostream& o, AST_Block* bl);
void print(std::ostream& o, AST_FnCall* node);
void print(std::ostream& o, AST_Struct* node, bool decl);
void print(std::ostream& o, AST_MemberAccess* node);

#endif // guard
