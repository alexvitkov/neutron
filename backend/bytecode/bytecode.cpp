#include "bytecode.h"

const char* opcode_names[] = {
    "NONE", "MOV", "RET", "ADD",
    "JMP", "JZ", "EQ", "CALL",
    "SUB", "LT",
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

struct BCLoc {
    LocPlace place;

    union {
        i32 stack_offset;
        u32 label;
        u64 value;
    };

    bool operator ==(const BCLoc& other) const {
        return place == other.place == value == other.value;
    }
    bool operator !=(const BCLoc& other) const {
        return !(*this == other);
    }
};

struct ASTBCTempRef : ASTValue {
    BCLoc loc;
    inline ASTBCTempRef(ASTType* type, BCLoc loc) : ASTValue(AST_TEMP_REF, type), loc(loc) {}
};

struct BCInstr {
    BC_OpCode opcode;
    BCLoc dst, src;
};

void disassemble(BCInstr* instr);


void BytecodeContext::emit(BCInstr i) {
    instr.push(i);
}

BCLoc BytecodeContext::alloc_offset(int n) {
    BCLoc offset = { .place = LOC_STACK, .stack_offset = frame_depth };
    frame_depth += n;
    return offset;
}

void BytecodeContext::free_offset(BCLoc loc) {}

u32 BytecodeContext::new_label() {
    labels.push(0);
    return labels.size - 1;
}

// FnData contains a label that we need to call the function 
// and its frame depth, so we know how much to increment the stack when calling another fn
BytecodeContext::FnData& BytecodeContext::get_fn_data(ASTNode* node) {
    u64 l;
    for (FnData& e : fn_data) {
        if (e.node == node)
            return e;
    }
    return fn_data.push({ node, new_label() });
}

void BytecodeContext::put_label(u32 label) {
    labels[label] = instr.size;
    // printf("Label%u:\n", label);
}

void bytecode_disassemble_all(BytecodeContext& ctx) {
    // TODO this is bad and won't work for long
    for (u32 i = 0; i < ctx.instr.size; i++) {
        for (u32 j = 0; j < ctx.labels.size; j++) {
            if (ctx.labels[j] == i)
                printf("Label%lu\n", j);
        }
        disassemble(&ctx.instr[i]);
    }
}

void disassemble(BCInstr* instr) {
    printf("    %-5s", opcode_names[instr->opcode]);

    switch (instr->opcode) {
        case OPC_RET:
            printf("\n");
            return;
    }
    switch(instr->dst.place) {
        case LOC_STACK:
            printf("s%d, ", instr->dst.stack_offset);
            break;
        case LOC_NEXTSTACK:
            printf("s[FS+%d], ", instr->dst.stack_offset);
            break;
        case LOC_LABEL:
            printf("Label%u, ", instr->dst.label);
            break;
        case LOC_VALUE:
            assert(!"LOC_VALUE is not valid as a target");
        default:
            assert(!"Invalid value");
    }
    switch(instr->src.place) {
        case LOC_STACK:
            printf("s%d\n", instr->src.stack_offset);
            break;
        case LOC_VALUE:
            printf("%lu\n", instr->src.value);
            break;
        default:
            assert(!"Not supported");
    }
}

int type_size(ASTType* t) {
    switch (t->nodetype) {
        case AST_PRIMITIVE_TYPE:
            return 1;
        case AST_STRUCT: {
            ASTStruct* s = (ASTStruct*)t;
            int size = 0;
            for(auto& m : s->members) {
                size += type_size(m.type);
            }
            return size;
        }
        default:
            assert(!"Not implemented");
    }
}

void make_stack_frame(BytecodeContext& c, ASTBlock* block) {
    for (auto& def : block->ctx.declarations_arr) {
        if (def.node->nodetype == AST_VAR) {
            ASTVar* var = (ASTVar*)def.node;
            ASTType* vt = var->type;
            c.locations[var] = c.alloc_offset(type_size(var->type));
        }
    }
    for (ASTNode*& stmt : block->statements) {
        switch (stmt->nodetype) {
            case AST_IF:
                make_stack_frame(c, &((ASTIf*)stmt)->block);
                break;
            case AST_WHILE:
                make_stack_frame(c, &((ASTWhile*)stmt)->block);
                break;
            case AST_BLOCK:
                make_stack_frame(c, (ASTBlock*)stmt);
                break;
            default:
                continue;
        }
    }
}

BCLoc lvalue(BytecodeContext& c, ASTNode* expr) {
    switch (expr->nodetype) {
        case AST_VAR:
            return c.locations[expr];
        case AST_FN:
            return { .place = LOC_LABEL, .label = c.get_fn_data(expr).label };
        case AST_TEMP_REF:
            return ((ASTBCTempRef*)expr)->loc;
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* ma = (ASTMemberAccess*)expr;

            // loc is the location of the beginning of the struct, aka the first member
            BCLoc loc = lvalue(c, ma->lhs);

            assert(loc.place == LOC_STACK);

            // We add to loc the index of the member variable
            // This works for now since all primitives take up 8 bytes on the stack
            // it will ahve to be changed later when we want to make guarentees
            // about the way structures are packed
            loc.stack_offset += ma->index;
            return loc;
        }
        default:
            assert(!"Not supported");
    }
}


