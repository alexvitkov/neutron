#ifndef TIR_H
#define TIR_H

#include "common.h"
#include "ast.h"
#include "context.h"

#include <initializer_list>

enum TIR_ValueSpace : u8 {
    TVS_DISCARD = 0x00,
    TVS_GLOBAL,
    TVS_ARGUMENT,
    TVS_RET_VALUE,
    TVS_TEMP,
    TVS_VALUE,
    TVS_STACK,
    TVS_C_STRING_LITERAL,
    TVS_AST_VALUE,
};

enum TIR_Value_Flags : u32 {
     TVF_BYVAL = 0x01,
};

#define TOPC_MODIFIES_DST_BIT 0x8000
#define TOPC_BINARY (0x4000 | TOPC_MODIFIES_DST_BIT)
#define TOPC_UNARY  (0x2000 | TOPC_MODIFIES_DST_BIT)

#define TOPC_FLOAT    0x1000
#define TOPC_SIGNED   0x0800
#define TOPC_UNSIGNED 0x0400

enum TIR_OpCode : u16 {
    TOPC_NONE       = 0x0000,

    TOPC_ADD        = 0x00 | TOPC_BINARY,
    TOPC_SUB,
    TOPC_MUL,
    TOPC_DIV,
    TOPC_MOD,
    TOPC_SHL,
    TOPC_SHR,
    TOPC_EQ,
    TOPC_LT,
    TOPC_LTE,
    TOPC_GT,
    TOPC_GTE,

    TOPC_UADD = TOPC_ADD | TOPC_UNSIGNED,
    TOPC_USUB = TOPC_SUB | TOPC_UNSIGNED,
    TOPC_UMUL = TOPC_MUL | TOPC_UNSIGNED,
    TOPC_UDIV = TOPC_DIV | TOPC_UNSIGNED,
    TOPC_UMOD = TOPC_MOD | TOPC_UNSIGNED,
    TOPC_UEQ  = TOPC_EQ  | TOPC_UNSIGNED,
    TOPC_ULT  = TOPC_LT  | TOPC_UNSIGNED,
    TOPC_ULTE = TOPC_LTE | TOPC_UNSIGNED,
    TOPC_UGT  = TOPC_GT  | TOPC_UNSIGNED,
    TOPC_UGTE = TOPC_GTE | TOPC_UNSIGNED,

    TOPC_FADD = TOPC_ADD | TOPC_FLOAT,
    TOPC_FSUB = TOPC_SUB | TOPC_FLOAT,
    TOPC_FMUL = TOPC_MUL | TOPC_FLOAT,
    TOPC_FDIV = TOPC_DIV | TOPC_FLOAT,
    TOPC_FMOD = TOPC_MOD | TOPC_FLOAT,
    TOPC_FEQ  = TOPC_EQ  | TOPC_FLOAT,
    TOPC_FLT  = TOPC_LT  | TOPC_FLOAT,
    TOPC_FLTE = TOPC_LTE | TOPC_FLOAT,
    TOPC_FGT  = TOPC_GT  | TOPC_FLOAT,
    TOPC_FGTE = TOPC_GTE | TOPC_FLOAT,

    TOPC_SADD = TOPC_ADD | TOPC_SIGNED,
    TOPC_SSUB = TOPC_SUB | TOPC_SIGNED,
    TOPC_SMUL = TOPC_MUL | TOPC_SIGNED,
    TOPC_SDIV = TOPC_DIV | TOPC_SIGNED,
    TOPC_SMOD = TOPC_MOD | TOPC_SIGNED,
    TOPC_SEQ  = TOPC_EQ  | TOPC_SIGNED,
    TOPC_SLT  = TOPC_LT  | TOPC_SIGNED,
    TOPC_SLTE = TOPC_LTE | TOPC_SIGNED,
    TOPC_SGT  = TOPC_GT  | TOPC_SIGNED,
    TOPC_SGTE = TOPC_GTE | TOPC_SIGNED,


    TOPC_MOV     = 0x00 | TOPC_UNARY,
    TOPC_BITCAST = 0x01 | TOPC_UNARY,
    TOPC_ZEXT    = 0x02 | TOPC_UNARY,
    TOPC_SEXT    = 0x03 | TOPC_UNARY,

    TOPC_JMPIF      = 0x01,
    TOPC_JMP        = 0x02,
    TOPC_RET        = 0x03,
    TOPC_LOAD       = 0x04,
    TOPC_STORE      = 0x05,

    TOPC_CALL,
    TOPC_GEP,
};


struct TIR_Value {
    TIR_ValueSpace valuespace;
    TIR_Value_Flags flags;

    u64 offset;
    AST_Type* type;

    operator bool();
};

