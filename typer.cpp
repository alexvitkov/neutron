#include "typer.h"
#include "ast.h"
#include "error.h"
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

// This is a special type that anything with size <= 8 can implicitly cast to
// variadic arguments are assumed to be of type any8
AST_PrimitiveType t_any8 (PRIMITIVE_UNIQUE, 8, "any8");


struct GetTypeJob : Job {
    AST_Context &ctx;
    AST_Value   *node;

    AST_Type *cache_lhs, *cache_rhs;

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

bool map_equals(AST_FnType* lhs, AST_FnType* rhs) {
    MUST (lhs->returntype == rhs->returntype);
    MUST (lhs->param_types.size == rhs->param_types.size);
    MUST (lhs->is_variadic == rhs->is_variadic);

    for (u32 i = 0; i < lhs->param_types.size; i++)
        MUST (lhs->param_types[i] == rhs->param_types[i]);

    return true;
}

void add_string_global(struct TIR_Context *tir_context, AST_Var *the_string_var, AST_StringLiteral *the_string_literal);


bool can_implicit_cast(AST_Type *dsttype, AST_Type *srctype) {
    return srctype == dsttype;
}


bool implicit_cast(AST_Context &ctx, AST_Value **dst, AST_Type *type) {
    AST_Type *dstt = (*dst)->type;

    MUST (dstt);

    if (dstt == type) 
        return true;

    if (dstt IS AST_PRIMITIVE_TYPE 
            && type IS AST_PRIMITIVE_TYPE
            && ((AST_PrimitiveType*)type)->kind != PRIMITIVE_UNIQUE)
    {
        AST_PrimitiveType* l = (AST_PrimitiveType*)dstt;
        AST_PrimitiveType* r = (AST_PrimitiveType*)type;

        if ((*dst) IS AST_NUMBER) {
            AST_Number* num = (AST_Number*)*dst;
            if (num->fits_in((AST_PrimitiveType*)type)) {
                num->type = type;
                return true;
            } else {
                return false;
            }
        }
        else {
            // We allow very conservative implicit casting
            // bool -> u8 -> u16 -> u32 -> u64
            //    \     \       \      \
            //     i8 -> i16 -> i32 -> i64
            if (l == &t_bool
                    || (l->kind == r->kind && l->size < r->size)
                    || (l->kind == PRIMITIVE_UNSIGNED && r->kind == PRIMITIVE_SIGNED && l->size < r->size))
            {
                AST_Cast *cast = ctx.alloc<AST_Cast>(r, *dst);;
                *dst = cast;

                cast->casttype = l->kind == PRIMITIVE_UNSIGNED ? AST_Cast::ZeroExtend : AST_Cast::SignedExtend;
                return true;
            }
        }
    }

    else if (dstt == &t_string_literal) {
        if (type == ctx.get_pointer_type(&t_i8) || type == &t_any8) {

            AST_StringLiteral* str = (AST_StringLiteral*)*dst;

            // If the same string literal has been seen before
            // There's already a static variable defined for it
            AST_Var* string_static_var = (AST_Var*)ctx.declarations.find2({ .string_literal = str });

            if (!string_static_var) {
                // Get an array type that's just big enough to fit the C string 
                AST_ArrayType* string_array_type = ctx.get_array_type(&t_i8, str->length + 1);

                string_static_var = ctx.alloc<AST_Var>(nullptr, 0);
                string_static_var->type = string_array_type;
                string_static_var->is_constant = true;
                // ctx.global.global_initial_nodes[string_static_var] = str;

                add_string_global(ctx.global.tir_context, string_static_var, str);

                string_static_var->is_global = true;

                MUST (ctx.global.declare({ .string_literal = str }, string_static_var, false));
            }

            // AST_Cast *cast = ctx.alloc<AST_Cast>(ctx.get_pointer_type(&t_i8), string_static_var);
            // cast->casttype = AST_Cast::StringLiteral_I8;
            *dst = string_static_var;
            return true;
        }

        return false;
    }

    else if (type == &t_any8) {
        return true;
    }

    return false;
}

bool match_fn_call(AST_Context &ctx, AST_FnCall *fncall, AST_Fn *fn) {
    AST_FnType *fntype = (AST_FnType*)fn->type;

    u64 num_args = fncall->args.size;
    u64 num_params = fntype->param_types.size;
    // wcout << "Matching " << fncall << " to " << fn << "\n";

    if (!fntype->is_variadic) {
        MUST (num_args == num_params);
    } else {
        MUST (num_args >= num_params);
    }

    for (int i = 0; i < num_args; i++) {
        AST_Type *param_type = i < num_params ? fntype->param_types[i] : &t_any8;
        AST_Type *arg_type   = fncall->args[i]->type;

        MUST (can_implicit_cast(param_type, arg_type));
    }

    fncall->type = fntype->returntype;
    fncall->fn = fn;

    wcout << "Matched " << fncall << " to " << fn << "\n";
    return true;
}

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
    AST_FnType *fntype = (AST_FnType*)fn->type;
    
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
            AST_FnCall *fncall = (AST_FnCall*)node;

