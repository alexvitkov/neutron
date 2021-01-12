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
        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            MUST (resolve_unresolved_references(&ret->value));
            break;
        }
        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (resolve_unresolved_references(&ifs->block));
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

AST_Type* gettype(Context& ctx, AST_Node* node) {
    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE:
            return &t_type;
        case AST_FN:
            assert(!"Function types aren't implemented");
        case AST_VAR:
        case AST_CAST:
        case AST_NUMBER:
            return ((AST_Value*)node)->type;
        case AST_FN_CALL: {
            AST_FnCall* fncall = (AST_FnCall*)node;

            AST_Fn* fn = (AST_Fn*)fncall->fn;
            assert(fn->nodetype == AST_FN);

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
                return &t_void;
            }
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
            return nullptr;
        }
        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;
            if (bin->type)
                return bin->type;

            AST_Type* lhst = gettype(ctx, bin->lhs);
            AST_Type* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            if (implicit_cast(ctx, &bin->rhs, lhst))
                return (bin->type = lhst);

            if (implicit_cast(ctx, &bin->lhs, rhst))
                return (bin->type = rhst);

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
        default:
            printf("gettype is not supported/implemented for nodetype %d\n", node->nodetype);
            assert(0);
    }
}


bool typecheck(Context& ctx, AST_Node* node) {
    switch (node->nodetype) {
        case AST_BLOCK: {
            AST_Block* block = (AST_Block*)node;

            // TODO this is a band aid
            // The real solution is to split AST_VAR
            // into AST_VAR_DECL and AST_VAR_REF
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
            return typecheck(fn->block.ctx, &fn->block);
        }
        // TODO I should probably get rid of initial_value
        // and treat it like an assignment statment
        case AST_VAR:{
            AST_Var* var = (AST_Var*)node;
            if (var->initial_value) {
                AST_Type* rhst = gettype(ctx, var->initial_value);
                MUST (rhst);
                MUST (implicit_cast(ctx, &var->initial_value, var->type));
                return true;
            }
        }
        case AST_RETURN: {
            // TODO child void functions right now inherit the returntype of the 
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
        {
            return gettype(ctx, node);
        }
        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (typecheck(ctx, ifs->condition));
            MUST (typecheck(ctx, ifs->condition));
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
