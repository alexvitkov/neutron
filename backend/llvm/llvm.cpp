#include "llvm.h"
#include <iostream>

using namespace llvm;

LLVM_Context::LLVM_Context(TIR_Context& tirc) : t_c(tirc), mod("ntrmod", lc), builder(lc) {
    types.insert(&t_bool, IntegerType::get(lc, 1));
    types.insert(&t_u8,   IntegerType::get(lc, 8));
    types.insert(&t_u16,  IntegerType::get(lc, 16));
    types.insert(&t_u32,  IntegerType::get(lc, 32));
    types.insert(&t_u64,  IntegerType::get(lc, 64));
    types.insert(&t_i8,   IntegerType::get(lc, 8));
    types.insert(&t_i16,  IntegerType::get(lc, 16));
    types.insert(&t_i32,  IntegerType::get(lc, 32));
    types.insert(&t_i64,  IntegerType::get(lc, 64));
    types.insert(&t_f32,  Type::getFloatTy(lc));
    types.insert(&t_f64,  Type::getFloatTy(lc));
}

void tir_to_llvm_val(LLVM_Context& c, TIR_Value* val, llvm::Value** out) {
    switch (val->valuespace) {
        case TVS_VALUE: {
            llvm::Type* l_type = c.types[val->type];
            assert(l_type);
            *out = ConstantInt::get(l_type, val->offset, false);
            break;
        }
        case TVS_RET_VALUE:
        case TVS_TEMP: {
            llvm::Value* l_value = c.values[val];
            assert(l_value);
            *out = l_value;
            break;
        }
        default:
            assert(!"Not implemented");
    }
    /*
    TVS_DISCARD = 0x00,
    TVS_ARGUMENT,
    TVS_NEXTARGUMENT,
    */
}

Function* LLVM_Context::compile_fn(TIR_Function* fn) {

    FunctionType* l_fn_type = FunctionType::get(types[&t_i32], false);

    /*
    Function::arg_iterator args = mul_add->arg_begin();
    Value* x = args++;
    x->setName("x");
    Value* y = args++;
    y->setName("y");
    Value* z = args++;
    z->setName("z");
    */

    Function* l_fn = Function::Create(l_fn_type, llvm::GlobalValue::WeakAnyLinkage, "main", mod);

    BasicBlock* l_bb = BasicBlock::Create(lc, "entry", l_fn);

    builder.SetInsertPoint(l_bb);

    for (auto& instr : fn->entry->instructions) {
        switch (instr.opcode) {
            case TOPC_MOV: {
                llvm::Value* l_src;
                tir_to_llvm_val(*this, instr.un.src, &l_src);

                values[instr.un.dst] = l_src;
                break;
            }
            case TOPC_ADD: {
                llvm::Value *lhs, *rhs;
                tir_to_llvm_val(*this, instr.bin.lhs, &lhs);
                tir_to_llvm_val(*this, instr.bin.rhs, &rhs);
                values[instr.bin.dst] = builder.CreateAdd(lhs, rhs);
                break;
            }
            case TOPC_RET: {
                llvm::Value *retval;
                tir_to_llvm_val(*this, &fn->retval, &retval);
                builder.CreateRet(retval); break;
            }
            default:
                assert(!"Not implemented");
        }
    }

    return l_fn;
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
    Type* l_exit_params[] = { c.types[&t_i32] };

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

    for (auto& kvp : t_c.fns) {
        TIR_Function* t_fn = kvp.value;
        l_main = compile_fn(t_fn);
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
