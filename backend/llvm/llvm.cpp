#include "llvm.h"
#include <iostream>

using namespace llvm;

T2L_Context::T2L_Context(TIR_Context& tirc) 
    : tir_context(tirc), mod("ntrmod", lc), builder(lc) 
{
    translated_types.insert(&t_bool, IntegerType::get(lc, 1));
    translated_types.insert(&t_u8,   IntegerType::get(lc, 8));
    translated_types.insert(&t_u16,  IntegerType::get(lc, 16));
    translated_types.insert(&t_u32,  IntegerType::get(lc, 32));
    translated_types.insert(&t_u64,  IntegerType::get(lc, 64));
    translated_types.insert(&t_i8,   IntegerType::get(lc, 8));
    translated_types.insert(&t_i16,  IntegerType::get(lc, 16));
    translated_types.insert(&t_i32,  IntegerType::get(lc, 32));
    translated_types.insert(&t_i64,  IntegerType::get(lc, 64));
    translated_types.insert(&t_f32,  Type::getFloatTy(lc));
    translated_types.insert(&t_f64,  Type::getFloatTy(lc));
    translated_types.insert(&t_void, Type::getVoidTy(lc));
    translated_types.insert(&t_any8, IntegerType::get(lc, 64));
}

llvm::Type* T2L_Context::get_llvm_type(AST_Type* type) {
    llvm::Type* t;
    if (translated_types.find(type, &t))
        return t;

    switch (type->nodetype) {
        case AST_FN_TYPE: {
            AST_FnType* fntype = (AST_FnType*)type;

            arr<llvm::Type*> l_param_types;

            for (auto& param : fntype->param_types)
                l_param_types.push(get_llvm_type(param));

            llvm::Type* returntype;
            if (!fntype->returntype || fntype->returntype == &t_void) {
                returntype = translated_types[&t_void];
            }
            else {
                returntype = get_llvm_type(fntype->returntype);
            }

            FunctionType* l_fn_type = FunctionType::get(
                    returntype, 
                    ArrayRef<llvm::Type*>(l_param_types.buffer, l_param_types.size),
                    fntype->is_variadic);

            translated_types.insert(type, l_fn_type);
            return l_fn_type;
        }

        case AST_POINTER_TYPE: {
            AST_PointerType* pt = (AST_PointerType*)type;
            llvm::Type* l_pointed_type = get_llvm_type(pt->pointed_type);
            llvm::Type* l_pt = PointerType::get(l_pointed_type, 0);

            translated_types.insert(type, l_pt);
            return l_pt;
        }

        case AST_ARRAY_TYPE: {
            AST_ArrayType* at = (AST_ArrayType*)type;
            llvm::Type* l_base_type = get_llvm_type(at->base_type);

            llvm::ArrayType* l_at = llvm::ArrayType::get(l_base_type, at->array_length);

            translated_types.insert(type, l_at);
            return l_at;
        }

        default:
            NOT_IMPLEMENTED();
    }
}

void T2L_BlockContext::prepass() {
    for (auto& instr : tir_block->instructions) {
        if (instr.opcode & TOPC_MODIFIES_DST_BIT) {
            TIR_Value dst = instr.un.dst;
            values_to_modify[dst] = &instr;
        }
    }
}

llvm::Value* T2L_BlockContext::get_value_graph_recurse(TIR_Value tir_val, bool ask_your_parents) {

    T2L_Context* ctx = fn->t2l_context;

    bool will_modify = values_to_modify.find2(tir_val);

    if (will_modify && compiled) {
        return modified_values[tir_val];
    }
    
    else if (will_modify && !ask_your_parents) {
        return (llvm::Value*)this;
    }



    if (this->tir_block->previous_blocks.size == 1) {
        return fn->block_translation[this->tir_block->previous_blocks[0]]->get_value(tir_val);
    }

    llvm::Value *asdf = nullptr;

    if (modified_values.find(tir_val, &asdf))
        return asdf;

    llvm::Type *llvm_type = ctx->get_llvm_type(tir_val.type);

    auto ip = ctx->builder.GetInsertBlock();

    llvm::Instruction* new_ip = llvm_block->getFirstNonPHI();
    if (new_ip)
        ctx->builder.SetInsertPoint(new_ip);
    else
        ctx->builder.SetInsertPoint(llvm_block);

    llvm::PHINode *phi = ctx->builder.CreatePHI(llvm_type, this->tir_block->previous_blocks.size);
    ctx->builder.SetInsertPoint(ip);




    for (TIR_Block* tir_parent_block : this->tir_block->previous_blocks) {
        T2L_BlockContext* t2l_parent_block = fn->block_translation[tir_parent_block];

        llvm::Value* val = t2l_parent_block->get_value_graph_recurse(tir_val, false);
        if (val == (llvm::Value*)t2l_parent_block) {
            t2l_parent_block->promises[tir_val].push(phi);
        } else {
            phi->addIncoming(val, t2l_parent_block->llvm_block);
        }
    }

    modified_values[tir_val] = phi;
    return phi;
}

