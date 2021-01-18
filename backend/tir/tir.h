#ifndef TIR_H
#define TIR_H

#include "../../common.h"
#include "../../ast.h"

enum TIR_ValueSpace : u8 {
    TVS_DISCARD = 0x00,
    TVS_GLOBAL,
    TVS_ARGUMENT,
    TVS_RET_VALUE,
    TVS_TEMP,
    TVS_VALUE,
};

enum TIR_OpCode : u8 {
    TOPC_ADD,
    TOPC_SUB,
    TOPC_EQ,
    TOPC_RET,
    TOPC_MOV,
    TOPC_CALL,
    TOPC_LOAD,
    TOPC_STORE,
    TOPC_JMPIF,
    TOPC_JMP,
    TOPC_PTR_OFFSET,
};

struct TIR_Value {
    TIR_ValueSpace valuespace;
    u64 offset;
    AST_Type* type;
};

struct TIR_Block;

struct TIR_Instruction {
    TIR_OpCode opcode;
    union {
        struct { char _; } none;

        struct {
            TIR_Value *dst, *lhs, *rhs;
        } bin;

        struct {
            TIR_Value *dst, *src;
        } un;

        struct {
            TIR_Value* dst;
            AST_Fn* fn;
            arr_ref<TIR_Value*> args;
        } call;

        struct {
            TIR_Block* next_block;
        } jmp;

        struct {
            TIR_Value* cond;
            TIR_Block *then_block, *else_block;
        } jmpif;

        struct {
            TIR_Value *dst, *base;
            arr_ref<TIR_Value*> offsets;
        } gep;
    };
};

struct TIR_Block {
    // This is only used for printing
    u64 id;

    arr<TIR_Instruction> instructions;

    // A list of all blocks that can jump into this one
    // This is used to generate the PHI instructions in LLVM
    arr<TIR_Block*> previous_blocks;

    TIR_Block();
};

struct TIR_Function;

struct TIR_Context {
    GlobalContext& global;
    map<AST_Fn*, TIR_Function*> fns;

    bucketed_arr<TIR_Value> values;
    map<AST_Value*, TIR_Value*> valmap;

    void compile_all();
};

struct TIR_Function {
    TIR_Context& c;
    AST_Fn* ast_fn;

    TIR_Value retval = { .valuespace = TVS_RET_VALUE };
    TIR_Block* writepoint;
    u64 temp_offset = 0;
    map<AST_Value*, TIR_Value*> _valmap;

    // Blocks are stored in the order they need to be compiled in
    arr<TIR_Block*> blocks;

    // We use the bucketed_arr here, so we can pass around pointers to TIR_Value safely
    bucketed_arr<TIR_Value> values;

    TIR_Function(TIR_Context& c, AST_Fn* fn) : ast_fn(fn), c(c) {};

    TIR_Value* alloc_temp(AST_Type* type);
    TIR_Value* alloc_val(TIR_Value val);
    void free_temp(TIR_Value* val);
    void emit(TIR_Instruction instr);

    void print();
};

std::ostream& operator<< (std::ostream& o, TIR_Value& loc);
std::ostream& operator<< (std::ostream& o, TIR_Instruction& instr);

#endif // guard
