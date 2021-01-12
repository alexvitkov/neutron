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
    print(o, val.type, false);
    o << ' ';

    switch (val.valuespace) {
        case TVS_DISCARD:
            o << '_';
            break;
        case TVS_ARGUMENT:
            o << "arg" << val.offset;
            break;
        case TVS_NEXTARGUMENT:
            o << "callarg" << val.offset;
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

std::ostream& operator<< (std::ostream& o, TIR_Instruction& instr) {
    o << "    ";
    switch (instr.opcode) {
        case TOPC_MOV:
            o << *instr.un.dst << " <- " << *instr.un.src << std::endl;
            break;
        case TOPC_ADD:
            o << *instr.bin.dst << " <- " << *instr.bin.lhs << " + " << *instr.bin.rhs << std::endl;
            break;
        case TOPC_SUB:
            o << *instr.bin.dst << " <- " << *instr.bin.lhs << " - " << *instr.bin.rhs << std::endl;
            break;
        case TOPC_RET:
            o << "ret" << std::endl;
            break;
        default:
            assert(!"Not implemented");
    }
    return o;
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

        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            TIR_Value* retval = compile_node(fn, ret->value, &fn.retval);

            fn.emit({ TOPC_RET });
            break;
        }

        case AST_BLOCK: {
            AST_Block* block = (AST_Block*)node;

            TIR_Block* new_block = new TIR_Block();
            fn.writepoint = new_block;

            for (auto& kvp : block->ctx.declarations_arr) {
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

            for (AST_Node* stmt : block->statements) {
                compile_node(fn, stmt, nullptr);
            }

            // TODO this is a hack of course
            return (TIR_Value*)(void*)new_block;
            break;
        }

        case AST_ASSIGNMENT: {
            AST_BinaryOp* assign = (AST_BinaryOp*)node;
            TIR_Value* lhs = compile_node(fn, assign->lhs, nullptr);
            compile_node(fn, assign->rhs, lhs);
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
                
                TIR_Function* tirfn = new TIR_Function(*this);

                AST_Type* rettype = (AST_Type*)fn->block.ctx.resolve("returntype");
                if (rettype) 
                    tirfn->retval.type = rettype;

                TIR_Block* tirbl = (TIR_Block*)compile_node(*tirfn, &fn->block, nullptr);

                tirfn->entry = tirbl;

                fns.insert(fn, tirfn);
                break;
            }
            default:
                assert(!"Not implemented");
        }
    }
}
