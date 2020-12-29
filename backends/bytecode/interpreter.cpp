#include "bytecode.h"

int interpret(u64* reg, void* main, void* code_end) {
    while (true) {
NEXT:
        Instr* i = (Instr*)main;

        switch (i->op) {
            case OP_EXIT: { 
                if (((u8*)i)[1])
                    printf("Bytecode exited with code %x\n", ((u8*)i)[1]);
                return 1;
            }
            case OP_JZ:
            case OP_JNZ: {
                if (!reg[i->srcreg] != (i-> op == OP_JNZ)) {
                    reg[IP] = (u64)((MemInstr*)i)->addr;
                    goto NEXT;
                }
                reg[IP] += sizeof(MemInstr);
                goto NEXT;
            }
            case OP_JMP: {
                reg[IP] = (u64)((MemInstr*)i)->addr;
                goto NEXT;
            }
            /*
            case OP_CALL: {
                *(u64*)(mem + reg[SP]) = reg[IP] + 16;
                // todo native functions
                reg[IP] = ((u64*)b)[1];
                reg[SP] += 8;
                goto NEXT;
            }
            case OP_RET: {
                reg[SP] -= 8;
                reg[IP] = *(u64*)(mem + reg[SP]);
                goto NEXT;
            }
            */
        }

        if (i->op & OP_ADDRESSABLE) {
            u64 src, *dstp;

            switch (i->addrmode) {
                case AM_REG2REG: {
                    dstp = &reg[i->dstreg];
                    src  =  reg[i->srcreg];
                    reg[IP] += sizeof(Instr);
                    break;
                }
                case AM_REG2MEM: {
                    MemInstr* ii = (MemInstr*)i;
                    dstp = (u64*)((u8*)ii->addr + ii->mem_offset);
                    src = reg[i->srcreg];
                    reg[IP] += sizeof(MemInstr);
                    break;
                }
                case AM_MEM2REG: {
                    MemInstr* ii = (MemInstr*)i;
                    dstp = &reg[i->dstreg];
                    src = *(u64*)((u8*)ii->addr + ii->mem_offset);
                    reg[IP] += sizeof(MemInstr);
                    break;
                }
                case AM_VAL2REG: {
                    ValInstr* ii = (ValInstr*)i;
                    dstp = reg + i->dstreg;
                    src = ii->val;
                    reg[IP] += sizeof(ValInstr);
                    break;
                }
                case AM_VAL2MEM: {
                    Val2MemInstr* ii = (Val2MemInstr*)i;
                    dstp = (u64*)ii->addr;
                    src = ii->val;
                    reg[IP] += sizeof(Val2MemInstr);
                    break;
                }
                default: {
                    assert(!"invalid addr mode");
                    return 1;
                }
            }

            switch (i->op) {
                case OP_ADD: *dstp += src; goto NEXT; 
                case OP_SUB: *dstp -= src; goto NEXT; 
                case OP_MOV: *dstp  = src; goto NEXT; 
                case OP_MULS: *dstp  *= src; goto NEXT; 
                case OP_MULU: *(int64_t*)dstp  *= *(int64_t*)&src; goto NEXT; 
                case OP_EQ:   reg[RRES] =           *dstp ==             src; goto NEXT;
                case OP_GTU:  reg[RRES] =           *dstp >              src; goto NEXT;
                case OP_LTU:  reg[RRES] =           *dstp <              src; goto NEXT;
                case OP_GTS:  reg[RRES] = *(int64_t*)dstp >  *(int64_t*)&src; goto NEXT;
                case OP_LTS:  reg[RRES] = *(int64_t*)dstp <  *(int64_t*)&src; goto NEXT;
                case OP_GTF:  reg[RRES] =  *(double*)dstp >   *(double*)&src; goto NEXT;
                case OP_LTF:  reg[RRES] =  *(double*)dstp <   *(double*)&src; goto NEXT;
                case OP_GTEU: reg[RRES] =           *dstp >=             src; goto NEXT;
                case OP_LTEU: reg[RRES] =           *dstp <=             src; goto NEXT;
                case OP_GTES: reg[RRES] = *(int64_t*)dstp >= *(int64_t*)&src; goto NEXT;
                case OP_LTES: reg[RRES] = *(int64_t*)dstp <= *(int64_t*)&src; goto NEXT;
                case OP_GTEF: reg[RRES] =  *(double*)dstp >=  *(double*)&src; goto NEXT;
                case OP_LTEF: reg[RRES] =  *(double*)dstp <=  *(double*)&src; goto NEXT;

                default: {
                    printf("Invalid instruction %x\n", i->op);
                    return 1;
                }
            }
        }
    }
}

void printreg(u64* reg) {
    for (int i = 0; i < NREG; i++) {
        printf("R%d: %s%016lX  ", i, i < 10? " " : "" ,reg[i]);
        if (i % 4 == 3)
            printf("\n");
    }
}
