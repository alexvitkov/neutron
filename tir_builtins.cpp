#include "tir_builtins.h"
#include "cast.h"
#include "typer.h"
#include "tir.h"

map<TypePair,    BuiltinCast>   builtin_casts;
map<BinaryOpKey, TIR_Function*> builtin_binary_ops;
map<UnaryOpKey,  TIR_Function*> builtin_unary_ops;


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
bool number_literal_to_u32(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    return number_literal_to_unsigned<u32>(global, src, dst, &t_u32);
}
bool number_literal_to_u16(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    return number_literal_to_unsigned<u16>(global, src, dst, &t_u16);
}
bool number_literal_to_u8(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    return number_literal_to_unsigned<u8>(global, src, dst, &t_u8);
}

struct Initializer {
    Initializer() {
        builtin_casts.insert({ &t_number_literal, &t_u64 }, { number_literal_to_u64, 100 });
        builtin_casts.insert({ &t_number_literal, &t_u32 }, { number_literal_to_u32, 90 });
        builtin_casts.insert({ &t_number_literal, &t_u16 }, { number_literal_to_u16, 80 });
        builtin_casts.insert({ &t_number_literal, &t_u8  }, { number_literal_to_u8,  70 });

        struct Pair { 
            TokenType tok; 
            TIR_OpCode opc; 
        };

        arr<Pair> opcodes = { 
            { (TokenType)('+'), TOPC_ADD }, 
            { (TokenType)('-'), TOPC_SUB }, 
            { (TokenType)('*'), TOPC_MUL },
            { (TokenType)('/'), TOPC_DIV }
        };

        for (Pair op : opcodes) {
            for (AST_Type *t1 : unsigned_types) {
                for (AST_Type *t2 : unsigned_types) {
                    TIR_Function *tirfn = new TIR_Function {};
                    tirfn->is_inline = true;

                    TIR_Value lhs = { .valuespace = TVS_ARGUMENT, .offset = 0, .type = t1 };
                    TIR_Value rhs = { .valuespace = TVS_ARGUMENT, .offset = 1, .type = t2 };
                    tirfn->returntype = t1->size > t2->size ? t1 : t2;

                    if (t1->size < t2->size) {
                        tirfn->emit({ 
                            .opcode = TOPC_ZEXT,
                            .un = { 
                                .dst = { .valuespace = TVS_TEMP,     .offset = 0, .type = t2 }, 
                                .src = { .valuespace = TVS_ARGUMENT, .offset = 0, .type = t1 },
                            }
                        });
                        tirfn->temps_count += 1;
                        lhs = { .valuespace = TVS_TEMP, .offset = 0, .type = t2 };
                    } else if (t2->size < t1->size) {
                        tirfn->emit({ 
                            .opcode = TOPC_ZEXT,
                            .un = { 
                                .dst = { .valuespace = TVS_TEMP,     .offset = 0, .type = t1 }, 
                                .src = { .valuespace = TVS_ARGUMENT, .offset = 1, .type = t2 },
                            }
                        });
                        tirfn->temps_count += 1;
                        rhs = { .valuespace = TVS_TEMP, .offset = 0, .type = t1 };
                    }

                    tirfn->emit({ 
                        .opcode = (TIR_OpCode)(op.opc | TOPC_UNSIGNED), 
                        .bin = { 
                            .dst = { .valuespace = TVS_RET_VALUE, .type = tirfn->returntype }, 
                            .lhs = lhs,
                            .rhs = rhs,
                        }
                    });
                    tirfn->emit({ .opcode = TOPC_RET, });

                    builtin_binary_ops.insert({ op.tok, t1, t2 }, tirfn);
                }
            }
        }
    }
} _;
