#include "../../common.h"
#include "../../ast.h"
#include "bytecode.h"

#include <algorithm>
#include <assert.h>

void compile_expr(Emitter& em, Loc dst, ASTNode *expr) {

    switch (expr->nodetype) {

        case AST_NUMBER: {
            ASTNumber* num = (ASTNumber*)expr;
            em.emit(OP_MOV, dst, lval(num->floorabs));
            break;
        }

        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)expr;

            compile_expr(em, dst, bin->lhs);
            compile_expr(em, lreg(TMPREG), bin->rhs);

            switch (bin->op) {
                case '+':
                    em.emit(OP_ADD, dst, lreg(TMPREG));
                    break;

                default:
                    assert(!"not impl 5");
                    break;
            }
            break;
        }

        default:
            assert(!"not impl 6");
            break;
    }

}

void Emitter::emit(OpCode op, Loc dst, Loc src) {
    switch (src.type) {
        case Loc::REGISTER: {
            switch (dst.type) {
                case Loc::REGISTER: {
                    Instr* i = (Instr*)s;
                    *i = {
                        .op = op,
                        .addrmode = AM_REG2REG,
                        .dstreg = dst.reg,
                        .srcreg = src.reg,
                    };
                    s += sizeof(*i);
                    break;
                }
                case Loc::NODE: {
                    Reg2MemInstr* i = (Reg2MemInstr*)s;
                    i->op = op;
                    i->addrmode = AM_REG2MEM;
                    i->srcreg = src.reg;
                    nodeaddr(dst.node, &i->dstaddr);
                    s += sizeof(*i);
                    break;
                }
                default:
                    assert(!"not impl 1");
            }
            break;
        }
        case Loc::VALUE: {
            switch (dst.type) {
                case Loc::REGISTER: {
                    Val2RegInstr* i = (Val2RegInstr*)s;
                    i->op = op;
                    i->addrmode = AM_VAL2REG;
                    i->dstreg = dst.reg;
                    i->val = src.value;
                    s += sizeof(*i);
                    break;
                }
                default:
                    assert(!"not impl 2");
            }
            break;
        }
        default:
            assert(!"not impl 3");
    }
}

void compile_fn(Emitter& em, ASTFn* fn) {
    // TODO check if already compilerd
    
    void* addr = em.s;
    em.addresses.insert(std::make_pair<>((ASTNode*)fn, addr));

    auto it = em.waiting.find(fn);
    if (it != em.waiting.end()) {
        auto v = it->second;
        for (void** pos : v) {
            *pos = addr;
        }
        em.waiting.erase(fn);
    }

    u32 frame_size = 0;

    // init the stack frame
    for (const auto& decl : fn->block->ctx->defines) {
        if (decl.second->nodetype == AST_VAR) {
            ASTVar* var = (ASTVar*)decl.second;

            // TODO not all arguments can fit in registers
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

    for (ASTNode* stmt : fn->block->statements) {
        if (stmt->nodetype == AST_RETURN) {
            ASTReturn* ret = (ASTReturn*)stmt;
            compile_expr(em, lreg(RETREG), ret->value);
            *em.s = OP_RET;
            em.s += 8;
        }
        else {
            assert(!"not impl 4");
        }
    }

}

void Emitter::nodeaddr(ASTNode* node, void** addr) {
    auto it = addresses.find(node);
    if (it != addresses.end()) {
        *addr = it->second;
        return;
    }
    else {
        waiting[node].push_back(addr);
    }
}

void* bytecode_compile(Context& global, void* code_start, void* stack_start) {
    Emitter em(global, code_start, stack_start);

    for (const auto& decl : global.defines) {
        if (decl.second->nodetype == AST_VAR) {
            // TODO global variable
        }
        else if (decl.second->nodetype == AST_FN) {
            compile_fn(em, (ASTFn*)decl.second);
        }
    }
    
    return em.s;
}

Emitter::Emitter(Context& global, void* code_start, void* stack_start) 
    : global(global), s((u8*)code_start), stack((u64*)stack_start) {}


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

void bytecode_disassemble(u8* start, u8* end) {
Next:
    while (start < end) {

        switch (start[0]) {
            case OP_RET:   
                printf("RET\n");   
                start += 8;
                break;
            case OP_EXIT:  
                printf("EXIT 0x%x", start[1]);  
                break;
            case OP_JMP:   printf("JMP");   break;
            case OP_JMPIF: printf("JMPIF"); break;
            case OP_CALL:  printf("CALL");  break;
            case OP_ADD:   printf("ADD");   break;
            case OP_SUB:   printf("SUB");   break;
            case OP_MOV:   printf("MOV");   break;
            case OP_MULS:  printf("MULS");  break;
            case OP_MULU:  printf("MULU");  break;
            case OP_EQ:    printf("EQ");    break;
            case OP_GTS:   printf("GTS");   break;
            case OP_LTS:   printf("LTS");   break;
            case OP_GTU:   printf("GTU");   break;
            case OP_LTU:   printf("LTU");   break;
            case OP_GTF:   printf("GTF");   break;
            case OP_LTF:   printf("LTF");   break;
            case OP_GTES:  printf("GTES");  break;
            case OP_LTES:  printf("LTES");  break;
            case OP_GTEU:  printf("GTEU");  break;
            case OP_LTEU:  printf("LTEU");  break;
            case OP_GTEF:  printf("GTEF");  break;
            case OP_LTEF:  printf("LTEF");  break;
            default:
                assert(!"???");
                return;
        }

        if (start[0] >= OP_ADDRESSABLE) {
            AddrMode addrmode = (AddrMode)start[1];
            switch (addrmode) {
                case AM_REG2REG: {
                    Instr* i = (Instr*)start;
                    printf(" R%d, R%d\n", i->dstreg, i->srcreg);
                    start += sizeof(*i);
                    break;
                }
                case AM_REG2MEM: {
                    Reg2MemInstr* i = (Reg2MemInstr*)start;
                    printf(" [%p], R%d\n", i->dstaddr, i->srcreg);
                    start += sizeof(*i);
                    break;
                }
                case AM_MEM2REG: {
                    Mem2RegInstr* i = (Mem2RegInstr*)start;
                    printf(" R%d, [%p]\n", i->dstreg, i->srcaddr);
                    start += sizeof(*i);
                    break;
                }
                case AM_VAL2REG: {
                    Val2RegInstr* i = (Val2RegInstr*)start;
                    printf(" R%d, %lu\n", i->dstreg, i->val);
                    start += sizeof(*i);
                    break;
                }
                case AM_VAL2MEM: {
                    Val2MemInstr* i = (Val2MemInstr*)start;
                    printf(" [%p], %lu\n", i->dstaddr, i->val);
                    start += sizeof(*i);
                    break;
                }
                default: {
                    assert(!"???");
                }
            }
        }

    }
}

Loc lreg(u8 reg)         { return { .type = Loc::REGISTER, .reg   = reg  }; }
Loc lnode(ASTNode* node) { return { .type = Loc::NODE,     .node  = node }; }
Loc lval(u64 num)        { return { .type = Loc::VALUE,    .value = num  }; }
