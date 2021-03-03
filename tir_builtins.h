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


struct BinaryOpKey {
    TokenType op;
    AST_Type *lhs;
    AST_Type *rhs;
};
u32  map_hash(BinaryOpKey key);
bool map_equals(BinaryOpKey a, BinaryOpKey b);


struct UnaryOpKey {
    TokenType op;
    AST_Type *inner;
};
u32  map_hash(UnaryOpKey key);
bool map_equals(UnaryOpKey a, UnaryOpKey b);


struct BuiltinCast {
    BuiltinCastFn fn;
    int priority;
};

extern map<TypePair,    BuiltinCast>   builtin_casts;
extern map<BinaryOpKey, TIR_Function*> builtin_binary_ops;
extern map<UnaryOpKey,  TIR_Function*> builtin_unary_ops;

bool number_literal_to_u64(AST_GlobalContext &global, AST_Value *src, AST_Value **dst);
bool number_literal_to_u32(AST_GlobalContext &global, AST_Value *src, AST_Value **dst);
bool number_literal_to_u16(AST_GlobalContext &global, AST_Value *src, AST_Value **dst);
bool number_literal_to_u8 (AST_GlobalContext &global, AST_Value *src, AST_Value **dst);


#endif // guard
