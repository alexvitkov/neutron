#ifndef AST_H
#define AST_H

#include "common.h"
#include "context.h"
#include "ds.h"
#include <iostream>

struct Context;

#define AST_TYPE_BIT  0x80
#define AST_VALUE_BIT 0x40

enum AST_NodeType : u8 {
	AST_NONE = 0,

    AST_RETURN        = 0x01,
    AST_IF            = 0x02,
    AST_WHILE         = 0x03,
    AST_BLOCK         = 0x04,

    AST_FN            = 0x00 | AST_VALUE_BIT,
    AST_VAR           = 0x01 | AST_VALUE_BIT,
    AST_BINARY_OP     = 0x02 | AST_VALUE_BIT,
    AST_ASSIGNMENT    = 0x03 | AST_VALUE_BIT,
    AST_CAST          = 0x04 | AST_VALUE_BIT,
    AST_NUMBER        = 0x05 | AST_VALUE_BIT,
    AST_FN_CALL       = 0x06 | AST_VALUE_BIT,
    AST_MEMBER_ACCESS = 0x07 | AST_VALUE_BIT,
    AST_TEMP_REF      = 0x08 | AST_VALUE_BIT,
    AST_UNRESOLVED_ID = 0x09 | AST_VALUE_BIT,
    AST_DEREFERENCE   = 0x0a | AST_VALUE_BIT,
    AST_ADDRESS_OF    = 0x0b | AST_VALUE_BIT,
    AST_STRING_LITERAL= 0x0c | AST_VALUE_BIT,


    AST_STRUCT         = 0x00 | AST_VALUE_BIT | AST_TYPE_BIT,
	AST_PRIMITIVE_TYPE = 0x01 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_FN_TYPE        = 0x02 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_POINTER_TYPE   = 0x03 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_ARRAY_TYPE     = 0x04 | AST_VALUE_BIT | AST_TYPE_BIT,
};

enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_BOOL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_UNIQUE
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

struct AST_Type;
struct AST_PrimitiveType;;
extern AST_PrimitiveType t_type;

struct AST_Value : AST_Node {
    union {
        const char* typeName;
        AST_Type* type;
    };
    inline AST_Value(AST_NodeType nodetype, AST_Type* type) : AST_Node(nodetype), type(type) {}
};


struct AST_Type : AST_Value {
    inline AST_Type(AST_NodeType nodetype) : AST_Value(nodetype, (AST_Type*)&t_type) {}
};

struct AST_UnresolvedId : AST_Value {
    Context& ctx;

    const char* name;
    AST_Node* resolved;

    inline AST_UnresolvedId(const char* typeName, Context& ctx) 
        : AST_Value(AST_UNRESOLVED_ID, nullptr), name(typeName), ctx(ctx) 
    {
        ctx.global->unresolved.push(this);
    }
};

struct AST_FnCall : AST_Value {
    AST_Node* fn;
    arr<AST_Value*> args;

    inline AST_FnCall(AST_Node* fn) : AST_Value(AST_FN_CALL, nullptr), fn(fn), args(4) {}
};

struct AST_PrimitiveType : AST_Type {
    PrimitiveTypeKind kind;
    u8 size;
    const char* name;

    inline AST_PrimitiveType(PrimitiveTypeKind kind, u8 size, const char* name)
        : AST_Type(AST_PRIMITIVE_TYPE), kind(kind), size(size), name(name) {}
};

struct AST_FnType : AST_Type {
    AST_Type* returntype;
    arr<AST_Type*> param_types;
    bool is_variadic;

    inline AST_FnType() : AST_Type(AST_FN_TYPE), is_variadic(false) {}
};


struct AST_ArrayType : AST_Type { 
    AST_Type* base_type;
    u64 array_length;
    inline AST_ArrayType(AST_Type* base_type, u64 length) 
        : AST_Type(AST_ARRAY_TYPE), base_type(base_type), array_length(length) {}
};

struct AST_Var : AST_Value {
    const char* name;
    AST_Value* initial_value;

    // If the variable is a function argument, this is its index
    i32 argindex;

    inline AST_Var(const char* name, int argindex)
        : AST_Value(AST_VAR, nullptr), name(name), argindex(argindex), initial_value(nullptr) {};
};

struct AST_Number : AST_Value {
    u64 floorabs;

    AST_Number(u64 floorabs);
};

struct AST_If : AST_Node {
    AST_Node* condition;
    AST_Block then_block;

