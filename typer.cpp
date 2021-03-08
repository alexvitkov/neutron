#include "typer.h"
#include "ast.h"
#include "error.h"
#include "cast.h"
#include "resolve.h"

#include <sstream>

AST_PrimitiveType t_bool (PRIMITIVE_BOOL,     1, "bool");
AST_PrimitiveType t_u8   (PRIMITIVE_UNSIGNED, 1, "u8");
AST_PrimitiveType t_u16  (PRIMITIVE_UNSIGNED, 2, "u16");
AST_PrimitiveType t_u32  (PRIMITIVE_UNSIGNED, 4, "u32");
AST_PrimitiveType t_u64  (PRIMITIVE_UNSIGNED, 8, "u64");
AST_PrimitiveType t_i8   (PRIMITIVE_SIGNED,   1, "i8");
AST_PrimitiveType t_i16  (PRIMITIVE_SIGNED,   2, "i16");
AST_PrimitiveType t_i32  (PRIMITIVE_SIGNED,   4, "i32");
AST_PrimitiveType t_i64  (PRIMITIVE_SIGNED,   8, "i64");
AST_PrimitiveType t_f32  (PRIMITIVE_FLOAT,    4, "f32");
AST_PrimitiveType t_f64  (PRIMITIVE_FLOAT,    8, "f64");


AST_PrimitiveType t_type           (PRIMITIVE_UNIQUE,  0, "type");
AST_PrimitiveType t_void           (PRIMITIVE_UNIQUE,  0, "void");
AST_PrimitiveType t_string_literal (PRIMITIVE_UNIQUE,  0, "string_literal");
AST_PrimitiveType t_number_literal (PRIMITIVE_UNIQUE,  0, "number_literal");

// This is a special type that anything with size <= 8 can implicitly cast to
// variadic arguments are assumed to be of type any8
AST_PrimitiveType t_any8 (PRIMITIVE_UNIQUE, 8, "any8");

arr<AST_Type*> unsigned_types = { &t_u64, &t_u32, &t_u16, &t_u8 };
arr<AST_Type*>   signed_types = { &t_i64, &t_i32, &t_i16, &t_i8 };


struct GetTypeJob : Job {
    AST_Context &ctx;
    AST_Value   *node; // TODO use Job.result instead of this

    AST_Type *cache1, *cache2;

    GetTypeJob(AST_Context &ctx, AST_Value *node) 
        : ctx(ctx), node(node), Job(ctx.global) { }

    bool run(Message *msg) override;
    std::wstring get_name() override;
};

std::wstring GetTypeJob::get_name() {
    std::wostringstream s;
    s << "GetTypeJob<" << node << ">";
    return s.str();
}

TypeCheckJob::TypeCheckJob(AST_Context& ctx, AST_Node* node) 
    : ctx(ctx), node(node), Job(ctx.global) 
{
    assert(node);
}

std::wstring TypeCheckJob::get_name() {
    std::wostringstream s;
    s << "TypeCheckJob<";
    if (node IS AST_BLOCK) {
        AST_Context *block = (AST_Context*)node;
        if (&block->global == block)
            s << "Global Scope";
        else
            s << "Scope in " << block->fn;
    } else {
        print(s, this->node, false);
    }
    s << ">";
    return s.str();
}


void add_string_global(struct TIR_Context *tir_context, AST_Var *the_string_var, AST_StringLiteral *the_string_literal);


bool is_valid_lvalue(AST_Node* expr) {
    switch (expr->nodetype) {
        case AST_VAR:
            return true;
        case AST_MEMBER_ACCESS:
            return true;
        case AST_DEREFERENCE:
            return true;
        default:
            return false;
    }
}

bool is_integral_type(AST_Type* type) {
    switch (type->nodetype) {
        case AST_PRIMITIVE_TYPE: {
            AST_PrimitiveType* prim = (AST_PrimitiveType*)type;
            return prim->kind == PRIMITIVE_BOOL
                || prim->kind == PRIMITIVE_SIGNED
                || prim->kind == PRIMITIVE_UNSIGNED;
        }
        default: 
            return false;
    }
}

bool validate_type(AST_Context& ctx, AST_Type** type);

bool validate_fn_type(AST_Context& ctx, AST_Fn* fn) {
    AST_FnType *fntype = fn->fntype();
    
    for (auto& p : fntype->param_types) {
        // we must check if the parameters are valid AST_Types
        MUST (validate_type(ctx, &p));
    }

    if (!fntype->returntype)
        fntype->returntype = &t_void;

    MUST (validate_type(ctx, &fntype->returntype));
    
    fntype = ctx.make_function_type_unique(fntype);
    fn->type = fntype;
    
    return fn->type;
}


