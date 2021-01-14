#include "typer.h"
#include "ast.h"
#include "error.h"

AST_PrimitiveType t_bool (PRIMITIVE_BOOL,    1, "bool");
AST_PrimitiveType t_u8  (PRIMITIVE_UNSIGNED, 1, "u8");
AST_PrimitiveType t_u16 (PRIMITIVE_UNSIGNED, 2, "u16");
AST_PrimitiveType t_u32 (PRIMITIVE_UNSIGNED, 4, "u32");
AST_PrimitiveType t_u64 (PRIMITIVE_UNSIGNED, 8, "u64");
AST_PrimitiveType t_i8  (PRIMITIVE_SIGNED,   1, "i8");
AST_PrimitiveType t_i16 (PRIMITIVE_SIGNED,   2, "i16");
AST_PrimitiveType t_i32 (PRIMITIVE_SIGNED,   4, "i32");
AST_PrimitiveType t_i64 (PRIMITIVE_SIGNED,   8, "i64");
AST_PrimitiveType t_f32 (PRIMITIVE_FLOAT,    4, "f32");
AST_PrimitiveType t_f64 (PRIMITIVE_FLOAT,    8, "f64");
AST_PrimitiveType t_type (PRIMITIVE_TYPE,    0, "type");
AST_PrimitiveType t_void (PRIMITIVE_TYPE,    0, "void");


// TODO This should be a hashset if someone ever makes one
//
// The reason this exists is to make sure function types are unique
// Type pointers MUST be comparable by checking their pointers with ==
// so we need additional logic to make sure we don't create the same
// composite type twice.
map<AST_FnType*, AST_FnType*> fn_types_hash;

map<AST_Type*, AST_PointerType*> pointer_types;

u32 hash(AST_Type* returntype, arr<AST_Type*>& param_types) {
    u32 hash = map_hash(returntype);

    for (AST_Type* param : param_types)
        hash ^= map_hash(param);

    return hash;
}

u32 map_hash(AST_FnType* fn_type) {
    return hash(fn_type->returntype, fn_type->param_types);
};

// TODO RESOLUTION we should check if it's a AST_UnresolvedId
AST_PointerType* get_pointer_type(AST_Type* pointed_type) {
    AST_PointerType* pt;
    if (!pointer_types.find(pointed_type, &pt)) {
        pt = new AST_PointerType(pointed_type);
        pointer_types.insert(pointed_type, pt);
    }
    return pt;
}

bool map_equals(AST_FnType* lhs, AST_FnType* rhs) {
    MUST (lhs->returntype == rhs->returntype);
    MUST (lhs->param_types.size == rhs->param_types.size);

    for (int i = 0; i < lhs->param_types.size; i++)
        MUST (lhs->param_types[i] == rhs->param_types[i]);

    return true;
}

AST_FnType* get_fn_type(AST_Type* returntype, arr<AST_Type*> param_types) {
    AST_FnType tmp;

    tmp.param_types = std::move(param_types);
    tmp.returntype = returntype;

    AST_FnType* result;
    if (fn_types_hash.find(&tmp, &result)) {
        return result;
    }
    else {
        // TODO ALLOCATION we need a sane allocation here
        AST_FnType* newtype = new AST_FnType(std::move(tmp));

        fn_types_hash.insert(newtype, newtype);
        return newtype;
    }
}

