#include "tir_builtins.h"
#include "cast.h"
#include "typer.h"
#include "tir.h"

map<TypePair,    BuiltinCast>   builtin_casts;
map<BinaryOpKey, TIR_Function*> builtin_binary_ops;
map<UnaryOpKey,  TIR_Function*> builtin_unary_ops;


struct Initializer {
    Initializer() {
        add_u64_u64.is_inline = true;
        builtin_casts.insert({ &t_number_literal, &t_u64 }, { number_literal_to_u64, 100 });
        // builtin_casts.insert({ &t_number_literal, &t_u32 }, { number_literal_to_u32, 90 });
        // builtin_casts.insert({ &t_number_literal, &t_u16 }, { number_literal_to_u16, 80 });
        // builtin_casts.insert({ &t_number_literal, &t_u8  }, { number_literal_to_u8,  70 });

        builtin_binary_ops.insert({ OP_ADD, &t_u64, &t_u64 }, &add_u64_u64);
    }
} _;

TIR_Function add_u64_u64 = {
    { 
        .opcode = TOPC_UADD, 
        .bin = { 
            .dst = { .valuespace = TVS_RET_VALUE }, 
            .lhs = { .valuespace = TVS_ARGUMENT, .offset = 0 },
            .rhs = { .valuespace = TVS_ARGUMENT, .offset = 1 },
        }
    },
    { .opcode = TOPC_RET }
};


template <typename T>
inline bool number_literal_to_unsigned(AST_GlobalContext &global, AST_Value *src, AST_Value **dst, AST_Type *t) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)src;
    assert(nl IS AST_NUMBER);

    T num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        return false;
    }

    AST_SmallNumber *sn = global.alloc<AST_SmallNumber>(t);
    sn->u64_val = num;
    *dst = sn;
    return true;
}

bool number_literal_to_u64(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    return number_literal_to_unsigned<u64>(global, src, dst, &t_u64);
}
