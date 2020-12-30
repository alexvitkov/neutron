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

// @VOLATILE
// If you add new instructions or change their order
// make sure to add the entry in instruction_names

// If you're adding new instructions it matters whether you add them
// before or after OP_ADDRESSABLE, see its comment
enum OpCode : u8 {
    OP_NONE = 0x00,

    // | OPCODE EXITCODE(u1) 0x00 0x00 | 0x00 0x00 0x00 0x00 |
    OP_EXIT  = 0x01,

    // | OPCODE 0x00 0x00 0x00 | 0x00 0x00 0x00 0x00 | ADDR |
    // JMP is an unconditional jump to ADDR
    OP_JMP,

    // JNZ will only jump if the value of TESTREG is non-zero
    // JZ jumps if it is zero
    // Right now for TESTREG we only use the 'RRES' register
    // that's similar to how x86 handles it with the FLAGS register
    // | OPCODE 0x00 0x00 TESTREG | 0x00 0x00 0x00 0x00 | ADDR |
    OP_JNZ,
    OP_JZ,

    OP_CALL,
    OP_RET,

    // The instructions after OP_ADDRESSABLE take in a source and destination
    // You can checck whether an instruction is addressable with OPCODE & OP_ADDRESSABLE
    OP_ADDRESSABLE = 0x80,

    OP_ADD = OP_ADDRESSABLE,
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

extern const char* instruction_names[256];

enum AddrModeBits : u8 {
    // lower 4 bits describe the source
    AM_SRC_BITS = 0x0F,
    AM_SRCREG   = 0x01,
    AM_SRCDEREF = 0x01 | 0x02,
    AM_SRCMEM   = 0x04,
    AM_SRCVAL   = 0x08,

    // upper 4 bits describe the destination
    AM_DST_BITS = 0xF0,
    AM_DSTREG   = 0x10,
    AM_DSTDEREF = 0x10 | 0x20,
    AM_DSTMEM   = 0x40,
};

// Instr is the smallest instruction supported, currently 16 bytes long.
// Its layout looks like | OPCODE ADDRMODE DSTREG SRCREG | 0x00 0x00 OFFSET (2 bytes) |
// @POINTERSIZE - We're assuming REGSIZE = POINTERSIZE = 64
struct Instr {
    OpCode op;

    // addrmode consists of two parts - source and destination
    // The addrmode of an instruciton that has register as src and dst is (AM_SRCREG | AM_DSTREG)
    u8 addrmode;

    u8 dstreg;
    u8 srcreg;
    u16 _zero;

    // For instructions that address memory via a register (address stored in resister)
    // this offset is added to the value of the address.
    // This is useful when targeting variables that live in the stack
    // we use the BP register plus a fixed offset that describes which variable we want
    u32 mem_offset;
};

// Instructions that have a 64 bit value as their source AND an address as their value
// Right now their size is sizeof(Instr) + 16
struct Val2MemInstr : public Instr {
    void* addr;
    u64 val;
};

// Instructions that address memory are of type MemInstr
// Right now their size is sizeof(Instr) + 8
struct MemInstr : public Instr {
    void* addr;
};

// Instructions that have a 64 bit value as their source are of type ValInstr
// Right now their size is sizeof(Instr) + 8
struct ValInstr : public Instr {
    u64 val;
};

// The size of an addressable instruction can be inferred from its addressing mode
u8 instr_size(u8 addrmode);

void* bytecode_compile(Context& global, void* code_start, void** main_addr);
void bytecode_disassemble(u8* start, u8* end);
int interpret(u64* reg, void* main, void* stack);

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

    void nodeaddr(ASTNode* node, void** addr);

    Emitter(Context& global, void* code_start);
};

#endif // guard
