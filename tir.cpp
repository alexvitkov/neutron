#include "tir.h"
#include "typer.h"
#include <iostream>
#include <sstream>

TIR_Value::operator bool() { return valuespace; }
bool operator== (TIR_Value& lhs, TIR_Value& rhs) {
    return lhs.valuespace == rhs.valuespace && lhs.offset == rhs.offset;
}
bool operator!= (TIR_Value& lhs, TIR_Value& rhs) {
    return !(lhs == rhs);
}
u32 map_hash(TIR_Value data)                  { return data.valuespace ^ data.offset; }
bool map_equals(TIR_Value lhs, TIR_Value rhs) { return lhs == rhs; }



void TIR_Function::emit(TIR_Instruction instr) {
    writepoint->instructions.push(instr);
}

TIR_Value TIR_Function::alloc_temp(AST_Type* type) {
    return {
        .valuespace = TVS_TEMP,
        .offset = temps_count++,
        .type = type,
    };
}

TIR_Value TIR_Function::alloc_stack(AST_Var* var) {
    TIR_Value val {
        .valuespace = TVS_STACK,
        .offset = temps_count++,
        .type = tir_context->global.get_pointer_type(var->type),
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
            break;
        case TVS_C_STRING_LITERAL:
            print_string(o, (const char*)val.offset); // TODO POINTERSIZE
            break;
        case TVS_AST_VALUE:
            o << "AST: " << (AST_Node*)val.offset; // TODO POINTERSIZE
            break;
    }
    return o;
}

TIR_Block::TIR_Block() {
    static u64 next_id;
    id = next_id++;
}

