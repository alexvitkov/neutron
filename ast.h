#ifndef AST_H
#define AST_H

#include "common.h"
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
    union {
        const char* typeName;
        AST_Type* type;
    };
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


// VOLATILE
// The parser miscategorizes casts as function calls as they have the same syntax
//  - foo(100) can either be a cast or a fn call, we don't know at parse time
//
// The typer then patches up AST_FnCalls into AST_Casts by changing their nodetype,
// so their structures must be compatible.
struct AST_Cast : AST_Value {
    AST_Value* inner;

    enum CastType {
        Pointer_Pointer,
        StringLiteral_I8,
        ZeroExtend,
        SignedExtend,
    } casttype;

    inline AST_Cast(AST_Type* targetType, AST_Value* inner)
        : AST_Value(AST_CAST, targetType), inner(inner) {};
};


// VOLATILE
// Read the comment before AST_Cast before changing this
struct AST_FnCall : AST_Value {
    AST_Value* fn;
    bucketed_arr<AST_Value*> args;

    inline AST_FnCall(AST_Value* fn) : AST_Value(AST_FN_CALL, nullptr), fn(fn), args() {}
};

struct AST_PrimitiveType : AST_Type {
    PrimitiveTypeKind kind;
    const char* name;

    inline AST_PrimitiveType(PrimitiveTypeKind kind, u64 size, const char* name)
        : AST_Type(AST_PRIMITIVE_TYPE, size), kind(kind), name(name) {}
};

// VOLATILE
// If you add stuff to this, you MUST update map_hash and map_equals for (defined in typer.cpp)
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
          always_on_stack(false) {};
};

struct AST_Number : AST_Value {
    u64 floorabs;

    bool has_decimal_point;
    u64 after_decimal_point;

    AST_Number(u64 floorabs);

    bool fits_in(AST_PrimitiveType* numtype);
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

struct StructElement {
    const char* name;
    AST_Type* type;
    u64 offset;
};

struct AST_Fn : AST_Value {
    const char* name;

    arr<const char*> argument_names;

    AST_Context block;
    bool is_extern = false;

    inline AST_Fn(AST_Context* parent_ctx, const char* name) 
        : AST_Value(AST_FN, nullptr), block(parent_ctx), name(name) 
    {
        block.fn = this;
    }
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
void print(std::wostream& o, AST_Var* node, bool decl);
void print(std::wostream& o, AST_Struct* node, bool decl);
void print(std::wostream& o, AST_BinaryOp* node, bool brackets);

std::wostream& operator<<(std::wostream& o, AST_Node* node);
std::wostream& operator<<(std::wostream& o, AST_PrimitiveType* node);
std::wostream& operator<<(std::wostream& o, AST_Return* node);
std::wostream& operator<<(std::wostream& o, AST_Cast* node);
std::wostream& operator<<(std::wostream& o, AST_Number* node);
std::wostream& operator<<(std::wostream& o, AST_If* node);
std::wostream& operator<<(std::wostream& o, AST_While* node);
std::wostream& operator<<(std::wostream& o, AST_Context* bl);
std::wostream& operator<<(std::wostream& o, AST_FnCall* node);
std::wostream& operator<<(std::wostream& o, AST_MemberAccess* node);
std::wostream& operator<<(std::wostream& o, AST_PointerType* node);
std::wostream& operator<<(std::wostream& o, AST_ArrayType* node);
std::wostream& operator<<(std::wostream& o, AST_Dereference* node);
std::wostream& operator<<(std::wostream& o, AST_AddressOf* node);
std::wostream& operator<<(std::wostream& o, AST_UnresolvedId* node);
std::wostream& operator<<(std::wostream& o, AST_StringLiteral* node);
std::wostream& operator<<(std::wostream& o, AST_Typeof* node);

#endif // guard
