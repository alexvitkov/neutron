#ifndef LLVM_H
#define LLVM_H

#include "../../common.h"
#include "../../ast.h"
#include "../../typer.h"
#include "../../cmdargs.h"
#include "../tir/tir.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm-c/TargetMachine.h>

struct LLVM_Context {
    TIR_Context& t_c;

    llvm::LLVMContext lc;
    llvm::Module mod;

    map<AST_Type*, llvm::Type*> translated_types;

    // map<TIR_Value*, arr<llvm::Value*>> values;
    map<AST_Value*, llvm::Value*> definitions;
    map<TIR_Block*, llvm::BasicBlock*> blocks;

    struct Tuple {
        llvm::Value* val;
        llvm::BasicBlock* block;
    };
    map<TIR_Value*, map<llvm::BasicBlock*, llvm::Value*>> _values;

    map<TIR_Value*, llvm::Value*> translated_globals;

    llvm::Value* retval;

    llvm::IRBuilder<> builder;

    LLVM_Context(TIR_Context& t_c);
    void compile_all();
    const char* output_object();

    void compile_fn_header(TIR_Function* fn);
    void compile_fn(TIR_Function* fn);
    void compile_block(TIR_Function* fn, llvm::Function* l_fn, TIR_Block* block);

    llvm::Value* translate_value(TIR_Value* val, TIR_Block* block);
    llvm::Type* translate_type(AST_Type* type);
    void set_value(TIR_Value* value, llvm::Value* l_value, llvm::BasicBlock* l_bb);
};

#endif // guard
