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

struct BinaryOpKey {
    TokenType op;
    AST_Type *lhs;
    AST_Type *rhs;
};
u32 map_hash(BinaryOpKey key) { return map_hash(key.lhs) ^ (map_hash(key.rhs) << 1) ^ key.op; }
bool map_equals(BinaryOpKey a, BinaryOpKey b) { return a.lhs == b.lhs && a.rhs == b.rhs && a.op == b.op; }
map <BinaryOpKey, TIR_Builder*> binary_builders;

map<TypePair, BuiltinCast>  builtin_casts;


struct TIR_UnsignedBinOpBuilder : TIR_Builder {
    TIR_OpCode opcode;

    TIR_UnsignedBinOpBuilder(TIR_OpCode opc) : TIR_Builder(false), opcode(opc) {}

    TIR_Value emit1(TIR_Function &tirfn, arr<TIR_Value> &args, TIR_Value dst) override {
        AST_Type *lhs_type = args[0].type;
        AST_Type *rhs_type = args[1].type;

        TIR_Value lhs = args[0];
        TIR_Value rhs = args[1];

        if (lhs_type->size < rhs_type->size) {
            TIR_Value lhs_old = lhs;
            lhs = tirfn.alloc_temp(rhs_type);

            tirfn.emit({ 
                .opcode = TOPC_ZEXT,
                .un = { .dst = lhs, .src = lhs_old, }
            });
        } else if (rhs_type->size < lhs_type->size) {
            TIR_Value rhs_old = rhs;
            rhs = tirfn.alloc_temp(rhs_type);

            tirfn.emit({ 
                .opcode = TOPC_ZEXT,
                .un = { .dst = rhs, .src = rhs_old, }
            });
        }

        if (!dst) {
            dst = tirfn.alloc_temp(lhs_type);
        }

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
    }
} _;

TIR_Builder *get_builder(TokenType op, AST_Type *lhs, AST_Type *rhs, AST_Type **out) {

    switch (op) {
        case '=': {
            *out = lhs;
            return &assignment_builder;
        }

        case '+': case '-': case '*': case '/': case '%':
        { 
            MUST (lhs IS AST_PRIMITIVE_TYPE);
            MUST (rhs IS AST_PRIMITIVE_TYPE);

            AST_PrimitiveType *plhs = (AST_PrimitiveType*)lhs;
            AST_PrimitiveType *prhs = (AST_PrimitiveType*)rhs;

            TIR_Builder *builder;
            if (binary_builders.find({ op, plhs, prhs }, &builder)) {
                *out = plhs->size > prhs->size ? plhs : prhs;
                return builder;
            }

            TIR_OpCode opcode;
            switch (op) {
                case '+': opcode = TOPC_ADD; break;
                case '-': opcode = TOPC_SUB; break;
                case '*': opcode = TOPC_MUL; break;
                case '/': opcode = TOPC_DIV; break;
                case '%': opcode = TOPC_MOD; break;
                default: UNREACHABLE;
            }

            if (plhs->kind == PRIMITIVE_UNSIGNED && prhs->kind == PRIMITIVE_UNSIGNED) {
                opcode = (TIR_OpCode)(opcode | TOPC_UNSIGNED);
                TIR_UnsignedBinOpBuilder *builder = new TIR_UnsignedBinOpBuilder(opcode);
                *out = plhs->size > prhs->size ? plhs : prhs;
                binary_builders[{op, lhs, rhs}] = builder;
                return builder;
            } else {
                NOT_IMPLEMENTED();
            }
        }

        default:
            UNREACHABLE;
    }
}

