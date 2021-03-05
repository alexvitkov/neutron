#ifndef AST_H
#define AST_H

#include "common.h"
#include "number.h"
#include "context.h"
#include "ds.h"
#include <iostream>

struct AST_Context;

#define IS ->nodetype == 


enum PrimitiveTypeKind : u8 {
    PRIMITIVE_SIGNED,
    PRIMITIVE_UNSIGNED,
    PRIMITIVE_BOOL,
    PRIMITIVE_FLOAT,
    PRIMITIVE_UNIQUE
};


struct AST_Type;
struct AST_PrimitiveType;;
extern AST_PrimitiveType t_type;

struct AST_Value : AST_Node {
    AST_Type* type;
    inline AST_Value(AST_NodeType nodetype, AST_Type* type) : AST_Node(nodetype), type(type) {}
};


struct AST_Type : AST_Value {
    u64 size;
    inline AST_Type(AST_NodeType nodetype, u64 size) 
        : AST_Value(nodetype, (AST_Type*)&t_type), 
          size(size) {}
};

struct AST_UnresolvedId : AST_Value {
    AST_Context &ctx;
    const char  *name;
    ResolveJob  *job;

    inline AST_UnresolvedId(const char* typeName, AST_Context& ctx) 
        : AST_Value(AST_UNRESOLVED_ID, nullptr), name(typeName), ctx(ctx) 
    {
        ctx.global.unresolved.push(this);
    }
};

enum CallKind {
    FNCALL_REGULAR_FN,
    FNCALL_UNARY_OP,
    FNCALL_BINARY_OP,
    FNCALL_CAST
};

enum BracketsKind {
    BRACKETS_NORMAL = '(',
    BRACKETS_TRIANGLE = '<',
    BRACEKTS_SQUARE = '['
};

struct AST_Call : AST_Value {
    CallKind      kind;
    BracketsKind  brackets;
    TokenType            op;
    AST_Value           *fn;
    struct TIR_Builder  *builder;
    arr<AST_Value*> args;

    inline AST_Call(CallKind kind, TokenType op, AST_Value* fn, u32 args_reserve) 
        : AST_Value(AST_FN_CALL, nullptr), kind(kind), op(op), fn(fn), args(args_reserve), builder(nullptr) {}
};

struct AST_PrimitiveType : AST_Type {
    PrimitiveTypeKind kind;
    const char* name;

    inline AST_PrimitiveType(PrimitiveTypeKind kind, u64 size, const char* name)
        : AST_Type(AST_PRIMITIVE_TYPE, size), kind(kind), name(name) {}
};

// VOLATILE - if you add stuff to this, you MUST update its map_hash and map_equals
struct AST_FnType : AST_Type {
    AST_Type* returntype;
    // TODO we can probably get away with a regular arr here
    bucketed_arr<AST_Type*> param_types;
    bool is_variadic;

    inline AST_FnType(u64 size) : AST_Type(AST_FN_TYPE, size), is_variadic(false) {}
};


struct AST_ArrayType : AST_Type { 
    AST_Type* base_type;
    u64 array_length;

    inline AST_ArrayType(AST_Type* base_type, u64 length) 
        : AST_Type(AST_ARRAY_TYPE, base_type->size * length), 
          base_type(base_type), 
          array_length(length) {}
};

struct AST_Var : AST_Value {
    const char* name;

    // If the variable is a function argument, this is its index
    i64 argindex;

    bool is_global;
    bool is_constant;

    // If anywhere we take the address of this variable it must explicitly be stored on the stack
    bool always_on_stack;

    inline AST_Var(const char* name, i64 argindex)
        : AST_Value(AST_VAR, nullptr), 
          name(name), 
          argindex(argindex), 
          is_global(false),
          is_constant(false),
          always_on_stack(false) {};
};


struct AST_NumberLiteral : AST_Value {
    NumberData number_data;
    AST_NumberLiteral(NumberData *data);
};

struct AST_SmallNumber : AST_Value {
    union {
        u64    u64_val;
        i64    i64_val;
        double f64_val;
        float  f32_val;
    };

    inline AST_SmallNumber(AST_Type *type) : AST_Value(AST_SMALL_NUMBER, type) {}
};

struct AST_If : AST_Node {
    AST_Value* condition;
    AST_Context then_block;

    inline AST_If(AST_Context* parent_ctx) : AST_Node(AST_IF), then_block(parent_ctx) {}
};

struct AST_While : AST_Node {
    AST_Value* condition;
    AST_Context block;

    inline AST_While(AST_Context* parent_ctx) : AST_Node(AST_WHILE), block(parent_ctx) {}
};

struct AST_Return : AST_Node {
    AST_Value *value;
    inline AST_Return(AST_Value* value) : AST_Node(AST_RETURN), value(value) {};
};

struct StructElement {
    const char* name;
    AST_Type* type;
    u64 offset;
};

struct AST_FnLike : AST_Value {
    const char* name;
    arr<const char*> argument_names;
    AST_Context block;

