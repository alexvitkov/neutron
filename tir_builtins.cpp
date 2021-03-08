#include "tir_builtins.h"
#include "cast.h"
#include "typer.h"
#include "tir.h"

struct UnaryOpKey {
    TokenType op;
    AST_Type *inner;
};
u32 map_hash(UnaryOpKey key) { return map_hash(key.inner) ^ key.op; }
bool map_equals(UnaryOpKey a, UnaryOpKey b) { return a.inner == b.inner && a.op == b.op; }

u32 map_hash(BinaryOpKey key) { return map_hash(key.lhs) ^ (map_hash(key.rhs) << 1) ^ key.op; }
bool map_equals(BinaryOpKey a, BinaryOpKey b) { return a.lhs == b.lhs && a.rhs == b.rhs && a.op == b.op; }
map <BinaryOpKey, TIR_Builder*> binary_builders;

map<TypePair, BuiltinCast>  builtin_casts;


struct TIR_CommonBinOpBuilder : TIR_Builder {
    TIR_OpCode opcode;

    TIR_CommonBinOpBuilder(TIR_OpCode opc) : TIR_Builder(false), opcode(opc) {}

    TIR_Value emit1(TIR_Function &tirfn, arr<TIR_Value> &args, TIR_Value dst) override {
        TIR_Value lhs = args[0];
        TIR_Value rhs = args[1];

        assert(lhs.type == rhs.type);

        if (!dst)
            dst = tirfn.alloc_temp(lhs.type);

        tirfn.emit({ 
            .opcode = (TIR_OpCode)(opcode), 
            .bin = { .dst = dst, .lhs = lhs, .rhs = rhs, }
        });
        return dst;
    }
};

struct TIR_AssignmentBuilder : TIR_Builder {

    TIR_AssignmentBuilder() : TIR_Builder(true) {};

    TIR_Value emit2(TIR_Function &fn, arr<AST_Value*> &args, TIR_Value dst) override {
        TIR_Value ptrloc;

        AST_Value *lhs = args[0];
        AST_Value *rhs = args[1];

        if (get_location(fn, lhs, &ptrloc)) {
            TIR_Value tmp = compile_node_rvalue(fn, rhs, {});

            fn.emit({ 
                .opcode = TOPC_STORE, 
                .un = {
                    .dst = ptrloc,
                    .src = tmp,
                }
            });

            if (dst) {
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = tmp } });
                return dst;
            } else {
                return tmp;
            }
        }
        else {
            TIR_Value lhsloc = compile_node_rvalue(fn, lhs, {});
            TIR_Value valloc = compile_node_rvalue(fn, rhs, lhsloc);

            if (dst) {
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = valloc } });
                return dst;
            } else {
                return ptrloc;
            }
        }
    }
} assignment_builder;


template <typename T>
inline bool number_literal_to_unsigned(AST_GlobalContext &global, AST_Value *src, AST_Value **dst, AST_Type *t) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)src;
    assert(nl IS AST_NUMBER);

    T num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        return false;
    }

    // TODO LOCATION
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

bool zext_to_u16(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    *dst = global.alloc<AST_ZeroExtend>(src, &t_u16);
    return true;
}
bool zext_to_u32(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    *dst = global.alloc<AST_ZeroExtend>(src, &t_u32);
    return true;
}
bool zext_to_u64(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    *dst = global.alloc<AST_ZeroExtend>(src, &t_u64);
    return true;
}

struct Initializer {
    Initializer() {
        builtin_casts.insert({ &t_number_literal, &t_u64 }, { number_literal_to_u64, 100 });
        builtin_casts.insert({ &t_number_literal, &t_u32 }, { number_literal_to_u32, 90 });
        builtin_casts.insert({ &t_number_literal, &t_u16 }, { number_literal_to_u16, 80 });
        builtin_casts.insert({ &t_number_literal, &t_u8  }, { number_literal_to_u8,  70 });

        builtin_casts.insert({ &t_u8,  &t_u16 }, { zext_to_u16,  80  });
        builtin_casts.insert({ &t_u8,  &t_u32 }, { zext_to_u32,  90  });
        builtin_casts.insert({ &t_u8,  &t_u64 }, { zext_to_u64,  100 });
        builtin_casts.insert({ &t_u16, &t_u32 }, { zext_to_u32,  90  });
        builtin_casts.insert({ &t_u16, &t_u64 }, { zext_to_u64,  100 });
        builtin_casts.insert({ &t_u32, &t_u64 }, { zext_to_u64,  100 });

        struct OperatorOpcode {
            TokenType op;
            TIR_OpCode opcode;
        };

        arr<OperatorOpcode> opcs = {
            { (TokenType)'+', TOPC_ADD },
            { (TokenType)'-', TOPC_SUB },
            { (TokenType)'*', TOPC_MUL },
            { (TokenType)'/', TOPC_DIV },
            { (TokenType)'%', TOPC_MOD },
        };

        TIR_Builder *builder;

        for (auto &p : opcs) {
            for (AST_Type * t : unsigned_types) {
                TIR_OpCode opcode = (TIR_OpCode)(p.opcode | TOPC_UNSIGNED);
                TIR_CommonBinOpBuilder *builder = new TIR_CommonBinOpBuilder(opcode);
                builder->rettype = t;
                binary_builders[{p.op, t, t}] = builder;
            }

            for (AST_Type * t : signed_types) {
                TIR_OpCode opcode = (TIR_OpCode)(p.opcode | TOPC_SIGNED);
                TIR_CommonBinOpBuilder *builder = new TIR_CommonBinOpBuilder(opcode);
                builder->rettype = t;
                binary_builders[{p.op, t, t}] = builder;
            }
        }

    }
} _;