            for (int i = 0; i < fncall->args.size; i++) {
                GetTypeJob arg_gettype(ctx, fncall->args[i]);
                WAIT (arg_gettype, GetTypeJob);
            }

            switch (fncall->fn->nodetype) {
                case AST_UNRESOLVED_ID: {
                    ResolveJob resolve_fn_job(ctx, nullptr);
                    resolve_fn_job.fncall = fncall;

                    WAIT (resolve_fn_job, ResolveJob,
                        heap_job->subscribe(MSG_NEW_DECLARATION);
                        heap_job->subscribe(MSG_SCOPE_CLOSED);
                    );
                    assert(!"Finally");

                    assert(fncall->fn->nodetype == AST_FN);

                    return true;
                }
                default: {
                    // CAST
                    NOT_IMPLEMENTED();
                }
            }

        }

        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;

            GetTypeJob lhs_job(ctx, bin->lhs);
            WAIT (lhs_job, GetTypeJob);

            GetTypeJob rhs_job(ctx, bin->rhs);
            WAIT (rhs_job, GetTypeJob);

            // Assignments are handled differently from other binary operators
            if ((prec[bin->op] & ASSIGNMENT) == ASSIGNMENT) {
                bin->nodetype = AST_ASSIGNMENT;

                if (!is_valid_lvalue(bin->lhs)) {
                    error({ .code = ERR_NOT_AN_LVALUE, .nodes = { lhs_job.node->type, } });
                    return false;
                }

                if (!implicit_cast(ctx, &bin->rhs, lhs_job.node->type)) {
                    // TODO ERROR
                    error({});
                    return false;
                }
                
                bin->type = lhs_job.node->type;
                return true;
            }

            // Handle pointer arithmetic
            if (lhs_job.node->type IS AST_POINTER_TYPE || rhs_job.node->type IS AST_POINTER_TYPE) {
                if (bin->op != '+' && bin->op != '-') {
                    error({
                        .code = ERR_INVALID_ASSIGNMENT,
                        .nodes = { bin->lhs, bin-> rhs }
                    });
                    return false;
                }

                // There are 6 cases here
                // Pointer  + Integral -- valid
                // Integral + Pointer  -- valid
                // Pointer  + Pointer  -- invalid
                
                // Pointer  - Integral -- valid
                // Pointer  - Pointer  -- valid
                // Integral - Pointer  -- invalid

                if (lhs_job.node->type IS AST_POINTER_TYPE) {
                    // If LHS is a pointer and operator is +, RHS can only be an integral type
                    if (bin->op == '+' && !is_integral_type(rhs_job.node->type))
                        goto Error;

                    if (bin->op == '-') {
                        // Pointer Pointer subtraction is valid if the pointers the same type
                        if (rhs_job.node->type IS AST_POINTER_TYPE) {
                            if (lhs_job.node->type != rhs_job.node->type)
                                goto Error;
                            bin->op = OP_SUB_PTR_PTR;
                        }
                        // The other valid case is Pointer - Integral
                        else {
                            if (!is_integral_type(rhs_job.node->type))
                                goto Error;
                            bin->op = OP_SUB_PTR_INT;
                        }
                    }
                    else {
                        bin->op = OP_ADD_PTR_INT;
                    }


                } else {
                    // LHS is not a pointer => the only valid case is Integral + Pointer
                    if (!is_integral_type(lhs_job.node->type) || bin->op != '+')
                        goto Error;

                    // In order to simplify code generation
                    // we will swap the arguments so that the pointer is LHS
                    std::swap(bin->lhs, bin->rhs);
                    bin->op = OP_ADD_PTR_INT;
                }

                // After this is all done, the type of the binary expression
                // is the same as the pointer type
                bin->type = lhs_job.node->type IS AST_POINTER_TYPE ? lhs_job.node->type : rhs_job.node->type;
                return bin->type;

                NOT_IMPLEMENTED("TODO ERROR");
                return false;
            }

            // Handle regular human arithmetic between numbers
            else {
                if (implicit_cast(ctx, &bin->rhs, lhs_job.node->type)) {
                    bin->type = lhs_job.node->type;
                    return true;
                }

                if (implicit_cast(ctx, &bin->lhs, rhs_job.node->type)) {
                    bin->type = rhs_job.node->type;
                    return true;
                }
            }

