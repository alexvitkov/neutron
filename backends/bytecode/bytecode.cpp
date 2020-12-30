#include "../../common.h"
#include "../../ast.h"
#include "../../typer.h"
#include "bytecode.h"

#include <algorithm>
#include <assert.h>

const char* instruction_names[256] = {
    "NONE",  "EXIT", "JMP",  "JNZ",  "JZ",   "CALL", "RET",  0,
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    0,       0,      0,      0,      0,      0,      0,      0,      
    // OPC_ADDRESSABLE = 0xF0
    "ADD",   "SUB",  "MOV",  "MULS", "MULU", "EQ",   "GTS",  "LTS", 
    "GTU",   "LTU",  "GTF",  "LTF",  "GTES", "LTES", "GTEU", "LTEU", 
    "GTEF",  "LTEF", 0,      0,      0,      0,      0,      0,
};

Loc lvalue(Emitter& em, ASTNode* expr) {
    switch (expr->nodetype) {
        case AST_VAR: {
            ASTVar* var = (ASTVar*)expr;
            return var->location;
        }
        default:
            assert(!"invalid lvalue");
    }
}

void compile_expr(Emitter& em, OpCode op, Loc dst, ASTNode *expr) {
    switch (expr->nodetype) {
        case AST_NUMBER: {
            ASTNumber* num = (ASTNumber*)expr;
            em.emit(op, dst, lval(num->floorabs));
            break;
        }

        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)expr;

            switch (bin->op) {
                case '+': {
                    compile_expr(em, OPC_MOV, lreg(RTMP), bin->lhs);
                    compile_expr(em, OPC_ADD, lreg(RTMP), bin->rhs);
                    if (op && !(op == OPC_MOV && dst.reg == RTMP && dst.type == Loc::REGISTER))
                        em.emit(op, dst, lreg(RTMP));
                    break;
                }
                case OP_DOUBLEEQUALS: {
                    compile_expr(em, OPC_MOV, lreg(RTMP), bin->lhs);
                    compile_expr(em, OPC_MOV, lreg(RTMP - 1), bin->rhs);
                    em.emit(OPC_EQ, lreg(RTMP), lreg(RTMP - 1));
                    break;
                }
                default:
                    if (prec[bin->op] & ASSIGNMENT) {
                        Loc loc = lvalue(em, bin->lhs);
                        OpCode op;
                        break;
                        switch (bin->op) {
                            case '=':                 op = OPC_MOV; break;
                            case OP_ADDASSIGN:        op = OPC_ADD; break;
                            case OP_SUBASSIGN:        op = OPC_SUB; break;
                            //case OP_MULASSIGN:        op = OPC_MUL; break;
                            //case OP_DIVASSIGN:        op = OPC_DIV; break;
                            //case OP_MODASSIGN:        op = OPC_MOD; break;
                            case OP_SHIFTLEFTASSIGN:  op = OPC_SHIFTLEFT;  break;
                            case OP_SHIFTRIGHTASSIGN: op = OPC_SHIFTRIGHT; break;
                            case OP_BITANDASSIGN:     op = OPC_BITAND;     break;
                            case OP_BITXORASSIGN:     op = OPC_BITXOR;     break;
                            case OP_BITORASSIGN:      op = OPC_BITOR;      break;
                        }
                        compile_expr(em, OPC_MOV, loc, bin->rhs);
                    }
                    assert(!"binary operator not implemented");
            }
            break;
        }

        case AST_IF: {
            ASTIf* ifs = (ASTIf*)expr;

            compile_expr(em, OPC_MOV, lreg(RTMP), ifs->condition);

            // Jump will be taken if the expression is false
            // we don't know the address we'll jump to yet, we'll fill it in when we're with the bloc
            MemInstr* jmp = (MemInstr*)em.s;
            jmp->srcreg = RRES;
            jmp->op = OPC_JZ;

            em.s += sizeof(*jmp);

            for (ASTNode* node : ifs->block.statements)
                compile_expr(em, OPC_NONE, lreg(0), node);

            // We're now right after the if's block, fill in the address
            jmp->addr = em.s;
            break;
        }

        case AST_RETURN: {
            ASTReturn* ret = (ASTReturn*)expr;
            if (ret->value)
                compile_expr(em, OPC_MOV, lreg(RRET), ret->value);
            *em.s = OPC_RET;
            em.s += sizeof(Instr);
            break;
        }

        case AST_CAST: {
            ASTCast* cast = (ASTCast*)expr;
            ASTType* oldType = gettype(em.global, cast->inner);
            compile_expr(em, op, dst, cast->inner);

            // TODO cast is ignored
            break;
        }

        case AST_VAR: {
            ASTVar* var = (ASTVar*)expr;
            em.emit(op, dst, var->location);
            break;
        }

        default:
            assert(!"compile_expr not impl for this ASTNode typek");
            break;
    }
}