std::wostream& operator<< (std::wostream& o, TIR_Instruction& instr) {
    o << "    ";

    if ((instr.opcode & TOPC_BINARY) == TOPC_BINARY) {
            o << instr.bin.dst << " <- " << instr.bin.lhs;

            switch (instr.opcode) {
                case TOPC_UADD: case TOPC_SADD: case TOPC_FADD: 
                    o << " + ";  
                    break;
                case TOPC_USUB: case TOPC_SSUB: case TOPC_FSUB: 
                    o << " - ";  
                    break;
                case TOPC_UMUL: case TOPC_SMUL: case TOPC_FMUL: 
                    o << " * ";  
                    break;
                case TOPC_UDIV: case TOPC_SDIV: case TOPC_FDIV: 
                    o << " / ";  
                    break;
                case TOPC_UMOD: case TOPC_SMOD: case TOPC_FMOD: 
                    o << " / ";  
                    break;
                case TOPC_UEQ:  case TOPC_SEQ:  case TOPC_FEQ:  
                    o << " == "; 
                    break;
                case TOPC_ULT:  case TOPC_SLT:  case TOPC_FLT:  
                    o << " < ";  
                    break;
                case TOPC_UGT:  case TOPC_SGT:  case TOPC_FGT:  
                    o << " > ";  
                    break;
                case TOPC_ULTE: case TOPC_SLTE: case TOPC_FLTE: 
                    o << " <= "; 
                    break;
                case TOPC_UGTE: case TOPC_SGTE: case TOPC_FGTE: 
                    o << " >= "; 
                    break;
                case TOPC_SHL:
                    o << " << "; 
                    break;
                case TOPC_SHR:
                    o << " >> "; 
                    break;
                default: UNREACHABLE;
            }

            o << instr.bin.rhs << std::endl;
            return o;
    }

    switch (instr.opcode) {
        case TOPC_MOV:
            o << instr.un.dst << " <- " << instr.un.src << std::endl;
            break;
        case TOPC_BITCAST:
            o << instr.un.dst << " <- bitcast " << instr.un.src << std::endl;
            break;
        case TOPC_SEXT:
            o << instr.un.dst << " <- sext " << instr.un.src << std::endl;
            break;
        case TOPC_ZEXT:
            o << instr.un.dst << " <- zext " << instr.un.src << std::endl;
            break;
        case TOPC_LOAD:
            o << instr.un.dst << " <- load " << instr.un.src << std::endl;
            break;
        case TOPC_STORE:
            o << "store " << instr.un.src << " -> " << instr.un.dst << std::endl;
            break;
        case TOPC_GEP: {
            o << instr.gep.dst << " <- GEP " << instr.gep.base << "";
            for (TIR_Value& v : instr.gep.offsets)
                o << '[' << v << ']';
            o << "\n";
            break;
        }
        case TOPC_RET:
            o << "ret" << std::endl;
            break;

        case TOPC_CALL: {
            if (instr.call.dst.valuespace != TVS_DISCARD)
                o << instr.call.dst << " <- ";
            
            if (instr.call.fn->ast_fn)
                o << "call " << instr.call.fn->ast_fn->name << "(";
            else 
                o << "call ?(";

            for (TIR_Value& val : instr.call.args)
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
                *out = var->is_global ? fn.tir_context->global_valmap[var] : fn.fn_valmap[var];
                return true;
            }
            else {
                *out = fn.fn_valmap[var];
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

            *out = fn.alloc_temp(fn.tir_context->global.get_pointer_type(member_access->type));
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


void compile_block(TIR_Function &fn, TIR_Block *tir_block, AST_Context *ast_block, TIR_Block *after) {
    fn.writepoint = tir_block;

    for (auto& kvp : ast_block->declarations) {
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

                fn.fn_valmap.insert(vardecl, val);
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
        if (fn.writepoint->instructions.size > 0 && fn.writepoint->instructions.last().opcode == TOPC_RET) {
            return;
        }
        fn.emit({
            .opcode = TOPC_JMP,
            .jmp = { after }
        });
        after->push_previous(tir_block);
    }
}

struct TIR_FnCompileJob : Job {
    TIR_Function *tir_fn;

    bool run(Message *msg) override {
        tir_fn->compile_signature();

        AST_Type* rettype = tir_fn->ast_fn->fntype()->returntype;
        if (rettype) 
            tir_fn->retval.type = rettype;

        if (!tir_fn->ast_fn->is_extern) {
            TIR_Block* entry = new TIR_Block();
            tir_fn->blocks.push(entry);
            compile_block(*tir_fn, entry, &tir_fn->ast_fn->block, nullptr);
        }
        return true;
    }

    std::wstring get_name() override {
        std::wostringstream s;
        s << L"TIR_FnCompileJob<" << tir_fn->ast_fn->name << L">";
        return s.str();
    }

    TIR_FnCompileJob(TIR_Function *tir_fn) : tir_fn(tir_fn), Job(tir_fn->tir_context->global) {}
};

TIR_Value get_array_ptr(TIR_Function& fn, AST_Value* arr) {
    switch (arr->nodetype) {
        case AST_VAR: {
            AST_Var* var = (AST_Var*)arr;

            if (var->is_global) {
                return fn.tir_context->global_valmap[var];
            } else {
                NOT_IMPLEMENTED();
            }

            break;
        }
        default:
            UNREACHABLE;
    }
}


void TIR_Block::push_previous(TIR_Block *previous) {
    previous_blocks.push_unique(previous);
    /*
    std::wcout << id << " after " << previous->id << "\n";

    for (TIR_Block *b : previous->previous_blocks) {
        if (b->id == id) {
            assert(!"AAAAAAAAA");
        }
    }
    */
}

struct InlineFnState {
    TIR_Function &fn;
    TIR_Function &callee;
    TIR_Value    retval;
    arr<TIR_Value> &args;
    u64 temps_start;
};

TIR_Value translate_inline_value(TIR_Value val, InlineFnState &state) {
    assert(val.type);
    switch (val.valuespace) {
        case TVS_RET_VALUE: {
            return state.retval;
        }
        case TVS_ARGUMENT: {
            return state.args[val.offset];
        }
        case TVS_TEMP: {
            val.offset += state.temps_start;
            return val;
        }
        default: {
            NOT_IMPLEMENTED();
        }
    }
}

void inline_function(InlineFnState &state) {
    state.temps_start = state.fn.temps_count;
    state.fn.temps_count += state.callee.temps_count;

    if (state.callee.blocks.size > 1) {
        NOT_IMPLEMENTED();
    } else {
        for (TIR_Instruction& instr : state.callee.blocks[0]->instructions) {
            if ((instr.opcode & TOPC_UNARY) == TOPC_UNARY) {
                state.fn.emit({
                    .opcode = instr.opcode,
                    .un = {
                        .dst = translate_inline_value(instr.un.dst, state),
                        .src = translate_inline_value(instr.un.src, state),
                    }
                });
            } else if ((instr.opcode & TOPC_BINARY) == TOPC_BINARY) {
                state.fn.emit({
                    .opcode = instr.opcode,
                    .bin = {
                        .dst = translate_inline_value(instr.bin.dst, state),
                        .lhs = translate_inline_value(instr.bin.lhs, state),
                        .rhs = translate_inline_value(instr.bin.rhs, state),
                    }
                });
            } else if (instr.opcode == TOPC_RET) {
                return;
            } else {
                NOT_IMPLEMENTED();
            }
        }
    }
}

TIR_Value compile_node_rvalue(TIR_Function& fn, AST_Node* node, TIR_Value dst) {
    switch (node->nodetype) {
        case AST_VAR: {
            AST_Var *var = (AST_Var*)node;

            if (var->is_constant) {
                if (!var->is_global)
                    NOT_IMPLEMENTED();
                if (!dst)
                    dst = fn.alloc_temp(((AST_Value*)node)->type);

                TIR_Value src = fn.tir_context->_global_initial_values[fn.tir_context->global_valmap[var].offset];

                TIR_Value offset_0 = { .valuespace = TVS_VALUE, .offset = 0, .type = &t_u32 };
                arr<TIR_Value> offsets = { offset_0, offset_0 };

                fn.emit({ 
                    .opcode =  TOPC_GEP, 
                    .gep = { 
                        .dst = dst, 
                        .base = src, 
                        .offsets = offsets.release(),
                    },
                });

                return dst;
            }
            // FALLTHROUGH
        }
        case AST_MEMBER_ACCESS:
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


        case AST_FN_CALL: {
            AST_Call* fncall = (AST_Call*)node;

            if (fncall->builder && fncall->builder->use_emit2) {
                fncall->builder->emit2(fn, fncall->args, dst);
                return dst;
            }

            arr<TIR_Value> args;

            for (u32 i = 0; i < fncall->args.size; i++) {
                TIR_Value arg;

                if (fncall->args[i]->type IS AST_STRUCT) {
                    assert (get_location(fn, fncall->args[i], &arg));
                } else {
                    arg = compile_node_rvalue(fn, fncall->args[i], {});
                }
                args.push(arg);
            }

            if (fncall->builder) {
                dst = fncall->builder->emit1(fn, args, dst);
                return dst;
            }

            TIR_Function *tir_callee;
            assert(fncall->fn);
            assert(fncall->fn IS AST_FN);
            AST_Fn *callee = (AST_Fn*)fncall->fn;
            tir_callee = fn.tir_context->fns[callee];

            if (!dst && fn.retval) {
                dst = fn.alloc_temp(fn.retval.type);
            }

            if (tir_callee->is_inline) {
                InlineFnState state {
                    .fn     = fn,
                        .callee = *tir_callee,
                        .retval = dst,
                        .args   = args,
                };
                inline_function(state);
            } else {
                fn.emit({ 
                        .opcode = TOPC_CALL, 
                        .call = { .dst = dst, .fn = tir_callee, .args = args.release(), }
                        });
            }

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
            AST_Context* ast_block = (AST_Context*)node;

            // TODO ALLOCATION
            TIR_Block* inner_block = new TIR_Block();
            TIR_Block* continuation = new TIR_Block();

            compile_block(fn, inner_block, ast_block, continuation);

            fn.blocks.push(inner_block);
            fn.blocks.push(continuation);

            fn.writepoint = continuation;
            return {};
        }

        case AST_ADDRESS_OF: {
            AST_AddressOf *addrof = (AST_AddressOf*)node;

            switch (addrof->inner->nodetype) {
                case AST_VAR: {
                    AST_Var* var = (AST_Var*)addrof->inner;
                    return var->is_global ? fn.tir_context->global_valmap[var] : fn.fn_valmap[var];
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
            then_block->push_previous(original_block);
            continuation->push_previous(original_block);

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

            cond_block->push_previous(original_block);
            cond_block->push_previous(loop_block);

            continuation->push_previous(cond_block);

            loop_block->push_previous(cond_block);

            fn.blocks.push(cond_block);
            fn.blocks.push(loop_block);
            fn.blocks.push(continuation);

            fn.writepoint = continuation;
            return {};
        }

        case AST_STRING_LITERAL: {
            AST_StringLiteral* str = (AST_StringLiteral*)node;

            DeclarationKey key = { .string_literal = str };

            AST_Var* strvar = (AST_Var*)fn.tir_context->global.declarations.find2(key);
            assert(strvar);

            TIR_Value val = fn.tir_context->global_valmap[str];

            if (!dst)
                dst = fn.alloc_temp(str->type);

            fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = val } });

            return val;
        }

        case AST_SMALL_NUMBER: {
            AST_SmallNumber   *num  = (AST_SmallNumber*)node;
            AST_PrimitiveType *type = (AST_PrimitiveType*)num->type;

            assert(type->kind == PRIMITIVE_UNSIGNED);

            TIR_Value val = { .valuespace = TVS_VALUE, .offset = num->u64_val, .type = num->type };

            if (dst) {
                fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = val } });
                return dst;
            } else {
                return val;
            }
        }

        case AST_ZEROEXTEND: {
            AST_ZeroExtend *z = (AST_ZeroExtend*)node;
            if (!dst)
                dst = fn.alloc_temp(z->type);

            TIR_Value src = compile_node_rvalue(fn, z->inner, {});
            fn.emit({ .opcode = TOPC_ZEXT, .un = { .dst = dst, .src = src } });

            return dst;
        }

        case AST_NUMBER: {
            // number literals should be converted to AST_SMALL_NUMBERs
            UNREACHABLE;
        }


        default:
            UNREACHABLE;
    }
}