Error:
            error({
                .code = ERR_BAD_BINARY_OP,
                .nodes = { bin->lhs, bin->rhs }
            });
            return false;
        }

        case AST_UNARY_OP: {
            AST_UnaryOp *un = (AST_UnaryOp*)node;

            GetTypeJob type_job(ctx, un->inner);
            WAIT (type_job, GetTypeJob);

            NOT_IMPLEMENTED();
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* ma = (AST_MemberAccess*)node;

            GetTypeJob type_job(ctx, ma->lhs);
            WAIT (type_job, GetTypeJob);

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
            WAIT (inner_type_job, GetTypeJob);

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
            WAIT (inner_type_job, GetTypeJob);

            addrof->type = ctx.get_pointer_type(inner_type_job.node->type);

            if (addrof->inner IS AST_VAR) {
                ((AST_Var*)addrof->inner)->always_on_stack = true;
            }

            node->type = addrof->type;
            return true;
        }

        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId *unres = (AST_UnresolvedId*)node;
            add_dependency(unres->job);
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
                    WAIT (var_typecheck, TypeCheckJob);

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
                WAIT (stmt_typecheck, TypeCheckJob);
            }
            return true;
        }

        case AST_FN: {
            AST_Fn* fn = (AST_Fn*)node;

            GetTypeJob gettype(ctx, fn);
            WAIT (gettype, GetTypeJob);

            MUST_OR_FAIL_JOB (validate_fn_type(ctx, fn));

            if (fn->name && !declared) {
                declared = true;

                DeclarationKey key = {
                    .name = fn->name,
                    .fn_type = (AST_FnType*)fn->type
                };

                // we know the fn's type, declare it
                MUST_OR_FAIL_JOB (ctx.declare(key, fn, true));
                ctx.decrement_hanging_declarations();
            }

            TypeCheckJob fn_typecheck(fn->block, &fn->block); 
            WAIT (fn_typecheck, TypeCheckJob);

            return true;
        }

        case AST_VAR: {
            AST_Var* var = (AST_Var*)node;
            if (var->type) {
                MUST_OR_FAIL_JOB (validate_type(ctx, &var->type));
            } else {
                assert(var->argindex >= 0);
                AST_FnType *fntype = (AST_FnType*)this->ctx.fn->type;
                var->type = fntype->param_types[var->argindex];
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
                WAIT (ret_typecheck, TypeCheckJob);
            }

            if (rettype != &t_void && !ret->value) {
                error({
                    .code = ERR_RETURN_TYPE_MISSING,
                    .nodes = { ret, ctx.fn,
                    }
                });
                return false;
            }

            if (rettype != &t_void && !implicit_cast(ctx, &ret->value, rettype)) {
                error({
                    .code = ERR_CANNOT_IMPLICIT_CAST, 
                    .nodes = {
                        rettype,
                        ret->value,
                        ret
                    }
                });
                return false;
            }

            if (rettype == &t_void && ret->value) {
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
            WAIT (cond_typecheck, TypeCheckJob);

            TypeCheckJob then_typecheck(ctx, &ifs->then_block); 
            WAIT (then_typecheck, TypeCheckJob);

            return true;
        }

        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;

            TypeCheckJob cond_typecheck(ctx, whiles->condition); 
            WAIT (cond_typecheck, TypeCheckJob);

            TypeCheckJob block_typecheck(ctx, &whiles->block); 
            WAIT (block_typecheck, TypeCheckJob);

            return true;
        }

        case AST_STRUCT: {
            AST_Struct* s = (AST_Struct*)node;
            s->size = 0;

            for (auto& member : s->members) {
                TypeCheckJob block_typecheck(ctx, member.type); 
                WAIT (block_typecheck, TypeCheckJob);

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
            add_dependency(id->job);
            return false;
        }

        default: {
            if (node->nodetype & AST_VALUE_BIT) {
                GetTypeJob gettype(ctx, (AST_Value*)node);
                WAIT (gettype, GetTypeJob);
                return true;
            }
            NOT_IMPLEMENTED();
        }

    }
}

/*
bool typecheck_all(AST_GlobalContext& global) {

    // During typechecking we can declare more stuff
    // so it's not safe to iterate over global.declarations as it may relocate
    // Right now we copy the original declartions into an array and iterate over those
    // This means that the new declarations WON'T BE TYPECHECKED
    arr<AST_Node*> declarations_copy;
    for (auto& decl : global.declarations)
        declarations_copy.push(decl.value);

    // First resolve the function types, we need their signatures for everything else
    for (auto& decl : declarations_copy) {
        if (decl IS AST_FN) {
            MUST (validate_fn_type(global, (AST_Fn*)decl));
        }
        else if (decl IS AST_VAR) {
            MUST (typecheck(global, decl));
        }
    }

    for (AST_Node*& stmt : global.statements)
        MUST (typecheck(global, stmt));

    // global.temp_allocator.free_all();

    for (auto& decl : declarations_copy)
        MUST (typecheck(global, decl));

    return true;
}
*/
