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

extern map<TypePair, BuiltinCast>  builtin_casts;

struct BinaryOpKey {
    TokenType op;
    AST_Type *lhs;
    AST_Type *rhs;
};

u32 map_hash(BinaryOpKey key);
bool map_equals(BinaryOpKey a, BinaryOpKey b);
extern map <BinaryOpKey, struct TIR_Builder*> binary_builders;

#endif // guard