bool implicit_cast(Context& ctx, AST_Node** dst, AST_Type* type) {
    AST_Type* dstt = gettype(ctx, *dst);
    MUST (dstt);

    if (dstt == type) 
        return true;

    if (dstt->nodetype != AST_PRIMITIVE_TYPE || type->nodetype != AST_PRIMITIVE_TYPE)
        return false;

    AST_PrimitiveType* l = (AST_PrimitiveType*)dstt;
    AST_PrimitiveType* r = (AST_PrimitiveType*)type;

    if (l == &t_bool
            || (l->kind == r->kind && l->size < r->size)
            || (l->kind == PRIMITIVE_UNSIGNED && r->kind == PRIMITIVE_SIGNED && l->size < r->size))
    {
        if ((*dst)->nodetype == AST_NUMBER) {
            ((AST_Number*)(*dst))->type = type;
        }
        else {
            *dst = ctx.alloc<AST_Cast>(r, *dst);
        }
        return true;
    }
    return false;
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

bool resolve_unresolved_references(AST_Node** nodeptr);

bool resolve_unresolved_references(AST_Block* block) {
    for (auto& stmt : block->statements)
        MUST (resolve_unresolved_references(&stmt));
    return true;
}

inline bool must_be_type(Context& ctx, AST_Node* node) {
    if (!(node->nodetype & AST_TYPE_BIT)) {
        ctx.error({ .code = ERR_INVALID_TYPE, .nodes = { node } });
        return false;
    }
    return true;
}

bool resolve_unresolved_references(AST_Node** nodeptr) {
    AST_Node* node = *nodeptr;
    if (!node)
        return true;

    switch (node->nodetype) {
        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId* id = (AST_UnresolvedId*)node;
            AST_Node* resolved = id->ctx.resolve(id->name);
            if (!resolved) {
                id->ctx.error({
                    .code = ERR_NOT_DEFINED,
                    .nodes = { id }
                });
                return false;
            }
            *nodeptr = resolved;
            return true;

        }
        case AST_VAR: {
            AST_Var* var = (AST_Var*)node;
            MUST (resolve_unresolved_references(&var->initial_value));
            MUST (resolve_unresolved_references((AST_Node**)&var->type));
            break;
        }
        case AST_CAST: {
            AST_Cast* cast = (AST_Cast*)node;
            MUST (resolve_unresolved_references(&cast->inner));
            MUST (resolve_unresolved_references((AST_Node**)&cast->type));
            break;
        }
        case AST_FN_CALL: {
            AST_FnCall* fncall = (AST_FnCall*)node;
            MUST (resolve_unresolved_references(&fncall->fn));
            for (auto& arg : fncall->args)
                MUST (resolve_unresolved_references(&arg));
            break;
        }
        case AST_FN: {
            AST_Fn* fn = (AST_Fn*)node;
            for (auto& arg : fn->args)
                MUST (resolve_unresolved_references((AST_Node**)&arg.type));
            MUST (resolve_unresolved_references(&fn->block));
            break;
        }
        case AST_ASSIGNMENT:
        case AST_BINARY_OP: 
        {
            AST_BinaryOp* op = (AST_BinaryOp*)node;
            MUST (resolve_unresolved_references(&op->lhs));
            MUST (resolve_unresolved_references(&op->rhs));
            break;
        }
        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;
            MUST (resolve_unresolved_references((AST_Node**)&deref->ptr));
            return true;
        }
        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            MUST (resolve_unresolved_references(&ret->value));
            break;
        }
        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (resolve_unresolved_references(&ifs->then_block));
            MUST (resolve_unresolved_references(&ifs->condition));
            break;
        }
        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;
            MUST (resolve_unresolved_references(&whiles->block));
            MUST (resolve_unresolved_references(&whiles->condition));
            break;
        }
        case AST_BLOCK: {
            MUST (resolve_unresolved_references((AST_Block*)node));
            break;
        }
        case AST_STRUCT: {
            AST_Struct* str = (AST_Struct*)node;
            for (auto& mem : str->members)
                MUST (resolve_unresolved_references((AST_Node**)&mem.type));
            break;
        }
        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* access = (AST_MemberAccess*)node;
            MUST (resolve_unresolved_references(&access->lhs));
            break;
        }
        case AST_POINTER_TYPE: {
            AST_PointerType* pt = (AST_PointerType*)node;
            if (pt->pointed_type->nodetype == AST_UNRESOLVED_ID) {
                MUST (resolve_unresolved_references((AST_Node**)&pt->pointed_type));

                *nodeptr = get_pointer_type(pt->pointed_type);
                // TODO ALLOCATION free the temp pointer type here
            }
            break;
        }


        case AST_NONE:
        case AST_TEMP_REF:
            assert(!"Not supported");

        case AST_NUMBER:
        case AST_PRIMITIVE_TYPE:
            break;


        default:
            assert(!"Not implemented");
    }
    return true;
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