void TIR_Function::compile_signature() {
    AST_FnType *fntype = ast_fn->fntype();
    // TODO VOID
    if (fntype->returntype && fntype->returntype != &t_void) {
        retval.valuespace = TVS_RET_VALUE;
        retval.type = fntype->returntype;
    }
    else {
        retval.valuespace = TVS_DISCARD;
        retval.type = &t_void;
    }

    for (auto& kvp : this->ast_fn->block.declarations) {
        if (kvp.value IS AST_VAR) {
            AST_Var *vardecl = (AST_Var*)kvp.value;

            if (vardecl->argindex < 0)
                continue;

            AST_Type *arg_type = vardecl->type;
            TIR_Value_Flags flags = (TIR_Value_Flags)0;

            if (vardecl->type IS AST_STRUCT) {
                arg_type = tir_context->global.get_pointer_type(vardecl->type);
                flags = TVF_BYVAL;
            }

            TIR_Value val = {
                .valuespace = TVS_ARGUMENT,
                .flags = flags,
                .offset = (u64)vardecl->argindex,
                .type = arg_type,
            };

            // TODO DS - RESERVE
            while (parameters.size <= vardecl->argindex)
                parameters.push({});
            parameters[vardecl->argindex] = val;
        }
    }
}


TIR_Value pack_into_const(void *val, AST_Type *type) {
    switch (type->nodetype) {
        case AST_PRIMITIVE_TYPE: {
            AST_PrimitiveType *prim = (AST_PrimitiveType*)type;
            switch (prim->kind) {
                case PRIMITIVE_UNSIGNED: {
                    return {
                        .valuespace = TVS_VALUE,
                        .offset = (u64)val,
                        .type = type,
                    };
                }
                case PRIMITIVE_SIGNED: {
                    return {
                        .valuespace = TVS_VALUE,
                        .offset = (u64)val,
                        .type = type,
                    };
                }
                default:
                    NOT_IMPLEMENTED();
            }
        }
        default: {
            NOT_IMPLEMENTED();
        }
    }
}