llvm::Value* T2L_BlockContext::get_value(TIR_Value tir_val) {
    T2L_Context* ctx = fn->t2l_context;

    switch (tir_val.valuespace) {
        case TVS_VALUE: {
            llvm::Type *l_type = ctx->translated_types[tir_val.type];
            assert(l_type);
            return ConstantInt::get(l_type, tir_val.offset, false);
        }
        case TVS_RET_VALUE:
        case TVS_TEMP: {

            llvm::Value **intermediate = modified_values.find2(tir_val);
            if (intermediate)
                return *intermediate;

            return get_value_graph_recurse(tir_val, true);
        }

        case TVS_GLOBAL: {
            llvm::Value* ptr_val = ctx->translated_globals[tir_val];
            return ptr_val;
        }

        case TVS_ARGUMENT: {
            return fn->llvm_fn->arg_begin() + tir_val.offset;
        }

        case TVS_STACK: {
            return fn->stack_pointers[tir_val];
        }

        default:
            NOT_IMPLEMENTED();
    }
}

void T2L_BlockContext::set_value(TIR_Instruction* tir_instr, llvm::Value* llvm_val) {
    TIR_Value dst = tir_instr->un.dst;

    modified_values[dst] = llvm_val;

    if (tir_instr == values_to_modify[dst]) { 
        arr<llvm::PHINode*>* p = promises.find2(dst);
        if (p) {
            for (llvm::PHINode* phi : *p) {
                phi->addIncoming(llvm_val, llvm_block);
            }
        }
    }
}

void T2L_BlockContext::compile() {
    auto& builder = fn->t2l_context->builder;

    builder.SetInsertPoint(llvm_block);

    for (auto& instr : tir_block->instructions) {
        switch (instr.opcode) {
            case TOPC_MOV: {
                llvm::Value *l_src = get_value(instr.un.src);

                set_value(&instr, l_src);
                break;
            }
            case TOPC_ADD: {
                llvm::Value *lhs = get_value(instr.bin.lhs);
                llvm::Value *rhs = get_value(instr.bin.rhs);
                set_value(&instr, builder.CreateAdd(lhs, rhs));
                break;
            }
            case TOPC_EQ: {
                llvm::Value *lhs = get_value(instr.bin.lhs);
                llvm::Value *rhs = get_value(instr.bin.rhs);
                set_value(&instr, builder.CreateICmpEQ(lhs, rhs));
                break;
            }
            case TOPC_LT: {
                llvm::Value *lhs = get_value(instr.bin.lhs);
                llvm::Value *rhs = get_value(instr.bin.rhs);
                set_value(&instr, builder.CreateICmpULT(lhs, rhs));
                break;
            }
            case TOPC_RET: {
                // TODO t_void
                if (((AST_FnType*)fn->tir_fn->ast_fn->type)->returntype != &t_void) {
                    llvm::Value *retval = get_value(fn->tir_fn->retval);
                    builder.CreateRet(retval);
                }
                else {
                    builder.CreateRetVoid();
                }
                break;
            }
            case TOPC_CALL: {
                AST_Fn* callee = (AST_Fn*)instr.call.fn;

                llvm::Function* l_callee = (llvm::Function*)fn->t2l_context->global_functions[callee]->llvm_fn;
                assert(l_callee); // TODO

                u32 argc = instr.call.args.size;

                llvm::Value** args = (llvm::Value**)malloc(sizeof(llvm::Value*) * argc);

                for (u32 i = 0; i < instr.call.args.size; i++) {
                    llvm::Value* l_arg = get_value(instr.call.args[i]);
                    assert(l_arg);
                    args[i] = l_arg;
                }

                llvm::Value* l_result = builder.CreateCall(l_callee, ArrayRef<llvm::Value*>(args, argc));

                if (instr.call.dst) {
                    set_value(&instr, l_result);
                }

                free(args);
                break;
            }
            case TOPC_STORE: {
                // TODO ERROR this will fail if the pointer is uninitialized
                // This is undefined behaviour anyway as we're deref'ing an
                // uninitialized pointer, the typer should probably catch this
                llvm::Value *l_dst_ptr = get_value(instr.un.dst);
                llvm::Value* l_src = get_value(instr.un.src);

                builder.CreateStore(l_src, l_dst_ptr, false);
                break;
            }
            case TOPC_LOAD: {
                llvm::Value *src = get_value(instr.un.src);
                llvm::Value* l_val = builder.CreateLoad(src);

                set_value(&instr, l_val);
                break;
            }
            case TOPC_JMP: {
                builder.CreateBr(fn->block_translation[instr.jmp.next_block]->llvm_block);
                break;
            }
            case TOPC_JMPIF: {
                llvm::Value *cond = get_value(instr.jmpif.cond);
                llvm::BasicBlock *true_b = fn->block_translation[instr.jmpif.then_block]->llvm_block;
                llvm::BasicBlock *false_b = fn->block_translation[instr.jmpif.else_block]->llvm_block;
                builder.CreateCondBr(cond, true_b, false_b);
                break;
            }
            case TOPC_PTR_OFFSET: {
                llvm::Value *lhs = get_value(instr.bin.lhs);

                llvm::Value* gep;

                arr<llvm::Value*> vals;

                for (TIR_Value v : instr.gep.offsets)
                    vals.push(get_value(v));

                gep = builder.CreateGEP(lhs, llvm::ArrayRef<llvm::Value*>(vals.buffer, vals.size));

                set_value(&instr, gep);
                break;
            }
            case TOPC_BITCAST: {
                llvm::Type* l_dest_type = fn->t2l_context->get_llvm_type(instr.un.dst.type);
                llvm::Value* l_src = get_value(instr.un.src);
                llvm::Value* val = builder.CreateBitCast(l_src, l_dest_type);
                set_value(&instr, val);
                break;
            }
            default:
                NOT_IMPLEMENTED();
        }
    }
    compiled = true;
}

