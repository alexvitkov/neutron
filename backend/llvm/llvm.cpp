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

llvm::Constant *get_constant(T2L_Context *ctx, TIR_Value tir_val) {
    switch (tir_val.valuespace) {
        case TVS_VALUE: {
            llvm::Type *l_type = ctx->translated_types[tir_val.type];
            assert(l_type);
            return ConstantInt::get(l_type, tir_val.offset, false);
        }
        case TVS_C_STRING_LITERAL: {
            llvm::Constant* l_val = llvm::ConstantDataArray::getString(ctx->lc, (const char*)tir_val.offset, true); // POINTERSIZE
            auto l_var = new llvm::GlobalVariable(ctx->mod, l_val->getType(), true, GlobalValue::WeakAnyLinkage, l_val, "");
            return l_var;
        }
        default:
            return nullptr;
    }
}

llvm::FunctionType *T2L_Context::get_function_type(TIR_Function* fn) {
    arr<llvm::Type*> l_param_types;
    AST_FnType *fntype = fn->ast_fn->fntype();

    llvm::Type *llvm_fntype;
    if (translated_types.find(fntype, &llvm_fntype)) {
        return (llvm::FunctionType*)llvm_fntype;
    }

    for (auto& param : fn->parameters) {
        l_param_types.push(get_llvm_type(param.type));
    }

    llvm::Type* returntype = returntype = get_llvm_type(fn->retval.type);

    FunctionType* l_fn_type = FunctionType::get(
            returntype, 
            ArrayRef<llvm::Type*>(l_param_types.buffer, l_param_types.size),
            fntype->is_variadic);

    translated_types.insert(fntype, l_fn_type);
    return l_fn_type;
}

llvm::Type *T2L_Context::get_llvm_type(AST_Type* type) {
    llvm::Type* t;
    if (translated_types.find(type, &t))
        return t;

    switch (type->nodetype) {
        case AST_FN_TYPE: {
            assert(!"NOT SUPPORTED");
            UNREACHABLE;
        }

        case AST_POINTER_TYPE: {
            AST_PointerType* pt = (AST_PointerType*)type;
            llvm::Type* l_pointed_type = get_llvm_type(pt->pointed_type);
            llvm::Type* l_pt = PointerType::get(l_pointed_type, 0);

            translated_types.insert(type, l_pt);
            return l_pt;
        }

        case AST_ARRAY_TYPE: {
            AST_ArrayType *at = (AST_ArrayType*)type;
            llvm::Type* l_base_type = get_llvm_type(at->base_type);

            llvm::ArrayType *llvm_array_type = llvm::ArrayType::get(l_base_type, at->array_length);

            translated_types.insert(type, llvm_array_type);
            return llvm_array_type;
        }

        case AST_STRUCT: {
            AST_Struct *st = (AST_Struct*)type;
            arr<llvm::Type*> elements;

            for (auto& member : st->members) {
                elements.push(get_llvm_type(member.type));
            }

            llvm::ArrayRef<llvm::Type*> ref(elements.begin(), elements.end());

            llvm::StructType *llvm_struct = llvm::StructType::create(lc, ref);
            translated_types.insert(type, llvm_struct);
            return llvm_struct;
        }

        default:
            NOT_IMPLEMENTED();
    }
}

void T2L_BlockContext::prepass() {
    for (auto& instr : tir_block->instructions) {
        if (instr.opcode & TOPC_MODIFIES_DST_BIT) {
            TIR_Value dst = instr.un.dst; values_to_modify[dst] = &instr;
        }
    }
}

/*
// Those are only used for debug output for the get_value_graph_recurse
int indenti = 0;
void print_indent() {
    for (int i = 0; i < indenti; i++)
        wcout << "    ";
}
*/