TIR_ExecutionJob::TIR_ExecutionJob(TIR_Context *tir_context) 
    : Job(tir_context->global),
      tir_context(tir_context) {}


struct TIR_GlobalVarInitJob : TIR_ExecutionJob {
    TIR_Function *pseudo_fn;
    TIR_Value     tir_var;
    AST_Var      *ast_var;

    TIR_GlobalVarInitJob(TIR_Value var, TIR_Context *tir_context);

    TIR_GlobalVarInitJob(TIR_Context &tir_context, AST_Call *assignment) 
        : TIR_ExecutionJob(&tir_context) 
    {
        tir_var = tir_context.global_valmap[assignment->args[0]];
        ast_var = (AST_Var*)assignment->args[0];

        // This function calculates the initial value and returns it
        TIR_Function *pseudo_fn = new TIR_Function(&tir_context, nullptr);
        TIR_Block* entry = new TIR_Block();
        pseudo_fn->retval = {
            .valuespace = TVS_RET_VALUE,
            .type = tir_var.type,
        };
        pseudo_fn->writepoint = entry;
        pseudo_fn->blocks.push(entry);

        compile_node_rvalue(*pseudo_fn, assignment->args[1], pseudo_fn->retval);
        pseudo_fn->emit({ .opcode = TOPC_RET });

        arr<void*> _args;
        call(pseudo_fn, _args);

        // tir_context.global_initializer_running_jobs[tir_var.offset];
    }

