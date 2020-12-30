#include "bytecode.h"

int interpret(u64* reg, void* main, void* stack) {
    // TODO remove this
    // This tells us how many function calls deep in we are
    // We use it so we know when we've exited main() with a RET
    int depth = 0;

    // @POINTERSIZE - We're assuming REGSIZE = POINTERSIZE = 64
    reg[SP] = (u64)stack;
    reg[IP] = (u64)main;

    while (true) {
NEXT:
        Instr* i = (Instr*)reg[IP];

        // printf("%-5s %p\n", instruction_names[i->op], (void*)reg[IP]);

        switch (i->op) {
            case OPC_EXIT: { 
                if (((u8*)i)[1])
                    printf("Bytecode exited with code %x\n", ((u8*)i)[1]);
                return 1;
            }
            case OPC_JZ:
            case OPC_JNZ: {
                if (!reg[i->srcreg] != (i-> op == OPC_JNZ)) {
                    reg[IP] = (u64)((MemInstr*)i)->addr;
                    goto NEXT;
                }
                reg[IP] += sizeof(MemInstr);
                goto NEXT;
            }
            case OPC_JMP: {
                reg[IP] = (u64)((MemInstr*)i)->addr;
                goto NEXT;
            }
            /*
            case OPC_CALL: {
                *(u64*)(mem + reg[SP]) = reg[IP] + 16;
                // todo native functions
                reg[IP] = ((u64*)b)[1];
                reg[SP] += 8;
                goto NEXT;
            }
            */
            case OPC_RET: {
                if (depth-- == 0) {
                    printf("Exited main with %lu\n", reg[RRET]);
                    return 0;
                }
                reg[SP] -= 8;
                reg[IP] = *(u64*)reg[SP];
                goto NEXT;
            }
            default: {
                if (!(i -> op && OPC_ADDRESSABLE))
                    assert(!"instruction not implemented");
            }
        }

        if (i->op & OPC_ADDRESSABLE) {
            u64 src, *dstp;

            switch (i->addrmode & AM_SRC_BITS) {
                case AM_SRCREG:
                    src = reg[i->srcreg];
                    break;
                case AM_SRCMEM:
                    src = *(u64*)((MemInstr*)i)->addr;
                    break;
                case AM_SRCDEREF:
                    src = *(u64*)(reg[i->srcreg] + i->mem_offset);
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
                    dstp = (u64*)(reg[i->dstreg] + i->mem_offset);
                    break;
            }

            switch (i->op) {
                case OPC_ADD: *dstp += src; break; 
                case OPC_SUB: *dstp -= src; break; 
                case OPC_MOV: *dstp  = src; break; 
                case OPC_MULS: *dstp  *= src; break; 
                case OPC_MULU: *(int64_t*)dstp  *= *(int64_t*)&src; break; 
                case OPC_EQ:   reg[RRES] =           *dstp ==             src; break;
                case OPC_GTU:  reg[RRES] =           *dstp >              src; break;
                case OPC_LTU:  
                               //printf("%lu < %lu\n", *dstp, src);
                               reg[RRES] =           *dstp <              src; break;
                case OPC_GTS:  reg[RRES] = *(int64_t*)dstp >  *(int64_t*)&src; break;
                case OPC_LTS:  reg[RRES] = *(int64_t*)dstp <  *(int64_t*)&src; break;
                case OPC_GTF:  reg[RRES] =  *(double*)dstp >   *(double*)&src; break;
                case OPC_LTF:  reg[RRES] =  *(double*)dstp <   *(double*)&src; break;
                case OPC_GTEU: reg[RRES] =           *dstp >=             src; break;
                case OPC_LTEU: reg[RRES] =           *dstp <=             src; break;
                case OPC_GTES: reg[RRES] = *(int64_t*)dstp >= *(int64_t*)&src; break;
                case OPC_LTES: reg[RRES] = *(int64_t*)dstp <= *(int64_t*)&src; break;
                case OPC_GTEF: reg[RRES] =  *(double*)dstp >=  *(double*)&src; break;
                case OPC_LTEF: reg[RRES] =  *(double*)dstp <=  *(double*)&src; break;

                default:
                    assert(!"Instruction not implemented");
            }

            reg[IP] += instr_size(i->addrmode);
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
