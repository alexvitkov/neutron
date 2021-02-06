#include "tir.h"
#include "../../typer.h"
#include <iostream>

TIR_Value::operator bool() {
    return valuespace;
}
bool operator== (TIR_Value& lhs, TIR_Value& rhs) {
    return lhs.valuespace == rhs.valuespace && lhs.offset == rhs.offset;
}
bool operator!= (TIR_Value& lhs, TIR_Value& rhs) {
    return !(lhs == rhs);
}
u32 map_hash(TIR_Value data)                  { return data.valuespace ^ data.offset; }
bool map_equals(TIR_Value lhs, TIR_Value rhs) { return lhs == rhs; }


void TIR_Function::emit(TIR_Instruction inst) {
    writepoint->instructions.push(inst);
}

TIR_Value TIR_Function::alloc_temp(AST_Type* type) {
    return {
        .valuespace = TVS_TEMP,
        .offset = temp_offset++,
        .type = type,
    };
}

TIR_Value TIR_Function::alloc_stack(AST_Var* var) {
    TIR_Value val {
        .valuespace = TVS_STACK,
        .offset = temp_offset++,
        .type = c.global.get_pointer_type(var->type),
    };
    stack.push({var, val});
    return val;
}

std::wostream& operator<< (std::wostream& o, TIR_Value val) {
    o << dim;
    print(o, val.type, false);
    o << resetstyle << ' ';

    switch (val.valuespace) {
        case TVS_DISCARD:
            o << '_';
            break;
        case TVS_ARGUMENT:
            if (val.flags & TVF_BYVAL)
                o << "byval ";
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
        case TVS_STACK:
            o << "stack" << val.offset;
            break;
        case TVS_GLOBAL:
            o << "@" << val.offset;
    }
    return o;
}

TIR_Block::TIR_Block() {
    static u64 next_id;
    id = next_id++;
}

std::wostream& operator<< (std::wostream& o, TIR_Instruction& instr) {
    o << "    ";
    switch (instr.opcode) {
        case TOPC_MOV:
            o << instr.un.dst << " <- " << instr.un.src << std::endl;
            break;
        case TOPC_BITCAST:
            o << instr.un.dst << " <- bitcast " << instr.un.src << std::endl;
            break;
        case TOPC_LOAD:
            o << instr.un.dst << " <-- load " << instr.un.src << std::endl;
            break;
        case TOPC_STORE:
            // o << *instr.un.dst << " <- store " << *instr.un.src << std::endl;
            o << "store " << instr.un.src << " -> " << instr.un.dst << std::endl;
            break;
        case TOPC_ADD:
            o << instr.bin.dst << " <- " << instr.bin.lhs << " + " << instr.bin.rhs << std::endl;
            break;
        case TOPC_SUB:
            o << instr.bin.dst << " <- " << instr.bin.lhs << " - " << instr.bin.rhs << std::endl;
            break;
        case TOPC_EQ:
            o << instr.bin.dst << " <- " << instr.bin.lhs << " == " << instr.bin.rhs << std::endl;
            break;
        case TOPC_LT:
            o << instr.bin.dst << " <- " << instr.bin.lhs << " < " << instr.bin.rhs << std::endl;
            break;
        case TOPC_GEP: {
            o << instr.gep.dst << " <- GEP " << instr.gep.base << "";
            for (TIR_Value v : instr.gep.offsets) {
                o << '[' << v << ']';
            }
            o << "\n";
            break;
        }
        case TOPC_RET:
            o << "ret" << std::endl;
            break;

        case TOPC_CALL: {
            if (instr.call.dst.valuespace != TVS_DISCARD)
                o << instr.call.dst << " <- ";
            
            o << instr.call.fn->name << "(";

            for (TIR_Value val : instr.call.args)
                o << val << ", ";

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
            o << "if " << instr.jmpif.cond 
              << " jmp block" << instr.jmpif.then_block->id
              << " else block" << instr.jmpif.else_block->id << std::endl;
            break;
        }

        default:
            NOT_IMPLEMENTED();
    }
    return o;
}

