#include "tir.h"
#include <iostream>

void TIR_Function::free_temp(TIR_Value* val) { }

void TIR_Function::emit(TIR_Instruction inst) {
    writepoint->instructions.push(inst);
}

TIR_Value* TIR_Function::alloc_temp(AST_Type* type) {
    return &writepoint->values.push({
        .valuespace = TVS_TEMP,
        .offset = temp_offset++,
        .type = type,
    });
}

TIR_Value* TIR_Function::alloc_val(TIR_Value val) {
    return &writepoint->values.push(val);
}

std::ostream& operator<< (std::ostream& o, TIR_Value& val) {
    o << dim;
    print(o, val.type, false);
    o << resetstyle << ' ';

    switch (val.valuespace) {
        case TVS_DISCARD:
            o << '_';
            break;
        case TVS_ARGUMENT:
            o << "arg" << val.offset;
            break;
        case TVS_VALUE:
            o << val.offset;
            break;
        case TVS_RET_VALUE:
            o << "retval";
            break;
        case TVS_TEMP:
            o << "$" << val.offset;
            break;
    }
    return o;
}

TIR_Block::TIR_Block() {
    static u64 next_id;
    id = next_id++;
}

std::ostream& operator<< (std::ostream& o, TIR_Instruction& instr) {
    o << "    ";
    switch (instr.opcode) {
        case TOPC_MOV:
            o << *instr.un.dst << " <- " << *instr.un.src << std::endl;
            break;
        case TOPC_LOAD:
            o << *instr.un.dst << " <- load " << *instr.un.src << std::endl;
            break;
        case TOPC_STORE:
            o << *instr.un.dst << " <- store " << *instr.un.src << std::endl;
            // o << "store " << *instr.un.src << " -> " << *instr.un.dst << std::endl;
            break;
        case TOPC_ADD:
            o << *instr.bin.dst << " <- " << *instr.bin.lhs << " + " << *instr.bin.rhs << std::endl;
            break;
        case TOPC_SUB:
            o << *instr.bin.dst << " <- " << *instr.bin.lhs << " - " << *instr.bin.rhs << std::endl;
            break;
        case TOPC_EQ:
            o << *instr.bin.dst << " <- " << *instr.bin.lhs << " == " << *instr.bin.rhs << std::endl;
            break;
        case TOPC_RET:
            o << "ret" << std::endl;
            break;

        case TOPC_CALL: {
            if (instr.call.dst)
                o << *instr.call.dst << " <- ";
            
            o << instr.call.fn->name << "(";

            for (TIR_Value* val : instr.call.args)
                o << *val << ", ";

            if (instr.call.args.size > 0)
                o << "\b\b";

            o << ")\n";
            break;
        }

        case TOPC_JMP: {
            o << "jmp " << "block" << instr.jmp.next_block->id << std::endl;
            break;
        }

        case TOPC_JMPIF: {
            o << "if " << *instr.jmpif.cond 
              << " jmp block" << instr.jmpif.then_block->id
              << " else block" << instr.jmpif.else_block->id << std::endl;
            break;
        }

        default:
            assert(!"Not implemented");
    }
    return o;
}

void TIR_Function::print() {
    printf("%s:\n", ast_fn->name);

    if (ast_fn->is_extern) {
        std::cout << "extern fn " << ast_fn->name << "...\n";
        return;
    }

    std::cout << "fn " << ast_fn->name << ":\n";
    arr<TIR_Block*> printed_blocks;
    arr<TIR_Block*> queue = { entry };

    while (queue.size > 0) {
        TIR_Block* block = queue.pop();
        printed_blocks.push(block);

        std::cout << "  block" << block->id << ":\n";

        for (auto& instr : block->instructions) {
            std::cout << instr;

            if (instr.opcode == TOPC_JMP) {
                if (!printed_blocks.contains(instr.jmp.next_block)
                        && !queue.contains(instr.jmp.next_block))
                    queue.push(instr.jmp.next_block);
            } 
            else if (instr.opcode == TOPC_JMPIF) {
                if (!printed_blocks.contains(instr.jmpif.else_block)
                        && !queue.contains(instr.jmpif.else_block))
                    queue.push(instr.jmpif.else_block);

                if (!printed_blocks.contains(instr.jmpif.then_block)
                        && !queue.contains(instr.jmpif.then_block))
                    queue.push(instr.jmpif.then_block);
            }
        }
    }
}

TIR_Value* compile_node(TIR_Function& fn, AST_Node* node, TIR_Value* dst);

void store(TIR_Function& fn, AST_Node* dst, AST_Node* src) {
    switch (dst->nodetype) {
        case AST_VAR: {
            TIR_Value* dstval = fn.valmap[dst];
            fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dstval, .src = dstval } });
            break;
        }
        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)dst;
            TIR_Value* ptrloc = compile_node(fn, deref->ptr, nullptr);
            TIR_Value* srcloc = compile_node(fn, src, nullptr);

            fn.emit({ .opcode = TOPC_STORE, .un = { .dst = ptrloc, .src = srcloc } });
            break;
        }
        default:
            assert(!"Not supported");
    }
}