BCLoc compile_expr(BytecodeContext& c, ASTNode* expr, BC_OpCode opc, BCLoc dst) {
    switch (expr->nodetype) {
        case AST_BLOCK: {
            ASTBlock* block = (ASTBlock*)expr;

            // Initialize the variables defined in the block
            for (auto& def : block->ctx.declarations_arr) {
                if (def.node->nodetype == AST_VAR) {
                    ASTVar* var = (ASTVar*)def.node;
                    if (var->initial_value)
                        compile_expr(c, var->initial_value, OPC_MOV, c.locations[(ASTNode*)var]);
                }
            }

            for (ASTNode*& stmt : block->statements) {
                compile_expr(c, stmt, OPC_NONE, {});
            }
            break;
        }
        case AST_NUMBER: {
            ASTNumber* num = (ASTNumber*)expr;
            BCLoc loc = { .place = LOC_VALUE, .value = num->floorabs };
            if (opc)
                c.emit({ .opcode = opc, .dst = dst, .src = loc, });
            return loc;
        }
        case AST_VAR:
        case AST_MEMBER_ACCESS:
        case AST_TEMP_REF:
        {
            assert(opc);
            BCLoc loc =  lvalue(c, expr);

            ASTValue* val = (ASTValue*)expr;
            
            switch (val->type->nodetype) {
                case AST_PRIMITIVE_TYPE:
                    c.emit({ .opcode = opc, .dst = dst, .src = loc, });
                    break;
                case AST_STRUCT: {
                    ASTStruct* s = (ASTStruct*)val->type;

                    for (int i = 0; i < s->members.size; i++) {
                        auto& entry = s->members[i];
                        ASTType* mt = entry.type;

                        ASTBCTempRef ref(mt, loc);

                        int mt_size = type_size(mt);
                        compile_expr(c, &ref, OPC_MOV, dst);
                        dst.stack_offset += mt_size;
                        loc.stack_offset += mt_size;
                    }
                    break;
                }
            }

            return loc;
        }
        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)expr;
            switch (bin->op) {
                case '+': {
                    compile_expr(c, bin->lhs, OPC_MOV, dst);
                    compile_expr(c, bin->rhs, OPC_ADD, dst);
                    break;
                }
                case '-': {
                    compile_expr(c, bin->lhs, OPC_MOV, dst);
                    compile_expr(c, bin->rhs, OPC_SUB, dst);
                    break;
                }
                case OP_DOUBLEEQUALS: {
                    BCLoc tmp = c.alloc_offset(1);
                    compile_expr(c, bin->lhs, OPC_MOV, dst);
                    compile_expr(c, bin->rhs, OPC_MOV, tmp);
                    c.emit({ .opcode = OPC_EQ, .dst = dst, .src = tmp });
                    c.free_offset(tmp);
                    break;
                }
                case '<': {
                    BCLoc tmp = c.alloc_offset(1);
                    compile_expr(c, bin->lhs, OPC_MOV, dst);
                    compile_expr(c, bin->rhs, OPC_MOV, tmp);
                    c.emit({ .opcode = OPC_LT, .dst = dst, .src = tmp });
                    c.free_offset(tmp);
                    break;
                }
                default: {
                    printf("Operator %d not implemented\n", bin->op);
                    assert(0);
                }
            }
            break;
        }
        case AST_ASSIGNMENT: {
            ASTBinaryOp* assign = (ASTBinaryOp*)expr;
            BCLoc tmp = c.alloc_offset(1);
            BCLoc lhs_pos = lvalue(c, assign->lhs);
            compile_expr(c, assign->rhs, OPC_MOV, tmp);
            c.emit({ .opcode = OPC_MOV, .dst = lhs_pos, .src = tmp });
            c.free_offset(tmp);
            break;
        }
        case AST_IF: {
            ASTIf* ifs = (ASTIf*)expr;
            BCLoc tmp = c.alloc_offset(1);
            u32 skip_label = c.new_label();

            compile_expr(c, ifs->condition, OPC_MOV, tmp);

            c.emit({
                .opcode = OPC_JZ,
                .dst = { .place = LOC_LABEL, .label = skip_label },
                .src = tmp,
            });

            c.free_offset(tmp);
            compile_expr(c, &ifs->block, {}, {});

            c.put_label(skip_label);
            break;
        }
        case AST_WHILE: {
            ASTWhile* whiles = (ASTWhile*)expr;
            u32 loop_start = c.new_label();
            u32 loop_end = c.new_label();
            BCLoc tmp = c.alloc_offset(1);

            c.put_label(loop_start);
            compile_expr(c, whiles->condition, OPC_MOV, tmp);

            c.emit({
                .opcode = OPC_JZ,
                .dst = { .place = LOC_LABEL, .label = loop_end },
                .src = tmp,
            });

            c.free_offset(tmp);
            compile_expr(c, &whiles->block, {}, {});

            c.emit({
                .opcode = OPC_JMP,
                .dst = { .place = LOC_LABEL, .label = loop_start },
                .src = tmp,
            });

            c.put_label(loop_end);
            break;
        }
        case AST_RETURN: {
            ASTReturn* ret = (ASTReturn*)expr;
            if (ret->value)
                compile_expr(c, ret->value, OPC_MOV, { .place = LOC_STACK, .stack_offset = -1 });
            c.emit({ .opcode = OPC_RET });
            break;
        }
        case AST_FN_CALL: {
            ASTFnCall* fncall = (ASTFnCall*)expr;
            BCLoc fnloc = lvalue(c, fncall->fn);

            // Pass the arguments
            for (int i = 0; i < fncall->args.size; i++) {
                BCLoc argloc = { .place = LOC_NEXTSTACK, .stack_offset = i };
                compile_expr(c, fncall->args[i], OPC_MOV, argloc);
            }
            c.emit({
                .opcode = OPC_CALL,
                .dst = fnloc
            });
            if (dst.place) {
                c.emit({
                    .opcode = opc,
                    .dst = dst,
                    .src = { .place = LOC_NEXTSTACK, .stack_offset = -1 }
                });
            }
            break;
        }
        default:
            assert(!"Not implemented");
    }
    return {};
}