llvm::Value* T2L_BlockContext::get_value_graph_recurse(TIR_Value tir_val, arr<T2L_BlockContext *>& visited, bool return_intermediate_values) {

    T2L_Context* ctx = fn->t2l_context;


    // If somewhere down the block we modify the value, this will be set to true
    // For values that we modify, we don't recurse upwards, but instead return
    // the most recent modified value
    if (values_to_modify.find2(tir_val)) {

        llvm::Value *value;
        if (modified_values.find(tir_val, &value) && (compiled || return_intermediate_values))
            return value;

        // This is a special magic value - it signifies that we haven't yet
        // been compiled, but we pinky promise to provide a value for a PHI node later on

        if (!return_intermediate_values) {
            // print_indent(); wcout << this->tir_block->id << " returned a promise\n";
            return (llvm::Value*)this;
        }
    }

    if (visited.contains(this)) {
        // print_indent(); wcout << "Already visited " << this->tir_block->id << "\n";
        llvm::Value *value;
        if (modified_values.find(tir_val, &value)) {
            return value;
        }

        // nullptr is a magic value that means the node has already been visited
        return nullptr;

    }
    visited.push(this);
    
    // If there are no parents, this is the entry block of a function
    // so the values for arguments are given to us directly from above
    if (tir_val.valuespace == TVS_ARGUMENT && this->tir_block->previous_blocks.size == 0) {
        // print_indent(); wcout << this->tir_block->id << " returned a raw argument\n";
        return fn->llvm_fn->arg_begin() + tir_val.offset;
    }


    struct BlockValuePair {
        T2L_BlockContext *block;
        llvm::Value      *value;
    };
    arr<BlockValuePair> values;

    for (TIR_Block *tir_parent_block : this->tir_block->previous_blocks) {
        T2L_BlockContext *t2l_parent_block = fn->block_translation[tir_parent_block];

        // print_indent(); wcout << this->tir_block->id << " calling parent " << t2l_parent_block->tir_block->id << "\n";
        // indenti++;
        llvm::Value* val = t2l_parent_block->get_value_graph_recurse(tir_val, visited, false);
        // indenti--;
        if (val)
            values.push({ t2l_parent_block, val });
    }

    if (values.size == 0) {
        return nullptr;
    }

    if (values.size == 1) {
        // If we only have a single value, then it should never be a promise
        // because we compile blocks in a certain order - child after parent
        // If in the future this has to be changed, promises can be extended
        // to handle this case, right now they only push to PHI nodes
        assert((void*)values[0].value != (void*)values[0].block);
        modified_values[tir_val] = values[0].value;
        // print_indent(); wcout << this->tir_block->id << " returned a single value\n";
        return values[0].value;
    }

    {
        // If we have multiple values, create a PHI node
        llvm::Type *llvm_type = ctx->get_llvm_type(tir_val.type);
        auto ip = ctx->builder.GetInsertBlock();

        llvm::Instruction* new_ip = llvm_block->getFirstNonPHI();
        if (new_ip)
            ctx->builder.SetInsertPoint(new_ip);
        else
            ctx->builder.SetInsertPoint(llvm_block);

        llvm::PHINode *phi = ctx->builder.CreatePHI(llvm_type, this->tir_block->previous_blocks.size);
        ctx->builder.SetInsertPoint(ip);

        for (BlockValuePair &bvp : values) {
            if ((void*)bvp.value == (void*)bvp.block) {
                bvp.block->promises[tir_val].push(phi);
            } else {
                phi->addIncoming(bvp.value, bvp.block->llvm_block);
            }
        }
        
        modified_values[tir_val] = phi;
        // print_indent(); wcout << this->tir_block->id << " returned a PHI node with " << values.size << " values\n";
        return phi;
    }
}