void TIR_Function::print() {
    if (ast_fn->is_extern) {
        wcout << "extern fn " << ast_fn->name << "...\n";
        return;
    }

    wcout << "fn " << ast_fn->name << "(";

    for (TIR_Value& param : parameters) {
        wcout << param << ", ";
    }
    if (parameters.size)
        wcout << "\b\b \b";
    wcout << ")\n";


    for (TIR_Block* block : blocks) {
        if (blocks.size > 1)
            wcout << "  block" << block->id << ":\n";
        for (auto& instr : block->instructions)
            wcout << instr;
    }
}

TIR_Value compile_node_rvalue(TIR_Function& fn, AST_Node* node, TIR_Value dst);

// *out will always be filled in.
// The function returns true if *out is a POINTER to the value we need,
// and false if *out is the value we need itself
//
// If true is returned, you need to use LOAD/STORE
// If false is returned, you can use the value directly
bool get_location(TIR_Function &fn, AST_Value *val, TIR_Value *out) {
    switch (val->nodetype) {
        case AST_VAR: {
            AST_Var* var = (AST_Var*)val;

            if (var->is_global || var->always_on_stack) {
                *out = var->is_global ? fn.c.valmap[var] : fn._valmap[var];
                return true;
            }
            else {
                TIR_Value varloc = fn._valmap[var];
                return false;
            }
        }

        case AST_DEREFERENCE: {
            AST_Dereference *deref = (AST_Dereference*)val;
            *out = compile_node_rvalue(fn, deref->ptr, {});
            return true;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess *member_access = (AST_MemberAccess*)val;

            TIR_Value base;
            assert (get_location(fn, member_access->lhs, &base));

            arr<TIR_Value> offsets = {
                {
                    .valuespace = TVS_VALUE,
                    .offset = 0,
                    .type = &t_u32,
                },
                {
                    .valuespace = TVS_VALUE,
                    .offset = member_access->index,
                    .type = &t_u32,
                }
            };

            *out = fn.alloc_temp(fn.c.global.get_pointer_type(member_access->type));
            fn.emit({ 
                .opcode = TOPC_GEP,
                .gep = {
                    .dst = *out,
                    .base = base,
                    .offsets = offsets.release(),
                }
            });
            return true;
        }
        default:
            UNREACHABLE;
    }
}


void compile_block(TIR_Function& fn, TIR_Block* tir_block, AST_Block* ast_block, TIR_Block* after) {

    fn.writepoint = tir_block;

    for (auto& kvp : ast_block->ctx.declarations) {
        AST_Node* decl = kvp.value;

        switch (decl->nodetype) {
            case AST_VAR: {
                AST_Var* vardecl = (AST_Var*)decl;
                TIR_Value val;

                if (vardecl->argindex >= 0) {
                    val = fn.parameters[vardecl->argindex];
                }
                else if (vardecl->always_on_stack) {
                    val = fn.alloc_stack(vardecl);
                } 
                else {
                    val = fn.alloc_temp(vardecl->type);
                }

                if (vardecl->initial_value)
                    compile_node_rvalue(fn, vardecl->initial_value, val);

                fn._valmap.insert(vardecl, val);
                break;
            }
            default:
                NOT_IMPLEMENTED();
        }
    }

    for (AST_Node* stmt : ast_block->statements) {
        compile_node_rvalue(fn, stmt, {});
    }

    if (after) {
        fn.emit({
            .opcode = TOPC_JMP,
            .jmp = { after }
        });
        after->previous_blocks.push_unique(tir_block);
    }
}

TIR_Value get_array_ptr(TIR_Function& fn, AST_Value* arr) {

    switch (arr->nodetype) {
        case AST_VAR: {
            AST_Var* var = (AST_Var*)arr;

            if (var->is_global) {
                return fn.c.valmap[var];
            } else {
                NOT_IMPLEMENTED();
            }

            break;
        }
        default:
            UNREACHABLE;
    }
}