void compile_fn(BytecodeContext& c, ASTFn* fn) {
    // Put down a unique label bound to that function
    // so callees know where it is
    c.frame_depth = 0;

    BytecodeContext::FnData& data = c.get_fn_data(fn);

    c.put_label(data.label);

    u32 start = c.instr.size;

    make_stack_frame(c, &fn->block);
    compile_expr(c, &fn->block, {}, {});
    c.emit({ .opcode = OPC_RET });

    c.fnaddr.insert(fn->name, start);

    u32 frame_size = c.frame_depth;

    // At this point we know what the framesize is, so we can
    // patch all the instrucitons inside the function that depend on it
    // TODO we should keep a list of them instead of iterating over the entire fn
    for (int i = start; i < c.instr.size; i++) {
        BCInstr& in = c.instr[i];
        if (in.dst.place == LOC_NEXTSTACK) {
            in.dst.place = LOC_STACK; 
            // We also reserve a 1 bytes buffer for the return value
            // as it's wriiten at offset -1
            in.dst.stack_offset += frame_size + 1;
        }
        if (in.src.place == LOC_NEXTSTACK) {
            in.src.place = LOC_STACK; 
            in.src.stack_offset += frame_size + 1;
        }
        if (in.opcode == OPC_CALL) {
            in.src.place = LOC_STACK; // this line is only needed so the printing works
            in.src.stack_offset = frame_size;
        }
    }

}

