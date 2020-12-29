#ifndef BYTECODE_H
#define BYTECODE_H

#include "../../common.h"
#include "../../ast.h"

#include <stdio.h>
#include <stdlib.h>

#define NREG 16
#define SP    (NREG - 1)
#define RRES  (NREG - 2)
#define IP    (NREG - 3)

#define RRET (NREG - 4)
#define RTMP (NREG - 5)

enum OpCode : u8 {
    OP_DISCARD = 0x00,

    // 0x00 EXITCODE(u1)
    OP_EXIT  = 0x01,

    // | OPCODE 0x00 0x00 0x00 | 0x00 0x00 0x00 0x00 | ADDR |
    // will always jump to addr
    OP_JMP,

    // | OPCODE 0x00 0x00 TESTREG | 0x00 0x00 0x00 0x00 | ADDR |
    // Will only jump if value of TESTREG is not zero
    OP_JNZ,
    // Will only jump if value of TESTREG is zero
    OP_JZ,

    OP_CALL,
    OP_RET,

    // ADDRESSABLE
    OP_ADDRESSABLE = 0x80,
    OP_ADD,
    OP_SUB,
    OP_MOV,
    OP_MULS,
    OP_MULU,
    OP_EQ,
    OP_GTS,
    OP_LTS,
    OP_GTU,
    OP_LTU,
    OP_GTF,
    OP_LTF,
    OP_GTES,
    OP_LTES,
    OP_GTEU,
    OP_LTEU,
    OP_GTEF,
    OP_LTEF,
};

enum AddrMode : u8 {

    // | OPCODE 0x01 DSTREG SRCREG | 0x00 0x00 0x00 0x00 |
    AM_REG2REG = 0x01,

    // | OPCODE 0x02 0x00 SRCREG | 0x00 0x00 MEM_OFFSET(2B) | DSTADDR |
    AM_REG2MEM = 0x02,

    // | OPCODE 0x03 DSTREG 0x00 | 0x00 0x00 MEM_OFFSET(2B) | SRCADDR |
    AM_MEM2REG = 0x03,

    // | OPCODE 0x04 DSTREG 0x00 | 0x00 0x00 0x00 0x00 | VAL (8B) |
    AM_VAL2REG = 0x04,

    // | OPCODE 0x05 0x00 0x00 | 0x00 0x00 MEM_OFFSET(2B) | ADDR (8B) | VAL (8B) |
    AM_VAL2MEM = 0x05,

    // | OPCODE 0x01 DSTREG SRCREG | 0x00 0x00 MEM_OFFSET(2B) |
    AM_REG2REGDEREFSRC = 0x06,

    // | OPCODE 0x01 DSTREG SRCREG | 0x00 0x00 MEM_OFFSET(2B) | VAL (8B) |
    AM_VAL2REGDEREF= 0x07,

    // | OPCODE 0x01 DSTREG SRCREG | 0x00 0x00 MEM_OFFSET(2B) |
    AM_REG2REGDEREFDST = 0x08,

};

struct Instr {
    OpCode op;
    AddrMode addrmode;
    u8 dstreg;
    u8 srcreg;
    u16 _zero;
    u32 mem_offset;
};

struct MemInstr : public Instr {
    void* addr;
};

struct ValInstr : public Instr {
    u64 val;
};

struct Val2MemInstr : public Instr {
    void* dstaddr;
    u64 val;
};


void* bytecode_compile(Context& ctx, void* code_start, void* stack_start);
void bytecode_disassemble(u8* start, u8* end);
int interpret(uint64_t* reg);

Loc lstack(u64 offset);
Loc lreg(u8 regid);
Loc lnode(ASTNode* node);
Loc lval(u64 num);

struct Emitter {
    std::unordered_map<ASTNode*, std::vector<void**>> waiting;
    std::unordered_map<ASTNode*, void*> addresses;

    void emitexit(u8 code);
    void emitcall(u64 ptr);
    void emit(OpCode op, Loc dst, Loc src);

    Context& global;
    u8 *s;
    u64* stack;

    void nodeaddr(ASTNode* node, void** addr);

    Emitter(Context& global, void* code_start, void* stack_start);
};

#endif // guard