u8 instr_size(u8 addrmode) {
    if (addrmode == (AM_SRCVAL | AM_DSTMEM))
        return sizeof(Val2MemInstr);
    else if (addrmode & AM_SRCVAL)
        return sizeof(ValInstr);
    else if (addrmode & (AM_SRCMEM | AM_DSTMEM))
        return sizeof(MemInstr);
    else
        return sizeof(Instr);
}

void Emitter::emit(OpCode op, Loc dst, Loc src) {
    Instr* i = (Instr*)s;
    i->op = op;

    switch (src.type) {
        case Loc::REGISTER: {
            i->addrmode = AM_SRCREG;
            i->srcreg = src.reg;

            switch (dst.type) {
                case Loc::REGISTER: {
                    i->dstreg = dst.reg;
                    i->addrmode |= AM_DSTREG;
                    break;
                }
                case Loc::NODE: {
                    MemInstr* i = (MemInstr*)s;
                    i->addrmode |= AM_DSTMEM;
                    nodeaddr(dst.node, &i->addr);
                    break;
                }
                case Loc::STACK: {
                    i->addrmode |= AM_DSTDEREF;
                    i->dstreg = SP;
                    i->mem_offset = dst.offset;
                    break;
                }
                default:
                    assert(!"addrmode not impl 1");
            }
            break;
        }
        case Loc::VALUE: {
            ValInstr* i = (ValInstr*)s;
            i->addrmode = AM_SRCVAL;
            i->val = src.value;

            switch (dst.type) {
                case Loc::REGISTER: {
                    i->addrmode |= AM_DSTREG;
                    i->dstreg = dst.reg;
                    break;
                }
                case Loc::STACK: {
                    i->addrmode |= AM_DSTDEREF;
                    i->dstreg = SP;
                    i->mem_offset = dst.offset;
                    break;
                }
                default:
                    assert(!"addrmode not impl 2");
            }
            break;
        }
        case Loc::STACK: {
            i->addrmode = AM_SRCDEREF;
            switch (dst.type) {
                case Loc::REGISTER: {
                    i->addrmode |= AM_DSTREG;
                    i->srcreg = SP;
                    i->dstreg = dst.reg;
                    i->mem_offset = src.offset;
                    break;
                }
                default:
                    assert(!"addrmode not impl 7");
            }
            break;
        }
        default:
            assert(!"addrmode not impl 3");
    }

    // Increment the instruction write pointer so we're ready for writing the next instruction
    s += instr_size(i->addrmode);
}