u64 bytecode_interpret(BytecodeContext& c, u32 i) {
    u64 stack[2048];
    u32 bp = 1;

    *(u64*)stack = 0;

    struct Stack2Entry {
        u32 return_addr;    
        u32 return_bp;
    };
    Stack2Entry stack2[200];

    u32 msp = 0;

    printf("\n\n\n");

Next:
    while (i < c.instr.size) {
        BCInstr& in = c.instr[i];
        // disassemble(&in);

        switch(in.opcode) {
            case OPC_RET: {
                if (msp == 0)
                    return stack[bp - 1];
                msp--;

                bp = stack2[msp].return_bp;
                i = stack2[msp].return_addr + 1;
                goto Next;
            }
            case OPC_CALL: {
                assert(in.dst.place == LOC_LABEL);
                assert(in.src.place == LOC_STACK);

                stack2[msp].return_bp = bp;
                stack2[msp].return_addr = i;

                bp += in.src.stack_offset + 1;
                i = c.labels[in.dst.label];
                msp++;
                goto Next;
            }
            case OPC_JMP: {
                assert(in.dst.place == LOC_LABEL);
                i = c.labels[in.dst.label];
                goto Next;
            }
            case OPC_JZ: {
                assert(in.dst.place == LOC_LABEL);
                assert(in.src.place == LOC_STACK);

                if (!stack[bp + in.src.stack_offset])
                    i = c.labels[in.dst.label];
                else
                    i ++;
                goto Next;
            }
        }

        u64 src;
        u64* dst;
        switch (in.src.place) {
            case LOC_VALUE:
                src = in.src.value;
                break;
            case LOC_STACK:
                src = *(u64*)&stack[bp + in.src.stack_offset];
                break;
            default:
                assert(!"Not implemented/supported");
        }
        switch (in.dst.place) {
            case LOC_STACK:
            case LOC_TEMP:
                dst = (u64*)&stack[bp + in.dst.stack_offset];
                break;
            default:
                assert(!"Not implemented/supported");
        }

        // printf("DST: %lu, SRC: %lu, BP: %d\n", *dst, src, bp);

        switch (in.opcode) {
            case OPC_MOV:
                *dst = src;
                break;
            case OPC_ADD:
                *dst += src;
                break;
            case OPC_SUB:
                *dst -= src;
                break;
            case OPC_EQ:
                *dst = (*dst == src);
                break;
            case OPC_LT:
                *dst = (*dst < src);
                break;
            default:
                assert(!"Not implemented");
        }
        i++;
    }
    assert(!"Went outside of instruction range");
}

void bytecode_commpile_all(BytecodeContext& c) {
    for (auto& def : c.ctx.declarations_arr) {
        if (def.node->nodetype == AST_FN) {
            compile_fn(c, (ASTFn*)def.node);
        }
    }
}