// For the implementation of all these we consider 2 values equal 
// if they have the same valuespace and offset
bool operator== (TIR_Value& lhs, TIR_Value& rhs);
bool operator!= (TIR_Value& lhs, TIR_Value& rhs);
u32 map_hash(TIR_Value data);
bool map_equals(TIR_Value lhs, TIR_Value rhs);

struct TIR_Block;
struct TIR_Function;

struct TIR_Instruction {
    TIR_OpCode opcode;
    union {
        struct { char _; } none;

        // VOLATILE
        // All union members with a TIR_Value destination MUST have it as the first field in the struct
        //
        // That way if we know the opcode if the instruction has TOPC_MODIFIES_DST_BIT set,
        // then we can just use the union .dst, regardless of whether the instruction a unary/binary/call/whatever
        TIR_Value dst;

        struct {
            TIR_Value dst, lhs, rhs;
        } bin;

        struct {
            TIR_Value dst, src;
        } un;

        struct {
            TIR_Value dst;
            TIR_Function *fn;
            arr_ref<TIR_Value> args;
        } call;

        struct {
            TIR_Block *next_block;
        } jmp;

        struct {
            TIR_Value cond;
            TIR_Block *then_block, *else_block;
        } jmpif;

        struct {
            TIR_Value dst, base;
            arr_ref<TIR_Value> offsets;
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

    void push_previous(TIR_Block *previous);
};

struct TIR_Function;

struct TIR_ExecutionStorage {
    map<u64, void*> global_values;
};

struct TIR_Context {
    AST_GlobalContext &global;
    map<AST_Fn*, TIR_Function*> fns;

    u64 globals_count = 0;
    map<AST_Value*, TIR_Value> global_valmap;

    map<u64, TIR_Value> _global_initial_values;

    map<u64, HeapJob*> global_initializer_running_jobs;

    TIR_ExecutionStorage storage;

    void compile_all(); // TODO DELETE
    HeapJob *compile_fn(AST_Fn *fn, HeapJob *fn_typecheck_job);

    TIR_Value append_global(AST_Var *var);

};

void add_string_global(TIR_Context *tir_context, AST_Var *the_string_var, AST_StringLiteral *the_string_literal);

struct TIR_Function {
    TIR_Context *tir_context;
    AST_Fn* ast_fn;

    TIR_Value retval = { .valuespace = TVS_RET_VALUE };
    TIR_Block* writepoint;
    u64 temps_count = 0;
    HeapJob *compile_job;

    // right now only set for th efunctions generated by tir_builtins.cpp
    AST_Type *returntype;

    bool is_inline = false;

    struct VarValTuple {
        AST_Var *var;
        TIR_Value val;
    };
    u64 stack_size = 0;
    arr<VarValTuple> stack;

    map<AST_Value*, TIR_Value> fn_valmap;

    arr<TIR_Value> parameters;

    // Blocks are stored in the order they need to be compiled in
    arr<TIR_Block*> blocks;

    TIR_Function(TIR_Context *c, AST_Fn* fn) : ast_fn(fn), tir_context(c) {};
    TIR_Function(std::initializer_list<TIR_Instruction> instrs);

    TIR_Value alloc_temp(AST_Type* type);
    TIR_Value alloc_stack(AST_Var* type);
    void emit(TIR_Instruction instr);

    void compile_signature();
    void compile();

    void print();
};

struct TIR_Builder {
    bool use_emit2;
    AST_Type *rettype;

    TIR_Builder(bool use_emit) : use_emit2(use_emit) {}

    virtual TIR_Value emit1(TIR_Function &tirfn, arr<TIR_Value> &args, TIR_Value dst) {
        UNREACHABLE;
    }
    virtual TIR_Value emit2(TIR_Function &tirfn, arr<AST_Value*> &args, TIR_Value dst) {
        UNREACHABLE;
    }
};


struct TIR_ExecutionJob : Job {
    struct StackFrame {
        TIR_Function *fn;
        TIR_Block    *block;
        void         *retval;
        u32           next_instruction;
        u8           *stack;
        arr<void*>    args, tmp;

        void  set_value(TIR_Value key, void *val);
        void *get_value(TIR_Value key); // TODO POINTERSIZE
    };

    TIR_Context *tir_context;
    bool has_next_retval = false;
    void *next_retval = nullptr;

    TIR_ExecutionJob(TIR_Context *tir_context);

    arr<StackFrame> stackframes;
    void call(TIR_Function *tir_fn, arr<void*> &args);

    virtual bool run(Message *message) override;
    virtual void on_complete(void *value) = 0;
};

TIR_Value compile_node_rvalue(TIR_Function& fn, AST_Node* node, TIR_Value dst);
bool get_location(TIR_Function &fn, AST_Value *val, TIR_Value *out);


std::wostream& operator<< (std::wostream& o, TIR_Value loc);
std::wostream& operator<< (std::wostream& o, TIR_Instruction& instr);

#endif // guard