TIR_Value compile_node_rvalue(TIR_Function& fn, AST_Node* node, TIR_Value dst) {
    switch (node->nodetype) {
        case AST_MEMBER_ACCESS:
        case AST_VAR: 
        case AST_DEREFERENCE: 
        {
            TIR_Value src;
            if (get_location(fn, (AST_Value*)node, &src)) {
                if (!dst)
                    dst = fn.alloc_temp(((AST_Value*)node)->type);

                fn.emit({
                    .opcode = TOPC_LOAD,
                    .un = { .dst = dst, .src = src }
                });
                return dst;
            }
            else {
                if (dst && dst != src) {
                    fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = src } });
                    return dst;
                } else {
                    return src;
                }
            }
        }

        case AST_NUMBER: {
            AST_Number* num = (AST_Number*)node;

            TIR_Value num_value = {
                .valuespace = TVS_VALUE,
                .offset = num->floorabs,
                .type = num->type,
            };

            if (dst)
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = num_value } });
            else
                dst = num_value;

            return dst;
        }

        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;

            TIR_Value lhs = compile_node_rvalue(fn, bin->lhs, {});
            TIR_Value rhs = compile_node_rvalue(fn, bin->rhs, {});

            TIR_OpCode opcode;

            switch (bin->op) {
                case TOK_ADD:  {
                    opcode = TOPC_ADD; 
                    break;
                }
                case TOK_SUBTRACT: {
                    opcode = TOPC_SUB; 
                    break;
                }
                case OP_DOUBLEEQUALS: {
                    opcode = TOPC_EQ; 
                    break;
                }
                case OP_LESSTHAN: {
                    opcode = TOPC_LT; 
                    break;
                }
                case OP_ADD_PTR_INT: {
                    opcode = TOPC_GEP;
                    break;
                }
                default:
                    NOT_IMPLEMENTED();
            }

            if (!dst)
                dst = fn.alloc_temp(bin->type);

            if (bin->op == OP_ADD_PTR_INT) {
                arr<TIR_Value> offsets = { rhs };
                fn.emit({
                    .opcode = TOPC_GEP,
                    .gep = {.dst = dst, .base = lhs, .offsets = offsets.release()}
                });
            } else {
                fn.emit({
                    .opcode = opcode,
                    .bin = {dst, lhs, rhs}
                });
            }

            return dst;
        }

        case AST_FN_CALL: {
            AST_FnCall* fncall = (AST_FnCall*)node;
            
            assert(fncall->fn IS AST_FN);
            AST_Fn *callee = (AST_Fn*)fncall->fn;
            // AST_FnType* callee_type = (AST_FnType*)fncall->fn->type;

            TIR_Function *tir_callee = fn.c.fns[callee];

            arr<TIR_Value> args;

            for (u32 i = 0; i < fncall->args.size; i++) {

                AST_Type* param_type = &t_any8;
                if (i < tir_callee->parameters.size)
                    param_type = tir_callee->parameters[i].type;

                TIR_Value  arg_dst = fn.alloc_temp(param_type);
                TIR_Value arg;

                if (fncall->args[i]->type IS AST_STRUCT) {
                    // arg = compile_node_rvalue(fn, fncall->args[i], arg_dst);
                    assert (get_location(fn, fncall->args[i], &arg));
                }
                else {
                    arg = compile_node_rvalue(fn, fncall->args[i], arg_dst);
                }
                args.push(arg);
            }

            if (!dst && fn.retval) {
                dst = fn.alloc_temp(fn.retval.type);
            }

            fn.emit({ 
                .opcode = TOPC_CALL, 
                .call = { .dst = dst, .fn = (AST_Fn*)fncall->fn, .args = args.release(), }
            });

            return dst;
        }

        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;

            if (ret->value)
                TIR_Value retval = compile_node_rvalue(fn, ret->value, fn.retval);

            fn.emit({ .opcode = TOPC_RET, .none = {} });
            return {};
        }

        case AST_BLOCK: {
            AST_Block* ast_block = (AST_Block*)node;

            // TODO ALLOCATION
            TIR_Block* inner_block = new TIR_Block();
            TIR_Block* continuation = new TIR_Block();

            compile_block(fn, inner_block, ast_block, continuation);

            fn.blocks.push(inner_block);
            fn.blocks.push(continuation);

            fn.writepoint = continuation;
            return {};
        }

        case AST_ASSIGNMENT: {
            AST_BinaryOp *assign = (AST_BinaryOp*)node;
            TIR_Value ptrloc;

            if (get_location(fn, assign->lhs, &ptrloc)) {
                TIR_Value tmp = compile_node_rvalue(fn, assign->rhs, {});

                fn.emit({ 
                    .opcode = TOPC_STORE, 
                    .un = {
                        .dst = ptrloc,
                        .src = tmp,
                    }
                });

                if (dst) {
                    fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = tmp } });
                    return dst;
                } else {
                    return tmp;
                }
            }
            else {
                TIR_Value lhsloc = compile_node_rvalue(fn, assign->lhs, {});
                TIR_Value valloc = compile_node_rvalue(fn, assign->rhs, lhsloc);

                if (dst) {
                    fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = valloc } });
                    return dst;
                } else {
                    return ptrloc;
                }
            }
        }


        case AST_ADDRESS_OF: {
            AST_AddressOf *addrof = (AST_AddressOf*)node;

            switch (addrof->inner->nodetype) {
                case AST_VAR: {
                    AST_Var* var = (AST_Var*)addrof->inner;
                    return var->is_global ? fn.c.valmap[var] : fn._valmap[var];
                }
                default:
                    UNREACHABLE;
            }
        }

        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            TIR_Block* original_block = fn.writepoint;

            TIR_Value cond = compile_node_rvalue(fn, ifs->condition, {});

            // TODO ALLOCATION
            TIR_Block* then_block = new TIR_Block();
            TIR_Block* continuation = new TIR_Block();

            compile_block(fn, then_block, &ifs->then_block, continuation);

            fn.writepoint = original_block;
            fn.emit({
                .opcode = TOPC_JMPIF, 
                .jmpif = {
                    .cond = cond,
                    .then_block = then_block,
                    .else_block = continuation,
                }
            });
            then_block->previous_blocks.push_unique(original_block);
            continuation->previous_blocks.push_unique(original_block);

            fn.blocks.push(then_block);
            fn.blocks.push(continuation);

            fn.writepoint = continuation;
            return {};
        }

        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;
            TIR_Block* original_block = fn.writepoint;

            // TODO ALLOCATION
            TIR_Block* cond_block = new TIR_Block();
            TIR_Block* loop_block = new TIR_Block();
            TIR_Block* continuation = new TIR_Block();

            // The current block leads into the condition block, unconditionally.
            fn.emit({
                .opcode = TOPC_JMP, 
                .jmp = { .next_block = cond_block, }
            });

            // Compile the condition into the condition block
            fn.writepoint = cond_block;
            TIR_Value cond = compile_node_rvalue(fn, whiles->condition, {});

            // If the condition is false, skip the loop body and jump to the continuation
            // If the condition is true, jump to the loop body
            fn.emit({
                .opcode = TOPC_JMPIF, 
                .jmpif = {
                    .cond = cond,
                    .then_block = loop_block,
                    .else_block = continuation,
                }
            });

            // The while loop block always jups back to the condition block
            compile_block(fn, loop_block, &whiles->block, cond_block);

            cond_block->previous_blocks.push_unique(original_block);
            cond_block->previous_blocks.push_unique(loop_block);

            continuation->previous_blocks.push_unique(cond_block);

            loop_block->previous_blocks.push_unique(cond_block);

            fn.blocks.push(cond_block);
            fn.blocks.push(loop_block);
            fn.blocks.push(continuation);

            fn.writepoint = continuation;
            return {};
        }

        case AST_STRING_LITERAL: {
            AST_StringLiteral* str = (AST_StringLiteral*)node;

            DeclarationKey key = { .string_literal = str };
            AST_Var* strvar = (AST_Var*)fn.c.global.resolve(key);

            TIR_Value val = fn.c.valmap[str];

            if (!dst)
                dst = fn.alloc_temp(str->type);

            fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = val } });

            return val;
        }

        case AST_CAST: {
            AST_Cast* cast = (AST_Cast*)node;

            if (cast->inner->type IS AST_ARRAY_TYPE && cast->type IS AST_POINTER_TYPE) {

                TIR_Value var_location = get_array_ptr(fn, cast->inner);

                TIR_Value offset_0 
                    = { .valuespace = TVS_VALUE, .offset = 0, .type = &t_u32 };

                arr<TIR_Value> offsets = { offset_0, offset_0 };

                if (!dst)
                    dst = fn.alloc_temp(cast->type);

                fn.emit({ 
                    .opcode =  TOPC_GEP, 
                    .gep = { 
                        .dst = dst, 
                        .base = var_location, 
                        .offsets = offsets.release(),
                    },
                });
                return dst;
            }

            else if ((cast->inner->type IS AST_POINTER_TYPE && cast->type IS AST_POINTER_TYPE)
                    || cast->type == &t_any8) {

                TIR_Value src = compile_node_rvalue(fn, cast->inner, {});

                if (!dst)
                    dst = fn.alloc_temp(cast->type);

                fn.emit({
                    .opcode = TOPC_BITCAST,
                    .un = {
                        .dst = dst,
                        .src = src,
                    }
                });
                
                return dst;
            }
            else {
                assert(!"Cast not supported");
            }
        }

        default:
            UNREACHABLE;
    }
}