AST_Type* gettype(Context& ctx, AST_Node* node) {
    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE: 
        case AST_FN_TYPE:
        case AST_POINTER_TYPE:
        {
            return &t_type;
        }

        case AST_FN: {
            AST_Fn *fn = (AST_Fn*)node;
            if (fn->type)
                return fn->type;

            arr<AST_Type*> param_types(fn->args.size);

            for (auto& p : fn->args) {
                // Right now we know that the parameters' .type 
                // are valid nodes, we must check if they're types
                MUST (must_be_type(ctx, p.type));
                param_types.push(p.type);
            }

            // TODO RETURNTYPE
            AST_Type* rettype = (AST_Type*)fn->block.ctx.resolve("returntype");
            if (rettype)
                MUST (must_be_type(ctx, rettype));

            fn->type = get_fn_type(rettype, std::move(param_types));


            if (!strcmp(fn->name, "printf"))
                ((AST_FnType*)fn->type)->is_variadic = true;
            return fn->type;
        }

        case AST_VAR:
        case AST_CAST:
        case AST_NUMBER:
        {
            return ((AST_Value*)node)->type;
        }

        case AST_FN_CALL: {
            AST_FnCall *fncall = (AST_FnCall*)node;
            if (fncall->type)
                return fncall->type;

            AST_Fn *fn = (AST_Fn*)fncall->fn;
            assert(fn->nodetype == AST_FN);

            // TODO RETURNTYPE
            AST_Type* returntype = (AST_Type*)fn->block.ctx.resolve("returntype");

            if (fncall->args.size != fn->args.size) {
                ctx.error({ .code = ERR_BAD_FN_CALL });
                return nullptr;
            }

            for (int i = 0; i < fn->args.size; i++) {
                if (!implicit_cast(ctx, &fncall->args[i], fn->args[i].type))
                    return nullptr;
            }

            if (!returntype) {
                returntype = &t_void;
            }

            fncall->type = returntype;
            return returntype;
        }

        case AST_ASSIGNMENT: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;
            if (bin->type)
                return bin->type;

            AST_Type* lhst = gettype(ctx, bin->lhs);
            AST_Type* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            if (!is_valid_lvalue(bin->lhs)) {
                ctx.error({ .code = ERR_NOT_AN_LVALUE, .nodes = { lhst, } });
                return nullptr;
            }

            if (implicit_cast(ctx, &bin->rhs, lhst))
                return (bin->type = rhst);

            ctx.error({ 
                .code = ERR_INCOMPATIBLE_TYPES, 
                .nodes = {
                    bin
                },
                .node_ptrs = { 
                    (AST_Node**)&bin->lhs, 
                    (AST_Node**)&bin->rhs, 
                }
            });
            return nullptr;
        }

        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;
            if (bin->type)
                return bin->type;

            AST_Type* lhst = gettype(ctx, bin->lhs);
            AST_Type* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            // Handle pointer arithmetic
            if (lhst->nodetype == AST_POINTER_TYPE || rhst->nodetype == AST_POINTER_TYPE) {
                if (bin->op != '+' && bin->op != '-') {
                    ctx.error({
                        .code = ERR_INCOMPATIBLE_TYPES,
                        .nodes = { bin->lhs, bin-> rhs }
                    });
                    return nullptr;
                }

                // There are 6 cases here
                // Pointer  + Integral -- valid
                // Integral + Pointer  -- valid
                // Pointer  + Pointer  -- invalid
                
                // Pointer  - Integral -- valid
                // Pointer  - Pointer  -- valid
                // Integral - Pointer  -- invalid

                if (lhst->nodetype == AST_POINTER_TYPE) {
                    // If LHS is a pointer and operator is +, RHS can only be an integral type
                    if (bin->op == '+' && !is_integral_type(rhst))
                        goto Error;

                    if (bin->op == '-') {
                        // Pointer Pointer subtraction is valid if the pointers the same type
                        if (rhst->nodetype == AST_POINTER_TYPE) {
                            if (lhst != rhst)
                                goto Error;
                            bin->op = OP_SUB_PTR_PTR;
                        }
                        // The other valid case is Pointer - Integral
                        else {
                            if (!is_integral_type(rhst))
                                goto Error;
                            bin->op = OP_SUB_PTR_INT;
                        }
                    }
                    else {
                        bin->op = OP_ADD_PTR_INT;
                    }


                } else {
                    // LHS is not a pointer => the only valid case is Integral + Pointer
                    if (!is_integral_type(lhst) || bin->op != '+')
                        goto Error;

                    // In order to simplify code generation
                    // we will swap the arguments so that the pointer is LHS
                    std::swap(bin->lhs, bin->rhs);
                    bin->op = OP_ADD_PTR_INT;
                }

                // After this is all done, the type of the binary expression
                // is the same as the pointer type
                bin->type = lhst->nodetype == AST_POINTER_TYPE ? lhst : rhst;
                return bin->type;

Error:
                ctx.error({
                    .code = ERR_INCOMPATIBLE_TYPES,
                    .nodes = { bin->lhs, bin->rhs }
                });
                return nullptr;
            }


            // Handle regular human arithmetic between numbers
            else {
                if (implicit_cast(ctx, &bin->rhs, lhst))
                    return (bin->type = lhst);

                if (implicit_cast(ctx, &bin->lhs, rhst))
                    return (bin->type = rhst);
            }

            ctx.error({
                .code = ERR_INCOMPATIBLE_TYPES,
                .nodes = { lhst, rhst }
            });
            return nullptr;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* ma = (AST_MemberAccess*)node;
            if (ma->type)
                return ma->type;

            AST_Struct* s = (AST_Struct*)gettype(ctx, ma->lhs);
            MUST (s);
            if (s->nodetype != AST_STRUCT) {
                ctx.error({ .code = ERR_NO_SUCH_MEMBER, });
                return nullptr;
            }

            for (int i = 0; i < s->members.size; i++) {
                auto& member = s->members[i];
                if (!strcmp(member.name, ma->member_name)) {
                    ma->index = i;
                    ma->type = member.type;
                    return member.type;
                }
            }
            ctx.error({ .code = ERR_NO_SUCH_MEMBER, });
            return nullptr;
        }

        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;
            if (deref->type)
                return deref->type;

            AST_Type* inner_type = gettype(ctx, deref->ptr);
            MUST (inner_type);

            if (inner_type->nodetype != AST_POINTER_TYPE) {
                ctx.error({
                    .code = ERR_INVALID_DEREFERENCE,
                    .nodes = { deref },
                });
            }

            deref->type = ((AST_PointerType*)inner_type)->pointed_type;
            return deref->type;
        }

        case AST_ADDRESS_OF: {
            AST_AddressOf* addrof = (AST_AddressOf*)node;
            if (addrof->type)
                return addrof->type;

            AST_Type* inner_type = gettype(ctx, addrof->inner);
            MUST (inner_type);

            addrof->type = get_pointer_type(inner_type);
            return addrof->type;
        }

        default:
            printf("gettype is not supported/implemented for nodetype %d\n", node->nodetype);
            assert(0);
    }
}