TIR_Block* compile_block(TIR_Function& fn, AST_Block* ast_block, TIR_Block* after) {

    // TODO ALLOCATION
    TIR_Block* tir_block = new TIR_Block();

    fn.writepoint = tir_block;

    for (auto& kvp : ast_block->ctx.declarations_arr) {
        AST_Node* decl = kvp.node;

        // TODO this is a stupid workaround
        // The returntype should be stored in a saner way
        if (!strcmp(kvp.name, "returntype"))
            continue;

        switch (decl->nodetype) {
            case AST_VAR: {
                AST_Var* vardecl = (AST_Var*)decl;
                TIR_Value* val = fn.alloc_temp(vardecl->type);

                if (vardecl->initial_value)
                    compile_node(fn, vardecl->initial_value, val);

                fn.valmap.insert(vardecl, val);
                break;
            }
            default:
                assert(!"Not implemented");
        }
    }

    for (AST_Node* stmt : ast_block->statements) {
        compile_node(fn, stmt, nullptr);
    }

    if (after) {
        fn.emit({
            .opcode = TOPC_JMP,
            .jmp = { after }
        });
    }

    return tir_block;
}



TIR_Value* compile_node(TIR_Function& fn, AST_Node* node, TIR_Value* dst) {
    switch (node->nodetype) {
        case AST_VAR: {
            TIR_Value* val = fn.valmap[node];
            assert(val);

            if (dst && dst != val)
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = val } });
            else
                dst = val;

            return val;
        }

        case AST_NUMBER: {
            AST_Number* num = (AST_Number*)node;

            TIR_Value* num_value = fn.alloc_val({
                .valuespace = TVS_VALUE,
                .offset = num->floorabs,
                .type = num->type,
            });

            if (dst)
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = num_value } });
            else
                dst = num_value;

            return dst;
        }

        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;

            TIR_Value* lhs = compile_node(fn, bin->lhs, nullptr);
            TIR_Value* rhs = compile_node(fn, bin->rhs, nullptr);

            TIR_OpCode opcode;

            switch (bin->op) {
                case '+': opcode = TOPC_ADD; break;
                case '-': opcode = TOPC_SUB; break;
                case OP_DOUBLEEQUALS: opcode = TOPC_EQ; break;
                default:
                    assert(!"Not implemented");
            }

            if (!dst)
                dst = fn.alloc_temp(bin->type);

            fn.emit({
                .opcode = opcode,
                .bin = {dst, lhs, rhs}
            });

            return dst;
        }

        case AST_FN_CALL: {
            AST_FnCall* fncall = (AST_FnCall*)node;
            
            assert(fncall->fn->nodetype == AST_FN);
            AST_Fn* callee = (AST_Fn*)fncall->fn;

            arr<TIR_Value*> args;

            for (int i = 0; i < fncall->args.size; i++) {
                AST_Type* param_type = callee->args[i].type;
                TIR_Value*  arg_dst = fn.alloc_temp(param_type);

                TIR_Value* arg = compile_node(fn, fncall->args[i], arg_dst);
                args.push(arg);
            }

            fn.emit({ 
                .opcode = TOPC_CALL, 
                .call = { .dst = dst, .fn = (AST_Fn*)fncall->fn, .args = args.release(), }
            });
            break;
        }

        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            TIR_Value* retval = compile_node(fn, ret->value, &fn.retval);

            fn.emit({ .opcode = TOPC_RET, .none = {} });
            break;
        }

        case AST_BLOCK: {
            AST_Block* ast_block = (AST_Block*)node;

            // TODO ALLOCATION
            TIR_Block* continuation = new TIR_Block();

            compile_block(fn, ast_block, continuation);

            fn.writepoint = continuation;
            break;
        }

        case AST_ASSIGNMENT: {
            AST_BinaryOp* assign = (AST_BinaryOp*)node;
            store(fn, assign->lhs, assign->rhs);
            break;
        }

        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;

            TIR_Value* ptrloc = fn.alloc_temp(deref->ptr->type);
            compile_node(fn, deref->ptr, ptrloc);

            if (!dst)
                dst = fn.alloc_temp(deref->type);
            
            fn.emit({
                .opcode = TOPC_LOAD,
                .un = { .dst = dst, .src = ptrloc }
            });
            return dst;
        }

        case AST_IF: {
            TIR_Block* current_block = fn.writepoint;
            AST_If* ifs = (AST_If*)node;

            TIR_Value* cond = compile_node(fn, ifs->condition, nullptr);

            // TODO ALLOCATION
            TIR_Block* continuation = new TIR_Block();

            TIR_Block* then_block = compile_block(fn, &ifs->then_block, continuation);

            fn.writepoint = current_block;
            fn.emit({
                .opcode = TOPC_JMPIF, 
                .jmpif = {
                    .cond = cond,
                    .then_block = then_block,
                    .else_block = continuation,
                }
            });

            fn.writepoint = continuation;
            break;
        }

        default:
            assert(!"Not implemented");
    }

    return nullptr;
}

void TIR_Context::compile_all() {
    for(auto& decl : global.declarations_arr) {
        switch (decl.node->nodetype) {
            case AST_FN: {
                AST_Fn* fn = (AST_Fn*)decl.node;
                
                // TODO ALLOCATION
                TIR_Function* tirfn = new TIR_Function(*this, fn);

                AST_Type* rettype = (AST_Type*)fn->block.ctx.resolve("returntype");
                if (rettype) 
                    tirfn->retval.type = rettype;

                if (!fn->is_extern) {
                    TIR_Block* tirbl = compile_block(*tirfn, &fn->block, nullptr);
                    tirfn->entry = tirbl;
                }

                fns.insert(fn, tirfn);

                break;
            }
            default:
                assert(!"Not implemented");
        }
    }
}