bool GetTypeJob::run(Message *msg) {
    if (node->type) {
        return true;
    }

    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE: 
        case AST_FN_TYPE:
        case AST_POINTER_TYPE:
        {
            node->type = &t_type;
            return true;
        }

        case AST_FN: {
            NOT_IMPLEMENTED();
        }

        case AST_FN_CALL: {
            AST_Call *fncall = (AST_Call*)node;

            for (int i = 0; i < fncall->args.size; i++) {
                // TODO TODO
                GetTypeJob arg_gettype(ctx, fncall->args[i]);
                WAIT (arg_gettype, GetTypeJob, GetTypeJob);
            }

            if (fncall->op) {
                OpResolveJob resolve_fn_job(ctx.global, fncall);
                resolve_fn_job.fncall = fncall;
                
                WAIT (resolve_fn_job, GetTypeJob, CallResolveJob,
                    ctx.subscribers.push(heap_job);
                );
            } else {
                assert(fncall->fn IS AST_UNRESOLVED_ID);

                CallResolveJob resolve_fn_job(ctx, fncall);
                resolve_fn_job.fncall = fncall;
                
                WAIT (resolve_fn_job, GetTypeJob, CallResolveJob,
                    ctx.subscribers.push(heap_job);
                );
            }

            return true;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* ma = (AST_MemberAccess*)node;

            GetTypeJob type_job(ctx, ma->lhs);
            WAIT (type_job, GetTypeJob, GetTypeJob);

            AST_Struct* s = (AST_Struct*)type_job.node->type;
            MUST_OR_FAIL_JOB (s);

            if (s->nodetype != AST_STRUCT) {
                // TODO ERROR
                error({ .code = ERR_NO_SUCH_MEMBER, });
                return false;
            }

            for (u32 i = 0; i < s->members.size; i++) {
                auto& member = s->members[i];
                if (!strcmp(member.name, ma->member_name)) {
                    ma->index = i;
                    ma->type = member.type;
                    return member.type;
                }
            }

            error({ .code = ERR_NO_SUCH_MEMBER, });
            return false;
        }

        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;

            GetTypeJob inner_type_job(ctx, deref->ptr);
            WAIT (inner_type_job, GetTypeJob, GetTypeJob);

            if (inner_type_job.node->type->nodetype != AST_POINTER_TYPE) {
                error({
                    .code = ERR_INVALID_DEREFERENCE,
                    .nodes = { deref },
                });
                return false;
            }

            node->type = deref->type = ((AST_PointerType*)inner_type_job.node->type)->pointed_type;
            return true;
        }

        case AST_ADDRESS_OF: {
            AST_AddressOf* addrof = (AST_AddressOf*)node;

            GetTypeJob inner_type_job(ctx, addrof->inner);
            WAIT (inner_type_job, GetTypeJob, GetTypeJob);

            addrof->type = ctx.get_pointer_type(inner_type_job.node->type);

            if (addrof->inner IS AST_VAR) {
                ((AST_Var*)addrof->inner)->always_on_stack = true;
            }

            node->type = addrof->type;
            return true;
        }

        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId *unres = (AST_UnresolvedId*)node;
            // TODO Duplicate
            heapify<GetTypeJob>()->add_dependency(unres->job);
            return false;
        }

        default:
            NOT_IMPLEMENTED();
    }

    UNREACHABLE;
}

// Types are parsed by the parse_expr function
// When parse_expr sees foo* it doesn't know it it's a dereference 
// or a pointer type, so it just assumes it's a dereference
//
// Same goes for foo[123], is it a index operator or array type?
//
// This is called on a AST_Value outputted by parse_expr
// to patch up the node into a valid type
//
// TODO ALLOCATION this function leaks from all sides
bool validate_type(AST_Context& ctx, AST_Type** type) {
    
    AST_Value* val = (AST_Value*)*type;

    if (val->nodetype & AST_TYPE_BIT)
        return true;

    switch (val->nodetype) {
        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)val;

            MUST (validate_type(ctx, (AST_Type**)&deref->ptr));

            Location loc = location_of(ctx, (AST_Node**)&deref);

            *type = ctx.get_pointer_type((AST_Type*)deref->ptr);
            ctx.global.reference_locations[(AST_Node**)type] = loc;

            return true;
        }
        case AST_TYPEOF: {
            NOT_IMPLEMENTED();
        }
        default: {
            ctx.error({
                .code = ERR_INVALID_TYPE,
                .node_ptrs = { (AST_Node**)val },
            });
            return false;
        }
    }

    return true;
};


