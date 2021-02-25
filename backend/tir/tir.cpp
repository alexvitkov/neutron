#include "tir.h"
#include "../../typer.h"
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



void TIR_Function::emit(TIR_Instruction inst) {
    writepoint->instructions.push(inst);
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
            
            o << "call " << instr.call.fn->ast_fn->name << "(";

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
                *out = var->is_global ? fn.c.global_valmap[var] : fn.fn_valmap[var];
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
        fn.emit({
            .opcode = TOPC_JMP,
            .jmp = { after }
        });
        after->previous_blocks.push_unique(tir_block);
    }
}

struct TIR_FnCompileJob : Job {
    TIR_Function *tir_fn;

    bool run(Message *msg) override {
        tir_fn->compile_signature();

        AST_Type* rettype = ((AST_FnType*)tir_fn->ast_fn->type)->returntype;
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

    TIR_FnCompileJob(TIR_Function *tir_fn) : tir_fn(tir_fn), Job(tir_fn->c.global) {}
};

TIR_Value get_array_ptr(TIR_Function& fn, AST_Value* arr) {
    switch (arr->nodetype) {
        case AST_VAR: {
            AST_Var* var = (AST_Var*)arr;

            if (var->is_global) {
                return fn.c.global_valmap[var];
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
                case OP_ADD:             opcode = TOPC_ADD; break;
                case OP_SUB:             opcode = TOPC_SUB; break;
                case OP_MUL:             opcode = TOPC_MUL; break;
                case OP_DIV:             opcode = TOPC_DIV; break;
                case OP_MOD:             opcode = TOPC_MOD; break;
                case OP_SHIFTLEFT:       opcode = TOPC_SHL; break;
                case OP_SHIFTRIGHT:      opcode = TOPC_SHR; break;
                case OP_DOUBLEEQUALS:    opcode = TOPC_EQ;  break;
                case OP_LESSTHAN:        opcode = TOPC_LT;  break;
                case OP_LESSEREQUALS:    opcode = TOPC_LTE; break;
                case OP_GREATERTHAN:     opcode = TOPC_GT;  break;
                case OP_GREATEREQUALS:   opcode = TOPC_GTE; break;
                case OP_ADD_PTR_INT:     opcode = TOPC_GEP; break;
                default:
                    NOT_IMPLEMENTED();
            }

            if (lhs.type IS AST_PRIMITIVE_TYPE) {
                AST_PrimitiveType *prim = (AST_PrimitiveType*)lhs.type;

                if (opcode != TOPC_SHL && opcode != TOPC_SHR) {
                    switch (prim->kind) {
                        case PRIMITIVE_SIGNED: 
                            opcode = (TIR_OpCode)(opcode | TOPC_SIGNED); 
                            break;
                        case PRIMITIVE_UNSIGNED: 
                            opcode = (TIR_OpCode)(opcode | TOPC_UNSIGNED); 
                            break;
                        case PRIMITIVE_FLOAT: 
                            opcode = (TIR_OpCode)(opcode | TOPC_FLOAT); 
                            break;
                        default:
                            UNREACHABLE;
                    }
                }
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

            TIR_Function *tir_callee = fn.c.fns[callee];
            assert(tir_callee);

            arr<TIR_Value> args;

            for (u32 i = 0; i < fncall->args.size; i++) {

                AST_Type* param_type = &t_any8;
                if (i < tir_callee->parameters.size)
                    param_type = tir_callee->parameters[i].type;

                TIR_Value  arg_dst = fn.alloc_temp(param_type);
                TIR_Value arg;

                if (fncall->args[i]->type IS AST_STRUCT) {
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
                .call = { .dst = dst, .fn = tir_callee, .args = args.release(), }
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
                    return var->is_global ? fn.c.global_valmap[var] : fn.fn_valmap[var];
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

            AST_Var* strvar = (AST_Var*)fn.c.global.declarations.find2(key);
            assert(strvar);

            TIR_Value val = fn.c.global_valmap[str];

            if (!dst)
                dst = fn.alloc_temp(str->type);

            fn.emit({ .opcode = TOPC_MOV, .un = { .dst = dst, .src = val } });

            return val;
        }

        case AST_CAST: {
            AST_Cast* cast = (AST_Cast*)node;

            // TODO this is probobobably unnnecessary
            if (cast->inner->type IS AST_ARRAY_TYPE 
                    && cast->type IS AST_POINTER_TYPE) 
            {
                TIR_Value var_location = get_array_ptr(fn, cast->inner);
                TIR_Value offset_0 = { .valuespace = TVS_VALUE, .offset = 0, .type = &t_u32 };
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
            } else {
                TIR_Value src = compile_node_rvalue(fn, cast->inner, {});
                if (!dst) dst = fn.alloc_temp(cast->type);

                TIR_OpCode topc;
                switch (cast->casttype) {
                    case AST_Cast::SignedExtend:
                        topc = TOPC_SEXT;
                        break;
                    case AST_Cast::ZeroExtend:
                        topc = TOPC_ZEXT;
                        break;
                    default:
                        topc = TOPC_BITCAST;
                        break;
                }

                fn.emit({
                    .opcode = topc,
                    .un = { .dst = dst, .src = src }
                });
                
                return dst;
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

    for (auto& kvp : this->ast_fn->block.declarations) {
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
    TIR_Value var;
    TIR_GlobalVarInitJob(TIR_Value var, TIR_Context *tir_context);

    AST_Var *varr;


    TIR_GlobalVarInitJob(TIR_Context &tir_context, AST_BinaryOp *assignment) 
        : TIR_ExecutionJob(&tir_context) 
    {
        var = tir_context.global_valmap[assignment->lhs];
        varr = (AST_Var*)assignment->lhs;

        // This function calculates the initial value and returns it
        TIR_Function *pseudo_fn = new TIR_Function(tir_context, nullptr);
        TIR_Block* entry = new TIR_Block();
        pseudo_fn->retval = {
            .valuespace = TVS_RET_VALUE,
            .type = var.type,
        };
        pseudo_fn->writepoint = entry;
        pseudo_fn->blocks.push(entry);

        compile_node_rvalue(*pseudo_fn, assignment->rhs, pseudo_fn->retval);
        pseudo_fn->emit({ .opcode = TOPC_RET });

        arr<void*> _args;
        call(pseudo_fn, _args);

        tir_context.global_initializer_running_jobs[var.offset] = this;
    }

    std::wstring get_name() override {
        std::wostringstream stream;
        stream << L"init<" << varr->name << ">";
        return stream.str();
    }

    virtual void on_complete(void *value) override {
        tir_context->_global_initial_values[var.offset]
            = pack_into_const(value, varr->type);

        // TODO DS DELETE
        tir_context->global_initializer_running_jobs[var.offset] = nullptr;
        tir_context->storage->global_values[var.offset] = value;

        wcout.flush();
    }
};

Job *TIR_Context::compile_fn(AST_Fn *fn, Job *fn_typecheck_job) {
    TIR_Function* tir_fn = new TIR_Function(*this, fn);
    fns.insert(fn, tir_fn);

    TIR_FnCompileJob *compile_job = new TIR_FnCompileJob(tir_fn);
    compile_job->add_dependency(fn_typecheck_job);
    tir_fn->c.global.add_job(compile_job);
    return compile_job;
}

void TIR_Context::compile_all() {
    storage = new TIR_ExecutionStorage();

    // Run through the global variables and assign them TIR_Values
    for(auto& decl : global.declarations) {
        switch (decl.value->nodetype) {
            case AST_VAR: {
                AST_Var* var = (AST_Var*)decl.value;
                TIR_Value val = {
                    .valuespace = TVS_GLOBAL,
                    .offset = globals_count++,
                    .type = global.get_pointer_type(((AST_Var*)decl.value)->type),
                };
                global_valmap[var] = val;
                break;
            }
            default: continue;
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
            default: continue;
        }
    }

    for (auto& stmt : global.statements) {
        switch (stmt->nodetype) {
            case AST_ASSIGNMENT: {
                AST_BinaryOp *assignment = (AST_BinaryOp*)stmt;
                new TIR_GlobalVarInitJob(*this, assignment);
                break;
            }
            default:
                UNREACHABLE;
        }
    }


    //for (auto& fn : fns) {
        //fn.value->compile();
    //}
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
    StackFrame &sf = stackframes.push({
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
                            tir_context->storage->global_values[instr.un.dst.offset] 
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
                            if (tir_context->storage->global_values.find(instr.un.src.offset, &val)) {
                                sf.set_value(instr.un.dst, val);
                            } else {
                                // If there's no value assigned to the global, look for a initial value.
                                // The initial value is assigned via a job that may not be completed, 
                                // so if the job isn't done we have to wait on it
                                Job *job;
                                if (tir_context->global_initializer_running_jobs.find(instr.un.src.offset, &job)) {
                                    // TODO DS DELETE
                                    assert(job);
                                    this->add_dependency(job);
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

