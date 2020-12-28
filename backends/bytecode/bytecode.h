#ifndef BYTECODE_H
#define BYTECODE_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#define NREG 16
#define SP    (NREG - 1)
#define RES   (NREG - 2)
#define IP    (NREG - 3)

enum opcode : u8 {
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

enum addrmode : uint8_t {
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


void printreg(uint64_t* reg);
void emitexit(uint8_t** c, uint8_t code);
void emitcall(uint8_t** c, uint64_t ptr);
void emitret(uint8_t** c, uint64_t ptr);
void emitr2r(uint8_t** c, opcode op, uint8_t dstreg, uint8_t srcreg);
void emitr2m(uint8_t** c, opcode op, uint32_t dstmem, uint8_t srcreg);
void emitm2r(uint8_t** c, opcode op, uint32_t srcmem, uint8_t dstreg);
void emitv2r(uint8_t** c, opcode op, uint64_t srcval, uint8_t dstreg);

int interpret(uint64_t* reg, uint8_t* mem);

#endif // guard