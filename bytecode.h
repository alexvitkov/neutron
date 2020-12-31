#ifndef BYTECODE_H
#define BYTECODE_H

#include "common.h"
#include "ast.h"
#include "ds.h"



// @VOLATILE
// If you change those add an entry in opcode_names
enum OpCode : u8 {
    OPC_NONE,
    OPC_MOV,
    OPC_RET,
    OPC_ADD,
    OPC_JMP,
    OPC_JZ,
    OPC_EQ,
};


enum LocPlace : u8 {
    LOC_NONE = 0,
    LOC_STACK,
    LOC_LABEL,
    LOC_VALUE,
};

// @VOLATILE
// This struct has be at most 16 bytes
// If it becomes larger, increase ASTVar's 'location' struct member
struct Loc {
    LocPlace place;
    union {
        i64 stack_offset;
        u32 label;
        u64 value;
    };

    bool operator ==(const Loc& other) const {
        return place == other.place == value == other.value;
    }
    bool operator !=(const Loc& other) const {
        return !(*this == other);
    }
};

struct Instruction {
    OpCode opcode;
    Loc dst;
    Loc src;
};

void disassemble(Instruction* instr);

struct CompileContext {
    Context& ctx;
    u32 frame_depth;
    arr<Instruction> instr;
    strtable<u32> fnaddr;

    // TODO arr is not a great data structure for this
    arr<u64> labels;

    void emit(Instruction i) {
        disassemble(&i);
        instr.push(i);
    }

    Loc alloc_offset() {
        Loc offset = { .place = LOC_STACK, .stack_offset = frame_depth };
        frame_depth += 8;
        return offset;
    }

    void free_offset(Loc loc) {
    }

    u32 label() {
        labels.push(0);
        return labels.size - 1;
    }

    void put_label(u32 label) {
        labels[label] = instr.size;
        printf("Label%u:\n", label);
    }
};

void compile_all(CompileContext& c);
u64 interpret(CompileContext& c, u32 i);

#endif // guard