    inline AST_FnLike(AST_NodeType nodetype, AST_Context* parent_ctx, const char* name) 
        : AST_Value(nodetype, nullptr), block(parent_ctx), name(name)
    {
    }
};

struct AST_Fn : AST_FnLike {

    bool is_extern = false;

    inline AST_FnType *fntype() { return (AST_FnType*)type; };

    inline AST_Fn(AST_Context* parent_ctx, const char* name) 
        : AST_FnLike(AST_FN, parent_ctx, name)
    {
        block.fn = this;
    }
};

struct AST_Macro : AST_FnLike {
    inline AST_Macro(AST_Context* parent_ctx, const char* name) 
        : AST_FnLike(AST_MACRO, parent_ctx, name)
    { 
        block.fn = this;
    }
};

struct AST_Emit : AST_Node {
    AST_Node *node_to_emit;
    inline AST_Emit() : AST_Node(AST_EMIT) {}
};

struct AST_Unquote : AST_Value {
    AST_Value *value_to_unquote;
    inline AST_Unquote(AST_Value *value_to_unquote) 
        : AST_Value(AST_UNQUOTE, nullptr), value_to_unquote(value_to_unquote) {}
};

struct AST_Struct : AST_Type {
    const char* name;
    bucketed_arr<StructElement> members;
    u64 alignment;
    inline AST_Struct(const char* name) 
        : AST_Type(AST_STRUCT, 0), 
          name(name),
          alignment(0) {}
};

struct AST_MemberAccess : AST_Value {
    AST_Value* lhs;
    u64 index;
    const char* member_name;
    inline AST_MemberAccess(AST_Value* lhs, const char* member_name) 
        : AST_Value(AST_MEMBER_ACCESS, nullptr), index(-1), lhs(lhs), member_name(member_name) {}
};


// VOLATILE
// Right now AST_PointerType and AST_Dereference have the same
// size and layout. The typechecker depends on that.
// If that has to change, update the validate_type function
//
// DOUBLE VOLATILE!
// If you add stuff to this, you MUST update map_hash and map_equals (defined in typer.cpp)
struct AST_PointerType : AST_Type {
    AST_Type* pointed_type;
    inline AST_PointerType(AST_Type* pointed_type, u64 pointer_size) 
        : AST_Type(AST_POINTER_TYPE, pointer_size), pointed_type(pointed_type) {}
};

struct AST_Dereference : AST_Value {
    AST_Value *ptr;

    inline AST_Dereference(AST_Value* ptr) 
        : AST_Value(AST_DEREFERENCE, nullptr), ptr(ptr) {}
};

struct AST_Typeof : AST_Value {
    AST_Value *inner;
    inline AST_Typeof(AST_Value* inner) 
        : AST_Value(AST_TYPEOF, &t_type), inner(inner) {}
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

// Used only for testing purposes, to test if two things
// that should parse down to the same AST, do in fact do that
bool postparse_tree_compare(AST_Node *lhs, AST_Node *rhs);

std::ostream& operator<< (std::ostream& o, AST_Node* node);

void print(std::wostream& o, AST_Node* node, bool decl);
void print(std::wostream& o, AST_Fn* node, bool decl);
void print(std::wostream& o, AST_Macro* node, bool decl);
void print(std::wostream& o, AST_Var* node, bool decl);
void print(std::wostream& o, AST_Struct* node, bool decl);

void print_string(std::wostream& o, const char* s);


std::wostream& operator<<(std::wostream& o, AST_FnType* node);
std::wostream& operator<<(std::wostream& o, AST_Node* node);
std::wostream& operator<<(std::wostream& o, AST_PrimitiveType* node);
std::wostream& operator<<(std::wostream& o, AST_Return* node);
std::wostream& operator<<(std::wostream& o, AST_NumberLiteral* node);
std::wostream& operator<<(std::wostream& o, AST_SmallNumber* node);
std::wostream& operator<<(std::wostream& o, AST_If* node);
std::wostream& operator<<(std::wostream& o, AST_While* node);
std::wostream& operator<<(std::wostream& o, AST_Context* bl);
std::wostream& operator<<(std::wostream& o, AST_Call* node);
std::wostream& operator<<(std::wostream& o, AST_MemberAccess* node);
std::wostream& operator<<(std::wostream& o, AST_PointerType* node);
std::wostream& operator<<(std::wostream& o, AST_ArrayType* node);
std::wostream& operator<<(std::wostream& o, AST_Dereference* node);
std::wostream& operator<<(std::wostream& o, AST_AddressOf* node);
std::wostream& operator<<(std::wostream& o, AST_UnresolvedId* node);
std::wostream& operator<<(std::wostream& o, AST_StringLiteral* node);
std::wostream& operator<<(std::wostream& o, AST_Typeof* node);
std::wostream& operator<<(std::wostream& o, AST_Emit* node);
std::wostream& operator<<(std::wostream& o, AST_Unquote* node);

#endif // guard
