#include "bytecode.h"

int interpret(u64* reg, u8* mem) {
    while (true) {
NEXT:
        u8* b = mem + reg[IP];
        const u8 op = b[0];

        switch (op) {
            case OP_EXIT: { 
                if (b[1])
                    printf("Bytecode exited with code %x\n", b[1]);
                return 1;
            }
            case OP_JMPIF: {
                if (!reg[b[3]]) {
                    reg[IP] += 8;
                    goto NEXT;
                }
                // FALLTHROUGH
            }
            case OP_JMP: {
                reg[IP] += ((int32_t*)b)[1];
                goto NEXT;
            }
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
        }

        if (op & OP_ADDRESSABLE) {
            u64 src, *dstp;

            switch (b[1]) {
                case AM_REG2REG: {
                    dstp = reg + b[2];
                    src = reg[b[3]];
                    reg[IP] += 8;
                    break;
                }
                case AM_REG2MEM: {
                    dstp = (u64*)(mem + ((u32*)b)[1]);
                    src = reg[b[3]];
                    reg[IP] += 8;
                    break;
                }
                case AM_MEM2REG: {
                    dstp = reg + b[3];
                    src = ((u64*)(mem + ((u32*)b)[1]))[0];
                    reg[IP] += 8;
                    break;
                }
                case AM_VAL2REG: {
                    dstp = reg + b[3];
                    src = ((u64*)b)[1];
                    reg[IP] += 16;
                    break;
                }
                case AM_VAL2MEM: {
                    dstp = (u64*)(mem + ((u32*)b)[1]);
                    src = ((u64*)b)[1];
                    reg[IP] += 16;
                    break;
                }
                default: {
                    printf("Invalid addressing mode %x\n", b[1]);
                    return 1;
                }
            }

            switch (op) {
                case OP_ADD: *dstp += src; goto NEXT; 
                case OP_SUB: *dstp -= src; goto NEXT; 
                case OP_MOV: *dstp  = src; goto NEXT; 
                case OP_MULS: *dstp  *= src; goto NEXT; 
                case OP_MULU: *(int64_t*)dstp  *= *(int64_t*)&src; goto NEXT; 
                case OP_EQ:   reg[RES] =           *dstp ==             src; goto NEXT;
                case OP_GTU:  reg[RES] =           *dstp >              src; goto NEXT;
                case OP_LTU:  reg[RES] =           *dstp <              src; goto NEXT;
                case OP_GTS:  reg[RES] = *(int64_t*)dstp >  *(int64_t*)&src; goto NEXT;
                case OP_LTS:  reg[RES] = *(int64_t*)dstp <  *(int64_t*)&src; goto NEXT;
                case OP_GTF:  reg[RES] =  *(double*)dstp >   *(double*)&src; goto NEXT;
                case OP_LTF:  reg[RES] =  *(double*)dstp <   *(double*)&src; goto NEXT;
                case OP_GTEU: reg[RES] =           *dstp >=             src; goto NEXT;
                case OP_LTEU: reg[RES] =           *dstp <=             src; goto NEXT;
                case OP_GTES: reg[RES] = *(int64_t*)dstp >= *(int64_t*)&src; goto NEXT;
                case OP_LTES: reg[RES] = *(int64_t*)dstp <= *(int64_t*)&src; goto NEXT;
                case OP_GTEF: reg[RES] =  *(double*)dstp >=  *(double*)&src; goto NEXT;
                case OP_LTEF: reg[RES] =  *(double*)dstp <=  *(double*)&src; goto NEXT;

                default: {
                    printf("Invalid instruction %x\n", op);
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
