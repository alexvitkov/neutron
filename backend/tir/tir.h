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

#define TOPC_MODIFIES_DST_BIT 0x80

enum TIR_OpCode : u8 {
    TOPC_NONE       = 0x00,

    TOPC_ADD        = 0x00 | TOPC_MODIFIES_DST_BIT,
    TOPC_SUB        = 0x01 | TOPC_MODIFIES_DST_BIT,
    TOPC_EQ         = 0x02 | TOPC_MODIFIES_DST_BIT,
    TOPC_LT         = 0x03 | TOPC_MODIFIES_DST_BIT,
    TOPC_MOV        = 0x04 | TOPC_MODIFIES_DST_BIT,
    TOPC_CALL       = 0x05 | TOPC_MODIFIES_DST_BIT,
    TOPC_PTR_OFFSET = 0x06 | TOPC_MODIFIES_DST_BIT,
    TOPC_BITCAST    = 0x07 | TOPC_MODIFIES_DST_BIT,

    TOPC_JMPIF      = 0x01,
    TOPC_JMP        = 0x02,
    TOPC_RET        = 0x03,
    TOPC_LOAD       = 0x04,
    TOPC_STORE      = 0x05,
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

        // VOLATILE
        // All union members with a TIR_Value* destination MUST have it as the first field in the struct
        //
        // That way if we know the opcode if the instruction has TOPC_MODIFIES_DST_BIT set,
        // then we can just use the union .dst, regardless of whether the instruction a unary/binary/call/whatever
        TIR_Value *dst;

        struct {
            TIR_Value *dst, *lhs, *rhs;
        } bin;

        struct {
            TIR_Value *dst, *src;
        } un;

        struct {
            TIR_Value *dst;
            AST_Fn *fn;
            arr_ref<TIR_Value*> args;
        } call;

        struct {
            TIR_Block *next_block;
        } jmp;

        struct {
            TIR_Value *cond;
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
