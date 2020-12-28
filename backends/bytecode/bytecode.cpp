#include "../../common.h"
#include "../../ast.h"
#include "bytecode.h"

#include <algorithm>


union Location {
    u8 reg;
    struct {
        bool resolved;
        union {
            u32 address;
            u32 label;
        };
    } mem;
};

struct Instruciton {
    OpCode opcode;
    AddrMode addrmode;
    Location src;
    Location dst;
};


struct Emitter {
    std::unordered_map<u32,u32> resolved_labels;
};

void* bytecode_compile(Context& global, u32 mem_size, u32 code_start, u32 data_start, u32 stack_start) {
    u8* mem = (u8*)malloc(mem_size);
    memset(mem, 0, mem_size);

    for (const auto& decl : global.defines) {
        print(std::cout, decl.second, true);
        std::cout << std::endl;
    }
    
    return 0;
}

/*

void emitexit(u8** c, u8 code) {
    u8* s = *c;
    s[0] = OP_EXIT;
    s[1] = code;
    *c += 8;
}

void emitcall(u8** c, u64 ptr) {
    u8* s = *c;
    s[0] = OP_CALL; 
    ((u64*)s)[1] = ptr;
    *c += 16;
}

void emitret(u8** c, u64 ptr) {
    u8* s = *c;
    s[0] = OP_RET;
    *c += 8;
}

void emitr2r(u8** c, opcode op, u8 dstreg, u8 srcreg) {
    u8* s = *c;
    s[0] = op;
    s[1] = AM_REG2REG;
    s[2] = dstreg;
    s[3] = srcreg;
    *c += 8;
}

void emitr2m(u8** c, opcode op, u32 dstmem, u8 srcreg) {
    u8* s = *c;
    s[0] = op;
    s[1] = AM_REG2MEM;
    s[3] = srcreg;
    ((u32*)s)[1] = dstmem;
    *c += 8;
}

void emitm2r(u8** c, opcode op, u32 srcmem, u8 dstreg) {
    u8* s = *c;
    s[0] = op;
    s[1] = AM_MEM2REG;
    s[3] = dstreg;
    ((u32*)s)[1] = srcmem;
    *c += 8;
}

// | OPCODE 0x04 0x00 DSTREG | 0x00 0x00 0x00 0x00 | VAL (8 bytes) |
void emitv2r(u8** c, opcode op, u64 srcval, u8 dstreg) {
    u8* s = *c;
    s[0] = op;
    s[1] = AM_VAL2REG;
    s[3] = dstreg;
    ((u64*)s)[1] = srcval;
    *c += 16;
}
*/ 
