#include "llvm.h"
#include <iostream>

using namespace llvm;

LLVM_Context::LLVM_Context(TIR_Context& tirc) : t_c(tirc), mod("ntrmod", lc), builder(lc) {
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
}

void LLVM_Context::set_value(TIR_Value* value, llvm::Value* l_value, llvm::BasicBlock* l_bb) {
     _values[value][l_bb] = l_value;
}

llvm::Type* LLVM_Context::translate_type(AST_Type* type) {
    llvm::Type* t;
    if (translated_types.find(type, &t))
        return t;

    switch (type->nodetype) {
        case AST_FN_TYPE: {
            AST_FnType* fntype = (AST_FnType*)type;

            arr<llvm::Type*> l_param_types;

            for (auto& param : fntype->param_types)
                l_param_types.push(translate_type(param));

            llvm::Type* returntype;
            if (!fntype->returntype || fntype->returntype == &t_void) {
                returntype = translated_types[&t_void];
            }
            else {
                returntype = translate_type(fntype->returntype);
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
            llvm::Type* l_pointed_type = translate_type(pt->pointed_type);
            llvm::Type* l_pt = PointerType::get(l_pointed_type, 0);

            translated_types.insert(type, l_pt);
            return l_pt;
        }

        case AST_ARRAY_TYPE: {
            AST_ArrayType* at = (AST_ArrayType*)type;
            llvm::Type* l_base_type = translate_type(at->base_type);

            llvm::ArrayType* l_at = llvm::ArrayType::get(l_base_type, at->array_length);

            translated_types.insert(type, l_at);
            return l_at;
        }

        default:
            assert(!"Not implemented");
    }
}

llvm::Value* LLVM_Context::translate_value(TIR_Value* val, TIR_Block* block) {
    switch (val->valuespace) {
        case TVS_VALUE: {
            llvm::Type* l_type = translated_types[val->type];
            assert(l_type);
            return ConstantInt::get(l_type, val->offset, false);
        }
        case TVS_RET_VALUE:
        case TVS_TEMP: {
            BasicBlock* l_bb = blocks[block];

            llvm::Value* out;
            if (_values[val].find(l_bb, &out)) {
                return out;
            }

            arr<llvm::Value*> prev;

            for (TIR_Block* p : block->previous_blocks) {
                llvm::Value* v = _values[val][blocks[p]];
                assert(v);
                prev.push(v);
            }

            // TODO this should be handled in the typer?
            assert(prev.size > 0);

            if (prev.size == 1) {
                _values[val][l_bb] = prev[0];
                return prev[0];
            } else {
                llvm::PHINode* phi = builder.CreatePHI(translate_type(val->type), prev.size);

                for (int i = 0; i < prev.size; i++) {
                    phi->addIncoming(prev[i], blocks[block->previous_blocks[i]]);
                }
                _values[val][l_bb] = phi;

                return phi;
            }
        }
        case TVS_GLOBAL: {
            return translated_globals[val];
        }
        default:
            assert(!"Not implemented");
    }
}


void LLVM_Context::compile_block(TIR_Function* fn, llvm::Function* l_fn, TIR_Block* block) {
    BasicBlock* l_bb = blocks[block];
    blocks[block] = l_bb;

    builder.SetInsertPoint(l_bb);

    for (auto& instr : block->instructions) {
        switch (instr.opcode) {
            case TOPC_MOV: {
                llvm::Value* l_src = translate_value(instr.un.src, block);

                set_value(instr.un.dst, l_src, l_bb);
                break;
            }
            case TOPC_ADD: {
                llvm::Value *lhs = translate_value(instr.bin.lhs, block);
                llvm::Value *rhs = translate_value(instr.bin.rhs, block);
                set_value(instr.bin.dst, builder.CreateAdd(lhs, rhs), l_bb);
                break;
            }
            case TOPC_EQ: {
                llvm::Value *lhs = translate_value(instr.bin.lhs, block);
                llvm::Value *rhs = translate_value(instr.bin.rhs, block);
                set_value(instr.bin.dst, builder.CreateICmpEQ(lhs, rhs), l_bb);
                break;
            }
            case TOPC_RET: {
                llvm::Value *retval = translate_value(&fn->retval, block);
                builder.CreateRet(retval);
                break;
            }
            case TOPC_CALL: {
                AST_Fn* callee = (AST_Fn*)instr.call.fn;

                llvm::Function* l_callee = (llvm::Function*)definitions[callee];
                assert(l_callee); // TODO

                u32 argc = instr.call.args.size;

                llvm::Value** args = (llvm::Value**)malloc(sizeof(llvm::Value*) * argc);

                for (int i = 0; i < instr.call.args.size; i++) {
                    llvm::Value* l_arg = translate_value(instr.call.args[i], block);
                    assert(l_arg);
                    args[i] = l_arg;
                }

                llvm::Value* l_result = builder.CreateCall(l_callee, ArrayRef<llvm::Value*>(args, argc));

                if (instr.call.dst) {
                    set_value(instr.un.dst, l_result, l_bb);
                }

                free(args);
                break;
            }
            case TOPC_STORE: {
                // TODO ERROR this will fail if the pointer is uninitialized
                // This is undefined behaviour anyway as we're deref'ing an
                // uninitialized pointer, the typer should probably catch this
                llvm::Value *l_dst_ptr = translate_value(instr.un.dst, block);
                llvm::Value* l_src = translate_value(instr.un.src, block);

                builder.CreateStore(l_src, l_dst_ptr, false);
                break;
            }
            case TOPC_LOAD: {
                llvm::Value *src = translate_value(instr.un.src, block);
                llvm::Value* l_val = builder.CreateLoad(src);

                set_value(instr.un.dst, l_val, l_bb);
                break;
            }
            case TOPC_JMP: {
                builder.CreateBr(blocks[instr.jmp.next_block]);
                break;
            }
            case TOPC_JMPIF: {
                llvm::Value *cond = translate_value(instr.jmpif.cond, block);
                llvm::BasicBlock *true_b = blocks[instr.jmpif.then_block];
                llvm::BasicBlock *false_b = blocks[instr.jmpif.else_block];
                builder.CreateCondBr(cond, true_b, false_b);
                break;
            }
            case TOPC_PTR_OFFSET: {
                llvm::Value *lhs = translate_value(instr.bin.lhs, block);
                llvm::Value *rhs = translate_value(instr.bin.rhs, block);

                llvm::Value* gep;

                if (instr.bin.lhs->type->nodetype == AST_ARRAY_TYPE) {
                    llvm::Value* vals[2];
                    vals[0] = Constant::getNullValue(Type::getInt32Ty(lc));
                    vals[1] = rhs;
                    gep = builder.CreateGEP(lhs, vals);
                }
                else {
                    gep = builder.CreateGEP(lhs, rhs);
                }

                set_value(instr.bin.dst, gep, l_bb);
                break;
            }
            default:
                assert(!"Not implemented");
        }
    }
}

void LLVM_Context::compile_fn_header(TIR_Function* fn) {
    llvm::FunctionType* l_fn_type = (llvm::FunctionType*)translate_type(fn->ast_fn->type);

    if (fn->ast_fn->is_extern) {
        llvm::Function* l_fn = Function::Create(l_fn_type, llvm::GlobalValue::ExternalLinkage, fn->ast_fn->name, mod);
        definitions.insert(fn->ast_fn, l_fn);
    } else {
        Function* l_fn = Function::Create(l_fn_type, llvm::GlobalValue::WeakAnyLinkage, fn->ast_fn->name, mod);
        definitions.insert(fn->ast_fn, l_fn);
    }
}

void LLVM_Context::compile_fn(TIR_Function* fn) {
    const char* fn_name = fn->ast_fn->name;
    Function* l_fn = (Function*)definitions[fn->ast_fn];

    definitions.insert(fn->ast_fn, l_fn);

    for (TIR_Block* block : fn->blocks) {
        blocks.insert(block, BasicBlock::Create(lc, "block", l_fn));
    }
    for (TIR_Block* block : fn->blocks) {
        compile_block(fn, l_fn, block);
    }
}

const char* LLVM_Context::output_object() {
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
void insert_entry_boilerplate(LLVM_Context& c, Function* l_main) {
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

void LLVM_Context::compile_all() {
    Function* l_main;

    for (auto& x : t_c.valmap) {
        AST_Value* ast = x.key;
        TIR_Value* tirv = x.value;

        assert (ast->nodetype == AST_VAR);
        AST_Var* var = (AST_Var*)ast;

        assert(var->initial_value && var->initial_value->nodetype == AST_STRING_LITERAL);

        AST_StringLiteral* str = (AST_StringLiteral*)var->initial_value;

        llvm::Constant* l_val = llvm::ConstantDataArray::getString(lc, str->str, true);

        auto l_var = new llvm::GlobalVariable(
                mod,
                l_val->getType(), 
                true, 
                GlobalValue::WeakAnyLinkage,
                l_val,
                "");

        translated_globals[tirv] = l_var;
    }


    for (auto& kvp : t_c.fns) {
        compile_fn_header(kvp.value);
    }

    for (auto& kvp : t_c.fns) {
        TIR_Function* t_fn = kvp.value;
        compile_fn(t_fn);
        if (!strcmp(t_fn->ast_fn->name, "main"))
            l_main = (Function*)definitions[t_fn->ast_fn];
    }

    insert_entry_boilerplate(*this, l_main);

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