void TIR_Function::compile_signature() {
    AST_FnType *fntype = (AST_FnType*)ast_fn->type;
    // TODO VOID
    if (fntype->returntype && fntype->returntype != &t_void) {
        retval.valuespace = TVS_RET_VALUE;
        retval.type = fntype->returntype;
    }
    else {
        retval.valuespace = TVS_DISCARD;
        retval.type = &t_void;
    }

    for (auto& kvp : this->ast_fn->block.ctx.declarations) {
        if (kvp.value IS AST_VAR) {
            AST_Var *vardecl = (AST_Var*)kvp.value;

            if (vardecl->argindex < 0)
                continue;

            AST_Type *arg_type = vardecl->type;
            TIR_Value_Flags flags = (TIR_Value_Flags)0;

            if (vardecl->type IS AST_STRUCT) {
                arg_type = c.global.get_pointer_type(vardecl->type);
                flags = TVF_BYVAL;
            }

            TIR_Value val = {
                .valuespace = TVS_ARGUMENT,
                .flags = flags,
                .offset = (u64)vardecl->argindex,
                .type = arg_type,
            };

            // TODO DS
            while (parameters.size <= vardecl->argindex)
                parameters.push({});
            parameters[vardecl->argindex] = val;
        }
    }
}

void TIR_Function::compile() {
    AST_Type* rettype = ((AST_FnType*)ast_fn->type)->returntype;
    if (rettype) 
        retval.type = rettype;

    if (!ast_fn->is_extern) {
        TIR_Block* entry = new TIR_Block();
        blocks.push(entry);
        compile_block(*this, entry, &ast_fn->block, nullptr);
    }
}

void TIR_Context::compile_all() {
    // Run through the global variables and assign them TIR_Values and initial values
    for(auto& decl : global.declarations) {
        switch (decl.value->nodetype) {
            case AST_VAR: {
                AST_Var* var = (AST_Var*)decl.value;

                TIR_Value val = {
                    .valuespace = TVS_GLOBAL,
                    .offset = globals_ofset++,
                    .type = global.get_pointer_type(((AST_Var*)decl.value)->type),
                };
                valmap[var] = val;
                break;
            }

            default: {
                continue;
            }
        }
    }

    for(auto& decl : global.declarations) {
        switch (decl.value->nodetype) {
            case AST_FN: {
                AST_Fn* fn = (AST_Fn*)decl.value;
                
                TIR_Function* tir_fn = new TIR_Function(*this, fn);

                tir_fn->compile_signature();

                fns.insert(fn, tir_fn);

                break;
            }

            // We already ran through the variables in the previous loop
            case AST_VAR:
                continue;

            // Nothing to do for structs
            case AST_STRUCT:
                continue;

            default:
                NOT_IMPLEMENTED();
        }
    }

    for (auto& fn : fns) {
        fn.value->compile();
    }
}