void T2L_FunctionContext::compile_header() {
    llvm::FunctionType* l_fn_type = (llvm::FunctionType*)t2l_context->get_llvm_type(tir_fn->ast_fn->type);
    auto linkage = tir_fn->ast_fn->is_extern ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::WeakAnyLinkage;
    llvm_fn = Function::Create(l_fn_type, linkage, tir_fn->ast_fn->name, t2l_context->mod);
}

void T2L_FunctionContext::compile() {
    AST_Fn* ast_fn = tir_fn->ast_fn;

    const char* fn_name = ast_fn->name;

    for (TIR_Block* tir_block : tir_fn->blocks) {
        T2L_BlockContext* t2l_block = new T2L_BlockContext();
        t2l_block->fn = this;
        t2l_block->tir_block = tir_block;
        t2l_block->llvm_block = BasicBlock::Create(t2l_context->lc, "block", llvm_fn);

        blocks.push(t2l_block);
        block_translation[tir_block] = t2l_block;
    }

    if (blocks.size > 0) {
        // Allocate all the variables that MUST be on the stack
        auto& builder = t2l_context->builder;
        builder.SetInsertPoint(blocks[0]->llvm_block);

        arr<Context*> rem = { &ast_fn->block.ctx };

        for (auto& varval : tir_fn->stack) {

            if (varval.val.valuespace == TVS_STACK) {

                AST_PointerType *val_ptr_type = (AST_PointerType*)varval.val.type;

                AST_Type *inner_type = val_ptr_type->pointed_type;
                llvm::Type* llvm_type = t2l_context->get_llvm_type(inner_type);

                llvm::Value *llvm_var_pointer = builder.CreateAlloca(llvm_type);
                stack_pointers[varval.val] = llvm_var_pointer;
            }
        }
    }



    // We can't use an iterator here because we add elements to the array during the loop
    /*
    for (u32 index = 0; index < rem.size; index ++) {
        Context* current = rem[index];

        for (auto decl : current->declarations) {
            if (decl.value->nodetype == AST_VAR) {
                AST_Var* var = (AST_Var*)decl.value;

                if (var->always_on_stack) {
                    llvm::Type* llvm_type = t2l_context->get_llvm_type(var->type);

                    llvm::Value* llvm_var_pointer = builder.CreateAlloca(llvm_type);
                    blocks[0]->modified_values[var] = llvm_var_pointer;
                }
            }

            for (auto child : current->children)
                rem.push(child);
        }
    }
    */




    for (T2L_BlockContext* block : blocks)
        block->prepass();

    for (T2L_BlockContext* block : blocks)
        block->compile();
}

