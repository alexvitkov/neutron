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

            switch (i->addrmode & AM_SRC_BITS) {
                case AM_SRCREG:
                    src = reg[i->srcreg];
                    break;
                case AM_SRCMEM:
                    src = *(u64*)((MemInstr*)i)->addr;
                    break;
                case AM_SRCDEREF:
                    src = *(u64*)(reg[i->srcreg]);
                    break;
                case AM_SRCVAL:
                    src = ((ValInstr*)i)->val;
                    break;
            }
            switch (i->addrmode & AM_DST_BITS) {
                case AM_DSTREG:
                    dstp = &reg[i->dstreg];
                    break;
                case AM_DSTMEM:
                    dstp = (u64*)((MemInstr*)i)->addr;
                    break;
                case AM_DSTDEREF:
                    dstp = (u64*)(reg[i->dstreg]);
                    break;
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

                default:
                    assert(!"invalid instr");
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
