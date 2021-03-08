#ifndef TIR_BULTINS_H
#define TIR_BULTINS_H

#include "context.h"

struct TIR_Function;

typedef bool (*BuiltinCastFn) (AST_GlobalContext &global, AST_Value *src, AST_Value **dst);

struct TypePair { 
    AST_Type *lhs;
    AST_Type *rhs;
};
u32  map_hash(TypePair key);
bool map_equals(TypePair a, TypePair b);


struct BuiltinCast {
    BuiltinCastFn fn;
    int priority;
};

struct TIR_Builder *get_builder(TokenType op, AST_Type *lhs, AST_Type *rhs, AST_Type **out);

extern map<TypePair, BuiltinCast>  builtin_casts;


#endif // guard