const char* T2L_Context::output_object() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    std::string Error;
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, Error);

    assert(target);
    
    llvm::TargetOptions opt;
    auto RM = Optional<Reloc::Model>();

    std::string cpu = "generic";
    std::string features = "";
    auto target_machine = target->createTargetMachine(target_triple, cpu, features, opt, RM);

    mod.setDataLayout(target_machine->createDataLayout());
    mod.setTargetTriple(target_triple);

    auto Filename = "output.o";
    std::error_code EC;
    raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);
    assert(!EC);

    char* error;


    const char* object_filename = ".ntrobject.o";
    if (output_type == OUTPUT_OBJECT_FILE)
        object_filename = output_file;

    LLVMTargetMachineEmitToFile((LLVMTargetMachineRef)target_machine, 
            (LLVMModuleRef)&mod, 
            (char*)object_filename,
            LLVMObjectFile, 
            &error);

    return object_filename;
}

// This creates a function called _entry
// that calls the main() function passed to it with NO arguments
// and then calls libc exit() with the value returned by main()
void insert_entry_boilerplate(T2L_Context& c, Function* l_main) {
    Type* l_void_type = Type::getVoidTy(c.lc);

    // NOTE we're assuming libc int == i32
    Type* l_exit_params[] = { c.translated_types[&t_i32] };

    FunctionType* l_exit_type     = FunctionType::get(l_void_type, ArrayRef<Type*>(l_exit_params, 1), false);
    FunctionType* l_entry_fn_Type = FunctionType::get(l_void_type, false);

    Function* l_exit     = Function::Create(l_exit_type,     GlobalValue::ExternalLinkage, "exit",   c.mod);
    Function* l_entry_fn = Function::Create(l_entry_fn_Type, GlobalValue::WeakAnyLinkage,  "_start", c.mod);

    BasicBlock* l_bb = BasicBlock::Create(c.lc, "entry", l_entry_fn);
    c.builder.SetInsertPoint(l_bb);

    Value* main_result = c.builder.CreateCall(l_main);

    c.builder.CreateCall(l_exit, ArrayRef<Value*>(&main_result, 1));
    c.builder.CreateRetVoid();
}

void T2L_Context::compile_all() {
    llvm::Function* llvm_main_fn;

    // TODO this has to go
    // LLVM should only read the TIR, there's no sane reason for it to touch the AST values
    // The reason we do this right now is because TIR values are shit and have to be redone
    // Initialize the global variables
    for (auto& kvp : tir_context.valmap) {
        AST_Value* ast_value = kvp.key;
        TIR_Value tir_value = kvp.value;

        assert (ast_value->nodetype == AST_VAR);

        switch (ast_value->nodetype) {
            case AST_VAR: {
                AST_Var* var = (AST_Var*)ast_value;

                if (var->initial_value) {
                    switch (var->initial_value->nodetype) {
                        case AST_STRING_LITERAL: {
                            AST_StringLiteral* str = (AST_StringLiteral*)var->initial_value;

                            llvm::Constant* l_val = llvm::ConstantDataArray::getString(lc, str->str, true);

                            auto l_var = new llvm::GlobalVariable( mod, l_val->getType(), true, GlobalValue::WeakAnyLinkage, l_val, "");

                            translated_globals[tir_value] = l_var;
                            break;
                        }
                        default:
                            assert(!"Not supported");
                    }
                }
                else {
                    llvm::Type* l_type = get_llvm_type(var->type);

                    auto l_var = new llvm::GlobalVariable( mod, l_type, false, GlobalValue::WeakAnyLinkage, llvm::Constant::getNullValue(l_type), "");
                    translated_globals[tir_value] = l_var;
                }

                break;
            }

            case AST_FN: {
                continue;
            }

            default:
                NOT_IMPLEMENTED();
        }

    }

    // Compile the signatures for all global functions so we can call them
    for (auto& kvp : tir_context.fns) {
        T2L_FunctionContext* llvmfnctx = new T2L_FunctionContext();
        llvmfnctx->t2l_context = this;
        llvmfnctx->tir_fn = kvp.value;
        llvmfnctx->compile_header();

        this->global_functions[kvp.key] = llvmfnctx;
    }

    // Compile the bodies for all global functions
    for (auto& kvp : global_functions) {
        kvp.value->compile();

        const char* fn_name = kvp.value->tir_fn->ast_fn->name;
        if (!strcmp(fn_name, "main"))
            llvm_main_fn = kvp.value->llvm_fn;
    }


    insert_entry_boilerplate(*this, llvm_main_fn);

    ModuleAnalysisManager mam;

    if (debug_output) {
        auto p = PrintModulePass();
        p.run(mod, mam);
    }

    mam.registerPass([]() { return VerifierAnalysis(); });
    mam.registerPass([]() { return PassInstrumentationAnalysis(); });
    auto v = VerifierPass();
    v.run(mod, mam);
}
