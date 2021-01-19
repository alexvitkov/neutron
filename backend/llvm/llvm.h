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

struct T2L_Context;
struct T2L_FunctionContext;
struct T2L_BlockContext;

struct T2L_Context {
    TIR_Context& tir_context;

    llvm::LLVMContext lc;
    llvm::Module mod;

    map<AST_Type*, llvm::Type*> translated_types;
    map<TIR_Value*, llvm::Value*> translated_globals;
    map<AST_Fn*, T2L_FunctionContext*> global_functions;

    llvm::IRBuilder<> builder;

    T2L_Context(TIR_Context& t_c);
    void compile_all();
    const char* output_object();

    llvm::Type* get_llvm_type(AST_Type* type);
};

struct T2L_FunctionContext {
    T2L_Context    *t2l_context;
    TIR_Function   *tir_fn;
    llvm::Function *llvm_fn;

    arr<T2L_BlockContext*> blocks;

    map<TIR_Block*, T2L_BlockContext*> block_translation;

    void compile_header();
    void compile();

    llvm::Value* retval;
};

struct T2L_BlockContext {
    T2L_FunctionContext* fn;
    bool compiled;

    TIR_Block *tir_block;
    llvm::BasicBlock *llvm_block;

    map<TIR_Value*, llvm::Value*> modified_values;
    map<TIR_Value*, TIR_Instruction*> values_to_modify;

    struct Promise {
        llvm::PHINode *phi;
        llvm::BasicBlock *llvm_block;
    };
    map<TIR_Value*, arr<llvm::PHINode*>> promises;

    void set_value(TIR_Instruction* tir_instr, llvm::Value* l_val);
    llvm::Value* get_value(TIR_Value* t_val);


    struct ParentBlockValue {
        T2L_BlockContext *source, *promise;
        llvm::Value *value;
    };
    llvm::Value* get_value_graph_recurse(TIR_Value* tir_val, bool ask_parents);

    void prepass();
    void compile();
};

#endif // guard
