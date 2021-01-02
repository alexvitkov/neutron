#ifndef BYTECODE_H
#define BYTECODE_H

#include "../../common.h"
#include "../../ast.h"
#include "../../ds.h"

enum BC_OpCode : u8 {
    OPC_NONE, OPC_MOV, OPC_RET, OPC_ADD,
    OPC_JMP, OPC_JZ, OPC_EQ, OPC_CALL,
    OPC_SUB, OPC_LT,
};


struct BCInstr;
struct BCLoc;

struct BytecodeContext {
    Context& ctx;
    i32 frame_depth;
    arr<BCInstr> instr;
    arr<u64> labels;
    map<const char*, u32> fnaddr;
    map<ASTNode*, BCLoc> locations;

    BytecodeContext(GlobalContext& global) : ctx(global) { }

    struct FnData {
        ASTNode* node;
        u32 label;
        u32 frame_depth;
    };

    // TODO arr is not a great data structure for this
    arr<FnData> fn_data;

    void emit(BCInstr i);

    BCLoc alloc_offset();

    void free_offset(BCLoc loc);

    u32 new_label();

    // FnData contains a label that we need to call the function 
    // and its frame depth, so we know how much to increment the stack when calling another fn
    FnData& get_fn_data(ASTNode* node);

    void put_label(u32 label);
};

void bytecode_commpile_all(BytecodeContext& c);
u64 bytecode_interpret(BytecodeContext& c, u32 i);
void bytecode_disassemble_all(BytecodeContext& ctx);

#endif // guard
