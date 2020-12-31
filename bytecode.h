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
    OPC_CALL,
    OPC_SUB,
};


enum LocPlace : u8 {
    LOC_NONE = 0,

    // The location is a fixed offset from the stack base pointer
    LOC_STACK,

    // Fixed offset relative to (base pointer + stack frame size)
    // This is used to pass arguments to functions we call
    // It's needed because our function is not yet compiled and we don't know
    // how big our stack frame is
    LOC_NEXTSTACK,

    // The location is described by a label, see CompileContext.label
    LOC_LABEL,

    // A 64 bit fixed value
    LOC_VALUE,
};

// @VOLATILE
// This struct has be at most 16 bytes
// If it becomes larger, increase ASTVar's 'location' struct member
struct Loc {
    LocPlace place;
    union {
        i64 stack_offset;
        u64 label;
        u64 value;
    };

    bool operator ==(const Loc& other) const {
        return place == other.place == value == other.value;
    }
    bool operator !=(const Loc& other) const {
        return !(*this == other);
    }
};

struct Instr {
    OpCode opcode;
    Loc dst;
    Loc src;
};

void disassemble(Instr* instr);

struct CompileContext {
    Context& ctx;
    u32 frame_depth;
    arr<Instr> instr;
    strtable<u32> fnaddr;

    arr<u64> labels;

    // TODO arr is not a great data structure for this
    struct FnData {
        ASTNode* node;
        u64 label;
        u64 frame_depth;
    };
    arr<FnData> fn_data;

    void emit(Instr i) {
        instr.push(i);
    }

    Loc alloc_offset() {
        Loc offset = { .place = LOC_STACK, .stack_offset = frame_depth };
        frame_depth += 8;
        return offset;
    }

    void free_offset(Loc loc) {
    }

    u32 new_label() {
        labels.push(0);
        return labels.size - 1;
    }

    // FnData contains a label that we need to call the function 
    // and its frame depth, so we know how much to increment the stack when calling another fn
    FnData& get_fn_data(ASTNode* node) {
        u64 l;
        for (FnData& e : fn_data) {
            if (e.node == node)
                return e;
        }
        return fn_data.push({ node, new_label() });
    }

    void put_label(u32 label) {
        labels[label] = instr.size;
        // printf("Label%u:\n", label);
    }

    // TODO this is bad and won't work for long
    void disassemble_all() {
        for (u32 i = 0; i < instr.size; i++) {
            for (u32 j = 0; j < labels.size; j++) {
                if (labels[j] == i)
                    printf("Label%lu\n", j);
            }
            disassemble(&instr[i]);
        }
    }
};

void compile_all(CompileContext& c);
u64 interpret(CompileContext& c, u32 i);

#endif // guard