    std::wstring get_name() override {
        std::wostringstream stream;
        stream << L"TIR_GlobalVarInitJob<" << ast_var->name << ">";
        return stream.str();
    }

    virtual void on_complete(void *value) override {
        tir_context->_global_initial_values[tir_var.offset]
            = pack_into_const(value, ast_var->type);

        // TODO DS DELETE
        tir_context->global_initializer_running_jobs[tir_var.offset] = heapify<TIR_GlobalVarInitJob>();
        tir_context->storage.global_values[tir_var.offset] = value;

        wcout.flush();
    }
};

HeapJob *TIR_Context::compile_fn(AST_Fn *fn, HeapJob *fn_typecheck_job) {
    TIR_Function* tir_fn = new TIR_Function(this, fn);
    fns.insert(fn, tir_fn);

    TIR_FnCompileJob _compile_job(tir_fn);
    HeapJob *compile_job = _compile_job.heapify<TIR_FnCompileJob>();

    compile_job->add_dependency(fn_typecheck_job, true);
    tir_fn->tir_context->global.add_job(compile_job);

    tir_fn->compile_job = compile_job;
    return compile_job;
}


TIR_Value TIR_Context::append_global(AST_Var *var) {
    TIR_Value val = {
        .valuespace = TVS_GLOBAL,
        .offset = globals_count++,
        .type = global.get_pointer_type(var->type),
    };
    global_valmap[var] = val;
    return val;
}


void add_string_global(TIR_Context *tir_context, AST_Var *the_string_var, AST_StringLiteral *the_string_literal) {
    TIR_Value array_val = {
        .valuespace = TVS_C_STRING_LITERAL,
        .offset = (u64)the_string_literal->str, // POINTERSIZE
        .type = &t_string_literal,
    };

    TIR_Value the_string_var_tir = tir_context->append_global(the_string_var);
    tir_context->_global_initial_values[the_string_var_tir.offset] = array_val;
}

void TIR_Context::compile_all() {

    // Top level assignments are always global varaible initial values
    // We handle them specially, with a CTE job
    for (auto& stmt : global.statements) {
        switch (stmt->nodetype) {
            case AST_FN_CALL: {
                NOT_IMPLEMENTED();
                AST_Call *assignment = (AST_Call*)stmt;
                TIR_GlobalVarInitJob _init(*this, assignment);
                HeapJob *init = _init.heapify<TIR_GlobalVarInitJob>();
                global.add_job(init);
                break;
            }
            default:
                UNREACHABLE;
        }
    }
}

