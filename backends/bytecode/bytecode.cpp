#include "../../common.h"
#include "../../ast.h"
#include "bytecode.h"

#include <algorithm>




void compile_fn(Emitter& em, ASTFn* fn) {

    // TODO check if already compilerd

    u32 addr = (u32)(em.s - em.mem);

    auto it = em.waiting.find(fn);
    if (it != em.waiting.end()) {
        auto v = it->second;
        for (u32* pos : v) {
            *pos = addr;
        }
        em.waiting.erase(fn);
    }
    em.addresses.insert(std::make_pair<>((ASTNode*)fn, addr));

    u32 frame_size = 0;

    // Locate the stack frame
    for (const auto& decl : fn->block->ctx->defines) {
        if (decl.second->nodetype == AST_VAR) {
            ASTVar* var = (ASTVar*)decl.second;
            if (var->argindex >= 0) {
                var->location = { VL_REGISTER, (u16)var->argindex };
            }
            else {
                var->location = { VL_STACK, (u16)frame_size };
                frame_size += 8;
            }
            printf("%s at %d:%d\n", var->name, var->location.type, var->location.offset);
        }
        else if (decl.second->nodetype == AST_FN) {
            compile_fn(em, (ASTFn*)decl.second);
        }
    }
}

void* bytecode_compile(Context& global, u32 mem_size, u32 code_start, u32 data_start, u32 stack_start) {
    Emitter em(global, mem_size, code_start, data_start, stack_start);

    for (const auto& decl : global.defines) {
        if (decl.second->nodetype == AST_VAR) {
            // TODO global variable
        }
        else if (decl.second->nodetype == AST_FN) {
            compile_fn(em, (ASTFn*)decl.second);
        }
    }
    
    return 0;
}


Emitter::Emitter(Context& global, u32 mem_size, u32 code_start, u32 data_start, u32 stack_start) : global(global) {
    mem = (u8*)malloc(mem_size);
    memset(mem, 0, mem_size);
    s = mem;
}


void Emitter::emitexit(u8 code) {
    s[0] = OP_EXIT;
    s[1] = code;
    s += 8;
}

void Emitter::emitcall(u64 ptr) {
    s[0] = OP_CALL; 
    ((u64*)s)[1] = ptr;
    s += 16;
}

void Emitter::emitret(u64 ptr) {
    s[0] = OP_RET;
    s += 8;
}

void Emitter::emitr2r(OpCode op, u8 dstreg, u8 srcreg) {
    s[0] = op;
    s[1] = AM_REG2REG;
    s[2] = dstreg;
    s[3] = srcreg;
    s += 8;
}

void Emitter::emitr2m(OpCode op, u32 dstmem, u8 srcreg) {
    s[0] = op;
    s[1] = AM_REG2MEM;
    s[3] = srcreg;
    ((u32*)s)[1] = dstmem;
    s += 8;
}

void Emitter::emitm2r(OpCode op, u32 srcmem, u8 dstreg) {
    s[0] = op;
    s[1] = AM_MEM2REG;
    s[3] = dstreg;
    ((u32*)s)[1] = srcmem;
    s += 8;
}

// | OPCODE 0x04 0x00 DSTREG | 0x00 0x00 0x00 0x00 | VAL (8 bytes) |
void Emitter::emitv2r(OpCode op, u64 srcval, u8 dstreg) {
    s[0] = op;
    s[1] = AM_VAL2REG;
    s[3] = dstreg;
    ((u64*)s)[1] = srcval;
    s += 16;
}