    inline AST_If(Context& parent_ctx) : AST_Node(AST_IF), then_block(parent_ctx) {}
};

struct AST_While : AST_Node {
    AST_Node* condition;
    AST_Block block;

    inline AST_While(Context& parent_ctx) : AST_Node(AST_WHILE), block(parent_ctx) {}
};

struct AST_Cast : AST_Value {
    AST_Value* inner;

    inline AST_Cast(AST_Type* targetType, AST_Value* inner)
        : AST_Value(AST_CAST, targetType), inner(inner) {};
};

struct AST_BinaryOp : AST_Value {
    TokenType op;
    AST_Value *lhs, *rhs;

    inline AST_BinaryOp(TokenType op, AST_Value* lhs, AST_Value* rhs)
        : AST_Value(AST_BINARY_OP, nullptr), lhs(lhs), rhs(rhs), op(op) {}
};

struct AST_Return : AST_Node {
    AST_Value *value;
    inline AST_Return(AST_Value* value) : AST_Node(AST_RETURN), value(value) {};
};

struct NamedType {
    const char* name;
    AST_Type* type;
};

struct AST_Fn : AST_Value {
    const char* name;
    arr<NamedType> args;
    AST_Block block;
    bool is_extern = false;

    inline AST_Fn(Context& parent_ctx, const char* name) 
        : AST_Value(AST_FN, nullptr), block(parent_ctx), name(name) {}
};

struct AST_Struct : AST_Type {
    const char* name;
    arr<NamedType> members;
    inline AST_Struct(const char* name) : AST_Type(AST_STRUCT), name(name) {}
};

struct AST_MemberAccess : AST_Value {
    AST_Value* lhs;
    int index;
    const char* member_name;
    inline AST_MemberAccess(AST_Value* lhs, const char* member_name) 
        : AST_Value(AST_MEMBER_ACCESS, nullptr), index(-1), lhs(lhs), member_name(member_name) {}
};


// VOLATILE
// Right now AST_PointerType and AST_Dereference have the same
// size and layout. The typechecker depends on that.
// If that has to change, update the validate_type function
struct AST_PointerType : AST_Type {
    AST_Type* pointed_type;
    inline AST_PointerType(AST_Type* pointed_type) 
        : AST_Type(AST_POINTER_TYPE), pointed_type(pointed_type) {}
};

struct AST_Dereference : AST_Value {
    AST_Value* ptr;

    inline AST_Dereference(AST_Value* ptr) 
        : AST_Value(AST_DEREFERENCE, nullptr), ptr(ptr) {}
};




struct AST_AddressOf : AST_Value {
    AST_Value* inner;

    inline AST_AddressOf(AST_Value* inner) 
        : AST_Value(AST_ADDRESS_OF, nullptr), inner(inner) {}
};

struct AST_StringLiteral : AST_Value {
    u64 length;
    const char* str;
    
    AST_StringLiteral(Token stringToken);
};


std::ostream& operator<< (std::ostream& o, AST_Node* node);

void print(std::ostream& o, AST_Node* node, bool decl);
void print(std::ostream& o, AST_Fn* node, bool decl);
void print(std::ostream& o, AST_Var* node, bool decl);
void print(std::ostream& o, AST_Struct* node, bool decl);
void print(std::ostream& o, AST_BinaryOp* node, bool brackets);

std::ostream& operator<<(std::ostream& o, AST_PrimitiveType* node);
std::ostream& operator<<(std::ostream& o, AST_Return* node);
std::ostream& operator<<(std::ostream& o, AST_Cast* node);
std::ostream& operator<<(std::ostream& o, AST_Number* node);
std::ostream& operator<<(std::ostream& o, AST_If* node);
std::ostream& operator<<(std::ostream& o, AST_While* node);
std::ostream& operator<<(std::ostream& o, AST_Block* bl);
std::ostream& operator<<(std::ostream& o, AST_FnCall* node);
std::ostream& operator<<(std::ostream& o, AST_MemberAccess* node);
std::ostream& operator<<(std::ostream& o, AST_PointerType* node);
std::ostream& operator<<(std::ostream& o, AST_ArrayType* node);
std::ostream& operator<<(std::ostream& o, AST_Dereference* node);
std::ostream& operator<<(std::ostream& o, AST_AddressOf* node);
std::ostream& operator<<(std::ostream& o, AST_UnresolvedId* node);
std::ostream& operator<<(std::ostream& o, AST_StringLiteral* node);

#endif // guard