void TIR_ExecutionJob::StackFrame::set_value(TIR_Value key, void *val) {
    switch (key.valuespace) {
        case TVS_DISCARD: {
            break;
        }
        case TVS_GLOBAL: {
            NOT_IMPLEMENTED();
        }
        case TVS_ARGUMENT: {
            args[key.offset] = val;
            break;
        }
        case TVS_RET_VALUE: {
            retval = val;
            break;
        }
        case TVS_TEMP: {
            tmp[key.offset] = val;
            break;
        }
        case TVS_STACK: {
            memcpy(stack + key.offset, val, key.type->size);
            break;
        }
        case TVS_VALUE: {
            UNREACHABLE;
        }
        default:
            NOT_IMPLEMENTED();
    }
}

void *TIR_ExecutionJob::StackFrame::get_value(TIR_Value key) {
    switch (key.valuespace) {
        case TVS_GLOBAL: {
            NOT_IMPLEMENTED();
        }
        case TVS_ARGUMENT: {
            return args[key.offset];
            break;
        }
        case TVS_RET_VALUE: {
            return retval;
            break;
        }
        case TVS_TEMP: {
            return tmp[key.offset];
            break;
        }
        case TVS_STACK: {
            return stack + key.offset;
            break;
        }
        case TVS_VALUE: {
            return (void*)key.offset;
        }
        case TVS_DISCARD: {
            UNREACHABLE;
        }
        default:
            NOT_IMPLEMENTED();
    }
}

void TIR_ExecutionJob::call(TIR_Function *fn, arr<void*> &args) {
    stackframes.push({
        .fn = fn,
        .block = nullptr,
        .next_instruction = 0,
        .stack = (u8*)malloc(fn->stack_size),
        .args = args,
        .tmp = arr<void*>(fn->temps_count),
    });
}

