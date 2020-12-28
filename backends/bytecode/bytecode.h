#ifndef BYTECODE_H
#define BYTECODE_H

#include "../../common.h"

#include <stdio.h>
#include <stdlib.h>

#define NREG 16
#define SP    (NREG - 1)
#define RES   (NREG - 2)
#define IP    (NREG - 3)

enum OpCode : u8 {
    // 0x00 EXITCODE(u1)
    OP_EXIT  = 0x00,

    // | OPCODE 0x00 0x00 0x00 | OFFSET (u32) |
    // will always jump to addr
    OP_JMP,

    // | OPCODE 0x00 0x00 TESTREG | OFFSET(u32) |
    // Will only jump if value of TESTREG != 0
    OP_JMPIF,

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

enum AddrMode : uint8_t {
    // ADDR is 4 bytes, REG is 1 byte, VAL is 8 bytes

    // | OPCODE 0x01 DSTREG SRCREG | 0x00 0x00 0x00 0x00 |
    AM_REG2REG = 0x01,

    // | OPCODE 0x02 0x00 SRCREG | DSTADDR |
    AM_REG2MEM = 0x02,

    // | OPCODE 0x03 0x00 DSTREG | SRCADDR |
    AM_MEM2REG = 0x03,

    // | OPCODE 0x04 0x00 DSTREG | 0x00 0x00 0x00 0x00 | VAL (8 bytes) |
    AM_VAL2REG = 0x04,

    // | OPCODE 0x04 0x00 0x00 | DSTREG (4 bytes) | VAL (8 bytes) |
    AM_VAL2MEM = 0x05,
};

void* bytecode_compile(Context& ctx, u32 mem_size, u32 code_start, u32 data_start, u32 stack_start);
int interpret(uint64_t* reg);

struct Emitter {
    std::unordered_map<ASTNode*, std::vector<u32*>> waiting;
    std::unordered_map<ASTNode*, u32> addresses;

    void emitexit(u8 code);
    void emitcall(u64 ptr);
    void emitret(u64 ptr);
    void emitr2r(OpCode op, u8 dstreg, u8 srcreg);
    void emitr2m(OpCode op, u32 dstmem, u8 srcreg);
    void emitm2r(OpCode op, u32 srcmem, u8 dstreg);
    void emitv2r(OpCode op, u64 srcval, u8 dstreg);

    Context& global;
    u8 *mem, *s;

    Emitter(Context& global, u32 mem_size, u32 code_start, u32 data_start, u32 stack_start);
};

#endif // guard
