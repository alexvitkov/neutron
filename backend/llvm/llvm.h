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

    // TODO this will not work for phi nodes
    map<TIR_Value*, llvm::Value*> values;

    map<AST_Value*, llvm::Value*> definitions;

    llvm::Value* retval;

    llvm::IRBuilder<> builder;

    LLVM_Context(TIR_Context& t_c);
    llvm::Function* compile_fn(TIR_Function* fn);
    void compile_all();
    const char* output_object();
    llvm::Type* translate_type(AST_Type* type);
};

#endif // guard
