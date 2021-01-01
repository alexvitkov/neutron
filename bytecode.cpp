#include "bytecode.h"

const char* opcode_names[] = {
    "NONE",
    "MOV",
    "RET",
    "ADD",
    "JMP",
    "JZ",
    "EQ",
    "CALL",
    "SUB",
};


Loc& location(ASTVar* var) {
    return *(Loc*)(&var->location);
}

void disassemble(Instr* instr) {
    printf("    %-5s", opcode_names[instr->opcode]);

    switch (instr->opcode) {
        case OPC_RET:
            printf("\n");
            return;
    }
    switch(instr->dst.place) {
        case LOC_STACK:
            printf("S[%ld], ", instr->dst.stack_offset);
            break;
        case LOC_NEXTSTACK:
            printf("S[FS+%ld], ", instr->dst.stack_offset);
            break;
        case LOC_LABEL:
            printf("Label%d, ", instr->dst.label);
            break;
        default:
            assert(!"Not supported");
    }
    switch(instr->src.place) {
        case LOC_STACK:
            printf("S[%ld]\n", instr->src.stack_offset);
            break;
        case LOC_VALUE:
            printf("%lu\n", instr->src.value);
            break;
        default:
            assert(!"Not supported");
    }
}


// Right now every variable takes 8 bytes on the stack
// we use this to calculate where the fucntion arguments are
// arg[0] is at stackframe[0], arg[N] is at stackframe[N * 8]
void make_stack_frame(CompileContext& c, ASTBlock* block) {
    for (auto& def : block->ctx.declarations_arr) {
        if (def.node->nodetype == AST_VAR) {
            ASTVar* var = (ASTVar*)def.node;
            // TODO this shouldnt be stored inside the node itself
            location(var) = c.alloc_offset();
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
        }
    }
}

Loc lvalue(CompileContext& c, ASTNode* expr) {
    switch (expr->nodetype) {
        case AST_VAR:
            return location((ASTVar*)expr);
        case AST_FN:
            return { .place = LOC_LABEL, .label = c.get_fn_data(expr).label };
        default:
            return {};
    }
}

Loc compile_expr(CompileContext& c, ASTNode* expr, OpCode opc, Loc dst) {
    switch (expr->nodetype) {
        case AST_BLOCK: {
            ASTBlock* block = (ASTBlock*)expr;

            // Initialize the variables defined in the block
            for (auto& def : block->ctx.declarations_arr) {
                if (def.node->nodetype == AST_VAR) {
                    ASTVar* var = (ASTVar*)def.node;
                    if (var->initial_value)
                        compile_expr(c, var->initial_value, OPC_MOV, location(var));
                }
            }

            for (ASTNode*& stmt : block->statements) {
                compile_expr(c, stmt, OPC_NONE, {});
            }
            break;
        }
        case AST_NUMBER: {
            ASTNumber* num = (ASTNumber*)expr;
            Loc loc = { .place = LOC_VALUE, .value = num->floorabs };
            if (opc)
                c.emit({ .opcode = opc, .dst = dst, .src = loc, });
            return loc;
        }
        case AST_VAR: {
            ASTVar* var = (ASTVar*)expr;
            Loc loc =  location(var);
            if (opc)
                c.emit({ .opcode = opc, .dst = dst, .src = loc, });
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
                    Loc tmp = c.alloc_offset();
                    compile_expr(c, bin->lhs, OPC_MOV, dst);
                    compile_expr(c, bin->rhs, OPC_MOV, tmp);
                    c.emit({ .opcode = OPC_EQ, .dst = dst, .src = tmp });
                    c.free_offset(tmp);
                    break;
                }
                case '=': {
                    Loc tmp = c.alloc_offset();
                    Loc lhs_pos = lvalue(c, bin->lhs);
                    compile_expr(c, bin->rhs, OPC_MOV, tmp);
                    c.emit({ .opcode = OPC_MOV, .dst = lhs_pos, .src = tmp });
                    c.free_offset(tmp);
                    break;
                }
                default:
                    assert(!"Not implemented");
            }
            break;
        }
        case AST_IF: {
            ASTIf* ifs = (ASTIf*)expr;

            Loc expr_loc = c.alloc_offset();
            compile_expr(c, ifs->condition, OPC_MOV, expr_loc);

            u32 after_expr_label = c.new_label();

            c.emit({
                .opcode = OPC_JZ,
                .dst = { .place = LOC_LABEL, .label = after_expr_label },
                .src = expr_loc,
            });
            
            compile_expr(c, &ifs->block, {}, {});

            c.put_label(after_expr_label);
            break;
        }
        case AST_RETURN: {
            ASTReturn* ret = (ASTReturn*)expr;
            if (ret->value)
                compile_expr(c, ret->value, OPC_MOV, { .place = LOC_STACK, .stack_offset = -8 });
            c.emit({ .opcode = OPC_RET });
            break;
        }
        case AST_FN_CALL: {
            ASTFnCall* fncall = (ASTFnCall*)expr;
            Loc fnloc = lvalue(c, fncall->fn);

            // Pass the arguments
            for (int i = 0; i < fncall->args.size; i++) {
                compile_expr(c, fncall->args[i], OPC_MOV, { .place = LOC_NEXTSTACK, .stack_offset = i * 8 });
            }
            c.emit({
                .opcode = OPC_CALL,
                .dst = fnloc
            });
            if (dst.place) {
                c.emit({
                    .opcode = opc,
                    .dst = dst,
                    .src = { .place = LOC_NEXTSTACK, .stack_offset = -8 }
                });
            }
            break;
        }
        default:
            assert(!"Not implemented");
    }
    return {};
}

void compile_fn(CompileContext& c, ASTFn* fn) {
    // Put down a unique label bound to that function
    // so callees know where it is
    c.frame_depth = 0;

    CompileContext::FnData& data = c.get_fn_data(fn);

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
        Instr& in = c.instr[i];
        if (in.dst.place == LOC_NEXTSTACK) {
            in.dst.place = LOC_STACK; 
            // We also reserve a 8 bytes buffer for the return value
            // as it's wriiten at offset -8
            in.dst.stack_offset += frame_size + 8;
        }
        if (in.src.place == LOC_NEXTSTACK) {
            in.src.place = LOC_STACK; 
            in.src.stack_offset += frame_size + 8;
        }
        if (in.opcode == OPC_CALL) {
            in.src.place = LOC_STACK; // this line is only needed so the printing works
            in.src.stack_offset = frame_size;
        }
    }

}

u64 interpret(CompileContext& c, u32 i) {
    u8 stack[2048];
    u32 bp = 8;

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
        Instr& in = c.instr[i];
        // disassemble(&in);

        switch(in.opcode) {
            case OPC_RET: {
                if (msp == 0)
                    return stack[bp - 8];
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

                bp += in.src.stack_offset + 8;
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
            default:
                assert(!"Not implemented");
        }
        i++;
    }
    assert(!"Went outside of instruction range");
}

void compile_all(CompileContext& c) {
    for (auto& def : c.ctx.declarations_arr) {
        if (def.node->nodetype == AST_FN) {
            compile_fn(c, (ASTFn*)def.node);
        }
    }
}
