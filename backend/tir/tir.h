#ifndef TIR_H
#define TIR_H

#include "../../common.h"
#include "../../ast.h"

enum TIR_ValueSpace : u8 {
    TVS_DISCARD = 0x00,
    TVS_ARGUMENT,
    TVS_NEXTARGUMENT,
    TVS_RET_VALUE,
    TVS_TEMP,
    TVS_VALUE,
};

enum TIR_OpCode : u8 {
    TOPC_ADD,
    TOPC_SUB,
    TOPC_RET,
    TOPC_MOV,
};

struct TIR_Value {
    TIR_ValueSpace valuespace;
    u64 offset;
    AST_Type* type;
};

struct TIR_Instruction {
    TIR_OpCode opcode;
    union {
        struct {
            TIR_Value *dst, *lhs, *rhs;
        } bin;

        struct {
            TIR_Value *dst, *src;
        } un;
    };
};

struct TIR_Block {
    arr<TIR_Instruction> instructions;
    bucketed_arr<TIR_Value> values;
    TIR_Block* parent;
};

struct TIR_Function;

struct TIR_Context {
    GlobalContext& global;
    map<AST_Fn*, TIR_Function*> fns;

    void compile_all();
};

struct TIR_Function {
    TIR_Context& c;
    AST_Fn* ast_fn;

    TIR_Value retval = { .valuespace = TVS_RET_VALUE };
    TIR_Block* writepoint;
    TIR_Block* entry;
    u64 temp_offset = 0;
    map<AST_Node*, TIR_Value*> valmap;

    TIR_Function(TIR_Context& c, AST_Fn* fn) : ast_fn(fn), c(c) {};

    TIR_Value* alloc_temp(AST_Type* type);
    TIR_Value* alloc_val(TIR_Value val);
    void free_temp(TIR_Value* val);
    void emit(TIR_Instruction instr);
};

std::ostream& operator<< (std::ostream& o, TIR_Value& loc);
std::ostream& operator<< (std::ostream& o, TIR_Instruction& instr);

#endif // guard