bool typecheck(Context& ctx, AST_Node* node) {
    switch (node->nodetype) {
        case AST_BLOCK: {
            AST_Block* block = (AST_Block*)node;

            for (const auto& decl : block->ctx.declarations_arr) {
                if (decl.node->nodetype == AST_VAR)
                    MUST (typecheck(block->ctx, decl.node));
            }

            for (const auto& stmt : block->statements)
                MUST (typecheck(block->ctx, stmt));
            return true;
        }
        case AST_FN: {
            AST_Fn* fn = (AST_Fn*)node;

            MUST (gettype(ctx, fn));
            MUST (typecheck(fn->block.ctx, &fn->block));
            return true;
        }

        // TODO I should probably get rid of initial_value
        // and treat it like an assignment statment
        case AST_VAR:{
            AST_Var* var = (AST_Var*)node;

            MUST (must_be_type(ctx, var->type));

            if (var->initial_value) {
                AST_Type* rhst = gettype(ctx, var->initial_value);
                MUST (rhst);
                MUST (implicit_cast(ctx, &var->initial_value, var->type));
                return true;
            }

            return true;
        }
        case AST_RETURN: {
            // TODO RETURNTYPE child void functions right now inherit the returntype of the 
            // parent funciton if the child fn's return type is void
            // this is wrong
            AST_Return* ret = (AST_Return*)node;
            AST_Type* rettype = (AST_Type*)ctx.resolve("returntype");

            if (rettype) {
                if (!ret->value) {
                    ctx.error({
                        .code = ERR_INVALID_RETURN,
                    });
                    return false;
                }
                if (!implicit_cast(ctx, &ret->value, rettype))
                    return false;
            }
            else if (ret->value) {
                ctx.error({
                    .code = ERR_INVALID_RETURN,
                    .nodes = { ret->value, }
                });
                return false;
            }
            return true;
        }
        case AST_BINARY_OP:
        case AST_ASSIGNMENT:
        case AST_FN_CALL:
        case AST_MEMBER_ACCESS:
        case AST_DEREFERENCE:
        case AST_ADDRESS_OF:
        {
            return gettype(ctx, node);
        }
        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (typecheck(ctx, ifs->condition));
            MUST (typecheck(ctx, &ifs->then_block));
            return true;
        }
        case AST_STRUCT: {
            //AST_Struct* s = (AST_Struct*)node;
            //for (auto& member : s->members.entries) {
                // MUST(typecheck(ctx, member.type));
            //}
            return true;
        }
        default:
            assert(!"Not implemented");
    }
}

bool typecheck_all(Context& global) {
    for (auto& decl : global.declarations_arr)
        MUST (resolve_unresolved_references(&decl.node));

    for (auto& decl : global.declarations_arr)
        MUST (typecheck(global, decl.node));

    return true;
}
