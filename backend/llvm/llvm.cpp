#include "llvm.h"
#include <iostream>

using namespace llvm;

void llvm_main() {

    LLVMContext context;

    Module mod("neutron_module", context);

    IntegerType* i32 = IntegerType::get(context, 32);

    Type* i32_args[3] = { i32, i32, i32 };

    FunctionType* ft = FunctionType::get(i32, ArrayRef<Type*>(i32_args, 3), false);

    Function* mul_add = Function::Create(ft, llvm::GlobalValue::CommonLinkage, "mul_add", mod);

    Function::arg_iterator args = mul_add->arg_begin();
    Value* x = args++;
    x->setName("x");
    Value* y = args++;
    y->setName("y");
    Value* z = args++;
    z->setName("z");

    IRBuilder<> builder(context);
    BasicBlock* bb = BasicBlock::Create(context, "entry", mul_add);
    builder.SetInsertPoint(bb);

    Value* v = builder.CreateMul(x, y);
    builder.CreateRet( builder.CreateAdd(v, z) );


    ModuleAnalysisManager mam;

    auto p = PrintModulePass();
    p.run(mod, mam);



}