bool TypeCheckJob::run(Message *msg) {
    switch (node->nodetype) {
        case AST_BLOCK: {
            AST_Context* block = (AST_Context*)node;

            for (const auto& decl : block->declarations) {
                if (decl.value IS AST_VAR) {
                    AST_Var* var = (AST_Var*)decl.value;

                    TypeCheckJob var_typecheck(*block, var); 
                    WAIT (var_typecheck, TypeCheckJob, TypeCheckJob);

                    // TODO STRUCT
                    // Right now structures are always on the stack
                    // since they are always passed by pointer in LLVM
                    // with the byval tag
                    if (var->type IS AST_STRUCT)
                        var->always_on_stack = true;
                }
            }

            for (const auto& stmt : block->statements) {
                TypeCheckJob stmt_typecheck(*block, stmt); 
                WAIT (stmt_typecheck, TypeCheckJob, TypeCheckJob);
            }
            return true;
        }

        case AST_FN: {
            AST_Fn *fn = (AST_Fn*)node;

            GetTypeJob gettype(ctx, fn);
            WAIT (gettype, TypeCheckJob, GetTypeJob);

            MUST_OR_FAIL_JOB (validate_fn_type(ctx, fn));

            if (fn->name && !declared) {
                declared = true;

                DeclarationKey key = {
                    .name = fn->name,
                    .fn_type = fn->fntype()
                };

                // we know the fn's type, declare it
                MUST_OR_FAIL_JOB (ctx.declare(key, fn, true));
                ctx.decrement_hanging_declarations();
            }

            TypeCheckJob fn_typecheck(fn->block, &fn->block); 
            WAIT (fn_typecheck, TypeCheckJob, TypeCheckJob);

            return true;
        }

        case AST_MACRO: {
            AST_Macro *macro = (AST_Macro*)node;

            return true;
        }

        case AST_VAR: {
            AST_Var *var = (AST_Var*)node;
            if (var->type) {
                MUST_OR_FAIL_JOB (validate_type(ctx, &var->type));
            } else {
                assert(var->argindex >= 0);
                assert(ctx.fn IS AST_FN);
                var->type = ((AST_Fn*)ctx.fn)->fntype()->param_types[var->argindex];
            }
            return true;
        }

        case AST_RETURN: {
            AST_Return *ret = (AST_Return*)node;
            AST_Type *rettype = ((AST_FnType*)ctx.fn->type)->returntype;

            // TODO VOID
            if (!rettype)
                rettype = &t_void;

            if (ret->value) {
                TypeCheckJob ret_typecheck(ctx, ret->value); 
                WAIT (ret_typecheck, TypeCheckJob, TypeCheckJob);
            }

            if (rettype != &t_void) {
                if (!ret->value) {
                    error({ .code = ERR_RETURN_TYPE_MISSING, .nodes = { ret, ctx.fn, } });
                    return false;
                }

                CastJob cast_job(&ctx, ret->value, rettype, nullptr);
                WAIT (cast_job, TypeCheckJob, CastJob);

                ret->value = (AST_Value*)cast_job.result;

                return true;
            } else if (ret->value) {
                error({
                    .code = ERR_RETURN_TYPE_INVALID,
                    .nodes = { ret, ctx.fn }
                });
                return false;
            }
            return true;
        }

        case AST_IF: {
            AST_If* ifs = (AST_If*)node;

            TypeCheckJob cond_typecheck(ctx, ifs->condition); 
            WAIT (cond_typecheck, TypeCheckJob, TypeCheckJob);

            TypeCheckJob then_typecheck(ctx, &ifs->then_block); 
            WAIT (then_typecheck, TypeCheckJob, TypeCheckJob);

            return true;
        }

        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;

            TypeCheckJob cond_typecheck(ctx, whiles->condition); 
            WAIT (cond_typecheck, TypeCheckJob, TypeCheckJob);

            TypeCheckJob block_typecheck(ctx, &whiles->block); 
            WAIT (block_typecheck, TypeCheckJob, TypeCheckJob);

            return true;
        }

        case AST_STRUCT: {
            AST_Struct* s = (AST_Struct*)node;
            s->size = 0;

            for (auto& member : s->members) {
                TypeCheckJob block_typecheck(ctx, member.type); 
                WAIT (block_typecheck, TypeCheckJob, TypeCheckJob);

                u64 member_size = member.type->size;

                // For most types, alignment = size
                // For structs, alignment = biggest member alignment
                u64 member_alignment = member.type->size;
                if (member.type IS AST_STRUCT) {
                    AST_Struct *member_struct = (AST_Struct*)member.type;
                    member_alignment = member_struct->alignment;
                }
                if (s->alignment < member_alignment)
                    s->alignment = member_alignment;

                u64 padding = s->size % member_size;
                if (padding > 0)
                    padding = member_size - padding;

                member.offset = s->size + padding;
                s->size += padding + member_size;
            }

            u64 end_padding = s->size % s->alignment;
            if (end_padding > 0)
                end_padding = s->alignment - end_padding;
            s->size += end_padding;

            return true;
        }

        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId *id = (AST_UnresolvedId*)node;
            // TODO Duplicate
            heapify<TypeCheckJob>()->add_dependency(id->job);
            return false;
        }

        default: {
            if (node->nodetype & AST_VALUE_BIT) {
                GetTypeJob gettype(ctx, (AST_Value*)node);
                WAIT (gettype, TypeCheckJob, GetTypeJob);
                return true;
            }
            NOT_IMPLEMENTED();
        }

    }
}