void compile_fn(Emitter& em, ASTFn* fn) {
    // TODO check if the function is already compiled
    
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

    // Initialize the stack frame
    for (const auto& decl : fn->block.ctx.defines) {
        if (decl.second->nodetype == AST_VAR) {
            ASTVar* var = (ASTVar*)decl.second;

            // TODO not all arguments can fit in registers
            if (var->argindex >= 0) {
                var->location = lreg(var->argindex); 
            }
            else {
                var->location = lstack(frame_size);
                frame_size += 8;
            }
            // printf("%s at %d:%d\n", var->name, var->location.type, var->location.offset);
        }
        else if (decl.second->nodetype == AST_FN) {
            compile_fn(em, (ASTFn*)decl.second);
        }
    }

    for (ASTNode* node : fn->block.statements)
        compile_expr(em, OPC_NONE, lreg(0), node);

    // Add a RET after the function body
    *em.s = OPC_RET;
    em.s += sizeof(Instr);

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

void* bytecode_compile(Context& global, void* code_start, void** main_addr) {
    Emitter em(global, code_start);

    for (const auto& decl : global.defines) {
        if (decl.second->nodetype == AST_VAR) {
            // TODO global variable
        }
        else if (decl.second->nodetype == AST_FN) {
            compile_fn(em, (ASTFn*)decl.second);
        }
    }

    *main_addr = 0;
    ASTFn* main_fn = (ASTFn*)global.resolve("main");
    if (main_fn && main_fn->nodetype == AST_FN) {
        *main_addr = em.addresses[main_fn];
    }
    return em.s;
}

Emitter::Emitter(Context& global, void* code_start) : global(global), s((u8*)code_start) {}

void Emitter::emitexit(u8 code) {
    s[0] = OPC_EXIT;
    s[1] = code;
    s += 8;
}

void Emitter::emitcall(u64 ptr) {
    s[0] = OPC_CALL; 
    ((u64*)s)[1] = ptr;
    s += 16;
}

void printregname(u8 reg) {
    if (reg == RTMP)
        printf("RTMP");
    else if (reg == RRET)
        printf("RRET");
    else if (reg == SP)
        printf("SP");
    else printf("R%d", reg);
}

void bytecode_disassemble(u8* start, u8* end) {

Next:
    while (start < end) {
        Instr* i = (Instr*)start;
        printf("%-5s", instruction_names[i->op]);

        switch (i->op) {
            case OPC_RET:   
                start += sizeof(*i);
                goto Next;
            case OPC_EXIT:  
                printf("0x%x\n", start[1]);  
                start += sizeof(*i);
                goto Next;
            case OPC_JMP:
                printf("\n");
                start += sizeof(*i);
                goto Next;
            case OPC_JNZ: 
                printf("%p\n", ((MemInstr*)start)->addr);
                start += sizeof(MemInstr);
                goto Next;
            case OPC_JZ: 
                printf("%p\n", ((MemInstr*)start)->addr);
                start += sizeof(MemInstr);
                goto Next;
            default:
                if (i->op & OPC_ADDRESSABLE)
                    break;
                printf("Invalid instruction %x\n", i->op);
                assert(0);
                return;
        }

        switch (i->addrmode & AM_DST_BITS) {
            case AM_DSTREG:
                printregname(i->dstreg);
                printf(", ");
                break;
            case AM_DSTMEM:
                printf("%p, ", (u64*)((MemInstr*)i)->addr);
                break;
            case AM_DSTDEREF:
                printf("[");
                printregname(i->dstreg);
                printf("], ");
                break;
        }

        switch (i->addrmode & AM_SRC_BITS) {
            case AM_SRCREG:
                printregname(i->srcreg);
                break;
            case AM_SRCMEM:
                printf("%p", (u64*)((MemInstr*)i)->addr);
                break;
            case AM_SRCDEREF:
                printf("[");
                printregname(i->srcreg);
                printf("]");
                break;
            case AM_SRCVAL:
                printf("%lu", ((ValInstr*)i)->val);
                break;
        }

        printf("\n");
        start += instr_size(i->addrmode);
    }
}

Loc lreg(u8 reg)         { return { .type = Loc::REGISTER, .reg   = reg  }; }
Loc lstack(u64 offset)   { return { .type = Loc::STACK,    .offset = offset }; }
Loc lnode(ASTNode* node) { return { .type = Loc::NODE,     .node  = node }; }
Loc lval(u64 num)        { return { .type = Loc::VALUE,    .value = num  }; }