llvm::Value* T2L_BlockContext::get_value(TIR_Value tir_val) {
    T2L_Context* ctx = fn->t2l_context;

    llvm::Constant *constant = get_constant(fn->t2l_context, tir_val);
    if (constant)
        return constant;

    switch (tir_val.valuespace) {
        case TVS_RET_VALUE:
        case TVS_ARGUMENT:
        case TVS_TEMP: 
        {

            llvm::Value **intermediate = modified_values.find2(tir_val);
            if (intermediate)
                return *intermediate;

            arr<T2L_BlockContext*> visited;
            return get_value_graph_recurse(tir_val, visited, true);
        }

        case TVS_GLOBAL: {
            llvm::Value* ptr_val = ctx->translated_globals[tir_val];
            return ptr_val;
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

        if ((instr.opcode & TOPC_BINARY) == TOPC_BINARY) {
            llvm::Value *lhs = get_value(instr.bin.lhs);
            llvm::Value *rhs = get_value(instr.bin.rhs);

            switch (instr.opcode) {
                case TOPC_SHL:
                    set_value(&instr, builder.CreateShl(lhs, rhs));
                    break;
                case TOPC_SHR:
                    set_value(&instr, builder.CreateLShr(lhs, rhs));
                    break;

                case TOPC_UADD:
                    set_value(&instr, builder.CreateAdd(lhs, rhs));
                    break;
                case TOPC_USUB:
                    set_value(&instr, builder.CreateSub(lhs, rhs));
                    break;
                case TOPC_UMUL:
                    set_value(&instr, builder.CreateMul(lhs, rhs));
                    break;
                case TOPC_UDIV:
                    set_value(&instr, builder.CreateUDiv(lhs, rhs));
                    break;
                case TOPC_UMOD:
                    set_value(&instr, builder.CreateURem(lhs, rhs));
                    break;
                case TOPC_UEQ :
                    set_value(&instr, builder.CreateICmpEQ(lhs, rhs));
                    break;
                case TOPC_ULT:
                    set_value(&instr, builder.CreateICmpULT(lhs, rhs));
                    break;
                case TOPC_ULTE:
                    set_value(&instr, builder.CreateICmpULE(lhs, rhs));
                    break;
                case TOPC_UGT:
                    set_value(&instr, builder.CreateICmpUGT(lhs, rhs));
                    break;
                case TOPC_UGTE:
                    set_value(&instr, builder.CreateICmpUGE(lhs, rhs));
                    break;

                case TOPC_FADD:
                    set_value(&instr, builder.CreateFAdd(lhs, rhs));
                    break;
                case TOPC_FSUB:
                    set_value(&instr, builder.CreateFSub(lhs, rhs));
                    break;
                case TOPC_FMUL:
                    set_value(&instr, builder.CreateFMul(lhs, rhs));
                    break;
                case TOPC_FDIV:
                    set_value(&instr, builder.CreateFDiv(lhs, rhs));
                    break;
                case TOPC_FMOD:
                    set_value(&instr, builder.CreateFRem(lhs, rhs));
                    break;
                case TOPC_FEQ:
                    set_value(&instr, builder.CreateFCmpUEQ(lhs, rhs));
                    break;
                case TOPC_FLT:
                    set_value(&instr, builder.CreateFCmpULT(lhs, rhs));
                    break;
                case TOPC_FLTE:
                    set_value(&instr, builder.CreateFCmpULE(lhs, rhs));
                    break;
                case TOPC_FGT :
                    set_value(&instr, builder.CreateFCmpUGT(lhs, rhs));
                    break;
                case TOPC_FGTE:
                    set_value(&instr, builder.CreateFCmpUGE(lhs, rhs));
                    break;
                    
                // TODO SADD = UADD, same for SUB/MUL/EQ
                case TOPC_SADD:
                    set_value(&instr, builder.CreateAdd(lhs, rhs));
                    break;
                case TOPC_SSUB:
                    set_value(&instr, builder.CreateSub(lhs, rhs));
                    break;
                case TOPC_SMUL:
                    set_value(&instr, builder.CreateMul(lhs, rhs));
                    break;
                case TOPC_SDIV:
                    set_value(&instr, builder.CreateSDiv(lhs, rhs));
                    break;
                case TOPC_SMOD:
                    set_value(&instr, builder.CreateSRem(lhs, rhs));
                    break;
                case TOPC_SEQ:
                    set_value(&instr, builder.CreateICmpEQ(lhs, rhs));
                    break;
                case TOPC_SLT:
                    set_value(&instr, builder.CreateICmpSLT(lhs, rhs));
                    break;
                case TOPC_SLTE:
                    set_value(&instr, builder.CreateICmpSLE(lhs, rhs));
                    break;
                case TOPC_SGT:
                    set_value(&instr, builder.CreateICmpSGT(lhs, rhs));
                    break;
                case TOPC_SGTE:
                    set_value(&instr, builder.CreateICmpSGE(lhs, rhs));
                    break;

                default:
                    UNREACHABLE;
            }
        } else {
            switch (instr.opcode) {
                case TOPC_MOV: {
                    llvm::Value *l_src = get_value(instr.un.src);
                    set_value(&instr, l_src);
                    break;
                }
                case TOPC_RET: {
                    if (fn->tir_fn->retval) {
                        llvm::Value *retval = get_value(fn->tir_fn->retval);
                        builder.CreateRet(retval);
                    } else {
                        builder.CreateRetVoid();
                    }
                    break;
                }
                case TOPC_CALL: {
                    AST_Fn* callee = instr.call.fn->ast_fn;

                    llvm::Function* l_callee = (llvm::Function*)fn->t2l_context->global_functions[callee]->llvm_fn;
                    assert(l_callee); // TODO

                    u32 argc = instr.call.args.size;

                    llvm::Value** args = (llvm::Value**)malloc(sizeof(llvm::Value*) * argc);

                    for (u32 i = 0; i < instr.call.args.size; i++) {
                        llvm::Value* l_arg = get_value(instr.call.args[i]);
                        assert(l_arg);
                        args[i] = l_arg;
                    }

                    llvm::CallInst* l_result = builder.CreateCall(l_callee, ArrayRef<llvm::Value*>(args, argc));

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
                case TOPC_GEP: {
                    llvm::Value *lhs = get_value(instr.gep.base);

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
    }
    compiled = true;
}

void T2L_FunctionContext::compile_header() {
    llvm::FunctionType* l_fn_type = t2l_context->get_function_type(tir_fn);

    auto linkage = tir_fn->ast_fn->is_extern ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::WeakAnyLinkage;
    llvm_fn = Function::Create(l_fn_type, linkage, tir_fn->ast_fn->name, t2l_context->mod);

    for (u32 i = 0; i < tir_fn->parameters.size; i++)
        if (tir_fn->parameters[i].flags & TVF_BYVAL) {

            AST_PointerType *pt = (AST_PointerType*)tir_fn->parameters[i].type;
            llvm::Type *t = t2l_context->get_llvm_type(pt->pointed_type);

            auto llvm_byval = llvm::Attribute::get(
                    this->t2l_context->lc, 
                    llvm::Attribute::ByVal, 
                    t);
            llvm_fn->addParamAttr(i, llvm_byval);
        }
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

        arr<AST_Context*> rem = { &ast_fn->block };

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
            if (decl.value IS AST_VAR) {
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

    // TODO we're assuming libc int == i32
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

void T2L_Context::compile_all(Job *after) {
    llvm::Function* llvm_main_fn;

    for (auto &kvp : tir_context.global_valmap) {
        AST_Var *var = (AST_Var*)kvp.key;
        if (!(var IS AST_VAR))
            continue;

        if (var->is_constant)
            continue;

        llvm::Type* l_type = get_llvm_type(var->type);

        TIR_Value initial_value;
        llvm::Constant *llvm_initial_value;

        if (tir_context._global_initial_values.find(kvp.value.offset, &initial_value)) {
            llvm_initial_value = get_constant(this, initial_value);
        } else {
            llvm_initial_value = llvm::Constant::getNullValue(l_type);
        }

        auto l_var = new llvm::GlobalVariable(mod, 
                l_type, 
                false, 
                GlobalValue::WeakAnyLinkage, 
                llvm_initial_value, 
                "");

        translated_globals[kvp.value] = l_var;
    }

    // TODO this has to go
    // LLVM should only read the TIR, there's no sane reason for it to touch the AST values
    // The reason we do this right now is because TIR values are shit and have to be redone
    // Initialize the global variables
    /*
    for (auto& kvp : tir_context.valmap) {
        AST_Value* ast_value = kvp.key;
        TIR_Value tir_value = kvp.value;

        assert (ast_value IS AST_VAR);

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
    */

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

    if (print_llvm) {
        auto p = PrintModulePass();
        p.run(mod, mam);
    }

    mam.registerPass([]() { return VerifierAnalysis(); });
    mam.registerPass([]() { return PassInstrumentationAnalysis(); });
    auto v = VerifierPass();
    v.run(mod, mam);
}