bool TIR_ExecutionJob::run(Message *message) {
    assert(!message);

NextFrame:
    if (stackframes.size == 0) {
        on_complete(next_retval);
        return true;
    }

    while (true) {
        StackFrame &sf = stackframes.last();

        if (!sf.block)
            sf.block = sf.fn->blocks[0];

        assert(sf.next_instruction < sf.block->instructions.size);
        TIR_Instruction &instr = sf.block->instructions[sf.next_instruction];

        if ((instr.opcode & TOPC_BINARY) == TOPC_BINARY) {
            void *lhs = sf.get_value(instr.bin.lhs);
            void *rhs = sf.get_value(instr.bin.rhs);
            void *val;

            switch (instr.opcode) {
                case TOPC_UADD: val = (void*)((u64)lhs +  (u64)rhs); break;
                case TOPC_USUB: val = (void*)((u64)lhs -  (u64)rhs); break;
                case TOPC_UMUL: val = (void*)((u64)lhs *  (u64)rhs); break;
                case TOPC_UDIV: val = (void*)((u64)lhs /  (u64)rhs); break;
                case TOPC_UMOD: val = (void*)((u64)lhs %  (u64)rhs); break;
                case TOPC_UEQ : val = (void*)((u64)lhs == (u64)rhs); break;
                case TOPC_ULT : val = (void*)((u64)lhs <  (u64)rhs); break;
                case TOPC_ULTE: val = (void*)((u64)lhs <= (u64)rhs); break;
                case TOPC_UGT : val = (void*)((u64)lhs >  (u64)rhs); break;
                case TOPC_UGTE: val = (void*)((u64)lhs >= (u64)rhs); break;

                case TOPC_SADD: val = (void*)((i64)lhs +  (i64)rhs); break;
                case TOPC_SSUB: val = (void*)((i64)lhs -  (i64)rhs); break;
                case TOPC_SMUL: val = (void*)((i64)lhs *  (i64)rhs); break;
                case TOPC_SDIV: val = (void*)((i64)lhs /  (i64)rhs); break;
                case TOPC_SMOD: val = (void*)((i64)lhs %  (i64)rhs); break;
                case TOPC_SEQ : val = (void*)((i64)lhs == (i64)rhs); break;
                case TOPC_SLT : val = (void*)((i64)lhs <  (i64)rhs); break;
                case TOPC_SLTE: val = (void*)((i64)lhs <= (i64)rhs); break;
                case TOPC_SGT : val = (void*)((i64)lhs >  (i64)rhs); break;
                case TOPC_SGTE: val = (void*)((i64)lhs >= (i64)rhs); break;

                default:
                    NOT_IMPLEMENTED();
            }

            sf.set_value(instr.bin.dst, val);
        } else {
            switch (instr.opcode) {
                case TOPC_RET: {
                    free(sf.stack);
                    stackframes.pop();
                    has_next_retval = true;
                    next_retval = sf.retval;
                    goto NextFrame;
                }

                case TOPC_MOV: {
                    sf.set_value(instr.un.dst, sf.get_value(instr.un.src));
                    break;
                }

                case TOPC_STORE: {
                    switch (instr.un.dst.valuespace) {
                        case TVS_GLOBAL:
                            NOT_IMPLEMENTED("not here");
                            tir_context->storage.global_values[instr.un.dst.offset] 
                                = sf.get_value(instr.un.src);
                            break;
                        default:
                            NOT_IMPLEMENTED();
                    }
                    break;
                }

                case TOPC_LOAD: {
                    switch (instr.un.src.valuespace) {
                        case TVS_GLOBAL:
                            void *val;
                            // If there is already a value assigned, use that
                            if (tir_context->storage.global_values.find(instr.un.src.offset, &val)) {
                                sf.set_value(instr.un.dst, val);
                            } else {
                                // If there's no value assigned to the global, look for a initial value.
                                // The initial value is assigned via a job that may not be completed, 
                                // so if the job isn't done we have to wait on it
                                HeapJob *job;
                                if (tir_context->global_initializer_running_jobs.find(instr.un.src.offset, &job)) {
                                    // TODO DS DELETE
                                    assert(job);
                                    // TODO TODO 
                                    // heapify<TIR_ExecutionJob>()->add_dependency(job);
                                    NOT_IMPLEMENTED();
                                    return false;
                                } else {
                                    UNREACHABLE;
                                }
                            }
                            break;
                        default:
                            NOT_IMPLEMENTED();
                    }
                    break;
                }

                case TOPC_CALL: {
                    if (has_next_retval) {
                        sf.set_value(instr.call.dst, next_retval);
                        has_next_retval = false;
                        break;
                    }
                    else {
                        arr<void*> args;

                        for (const TIR_Value& arg : instr.call.args) {
                            args.push(sf.get_value(arg));
                        }

                        call(instr.call.fn, args);

                        if (instr.call.fn->blocks.size == 0) {
                            NOT_IMPLEMENTED();
                            // heapify<TIR_ExecutionJob>()->add_dependency (instr.call.fn->compile_job);
                            return false;
                        }

                        goto NextFrame;
                    }
                }

                case TOPC_JMP: {
                    sf.block = instr.jmp.next_block;
                    sf.next_instruction = 0;
                    goto NextFrame;
                }

                case TOPC_JMPIF: {
                    if (sf.get_value(instr.jmpif.cond)) {
                        sf.block = instr.jmpif.then_block;
                    } else {
                        sf.block = instr.jmpif.else_block;
                    }
                    sf.next_instruction = 0;
                    goto NextFrame;
                }

                case TOPC_SEXT: {
                    AST_PrimitiveType *srct = (AST_PrimitiveType*)instr.un.src.type;
                    AST_PrimitiveType *dstt = (AST_PrimitiveType*)instr.un.dst.type;
                    size_t src = (size_t)sf.get_value(instr.un.src);

                    int sing_bit = srct->size * 8 - 1;
                    bool is_negative = src & (1 << sing_bit);

                    if (is_negative) {
                        NOT_IMPLEMENTED();
                    }
                    else {
                        sf.set_value(instr.dst, sf.get_value(instr.un.src));
                        break;
                    }

                }

                case TOPC_BITCAST:
                case TOPC_ZEXT: 
                {
                    sf.set_value(instr.dst, sf.get_value(instr.un.src));
                    break;
                }

                default:
                    NOT_IMPLEMENTED();
            }
        }

        sf.next_instruction ++;
    }
}

TIR_Function::TIR_Function(std::initializer_list<TIR_Instruction> instrs) {
    tir_context = nullptr;
    ast_fn = nullptr;
    writepoint = blocks.push(new TIR_Block());

    for (TIR_Instruction instr : instrs) {
        emit(instr);
    }
}
