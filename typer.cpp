#include "typer.h"
#include "ast.h"
#include "error.h"

ASTPrimitiveType t_bool (PRIMITIVE_BOOL,    1, "bool");
ASTPrimitiveType t_u8  (PRIMITIVE_UNSIGNED, 1, "u8");
ASTPrimitiveType t_u16 (PRIMITIVE_UNSIGNED, 2, "u16");
ASTPrimitiveType t_u32 (PRIMITIVE_UNSIGNED, 4, "u32");
ASTPrimitiveType t_u64 (PRIMITIVE_UNSIGNED, 8, "u64");
ASTPrimitiveType t_i8  (PRIMITIVE_SIGNED,   1, "i8");
ASTPrimitiveType t_i16 (PRIMITIVE_SIGNED,   2, "i16");
ASTPrimitiveType t_i32 (PRIMITIVE_SIGNED,   4, "i32");
ASTPrimitiveType t_i64 (PRIMITIVE_SIGNED,   8, "i64");
ASTPrimitiveType t_f32 (PRIMITIVE_FLOAT,    4, "f32");
ASTPrimitiveType t_f64 (PRIMITIVE_FLOAT,    8, "f64");
ASTPrimitiveType t_type (PRIMITIVE_TYPE,    0, "type");
ASTPrimitiveType t_void (PRIMITIVE_TYPE,    0, "void");

bool implicit_cast(Context& ctx, ASTNode** dst, ASTType* type) {
    ASTType* dstt = gettype(ctx, *dst);
    MUST (dstt);

    if (dstt == type) 
        return true;

    if (dstt->nodetype != AST_PRIMITIVE_TYPE || type->nodetype != AST_PRIMITIVE_TYPE)
        return false;

    ASTPrimitiveType* l = (ASTPrimitiveType*)dstt;
    ASTPrimitiveType* r = (ASTPrimitiveType*)type;

    if (l == &t_bool
            || (l->kind == r->kind && l->size < r->size)
            || (l->kind == PRIMITIVE_UNSIGNED && r->kind == PRIMITIVE_SIGNED && l->size < r->size))
    {
        if ((*dst)->nodetype == AST_NUMBER) {
            ((ASTNumber*)(*dst))->type = type;
        }
        else {
            *dst = ctx.alloc<ASTCast>(r, *dst);
        }
        return true;
    }
    return false;
}

bool is_valid_lvalue(ASTNode* expr) {
    switch (expr->nodetype) {
        case AST_VAR:
            return true;
        case AST_MEMBER_ACCESS:
            return true;
        default:
            return false;
    }
}

ASTType* gettype(Context& ctx, ASTNode* node) {
    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE:
            return &t_type;
        case AST_FN:
            assert(!"Function types aren't implemented");
        case AST_VAR:
            return ((ASTVar*)node)->type;
        case AST_CAST:
            return ((ASTCast*)node)->newtype;
        case AST_NUMBER:
            return ((ASTNumber*)node)->type;
        case AST_FN_CALL: {
            ASTFnCall* fncall = (ASTFnCall*)node;
            assert(fncall->fn->nodetype == AST_FN);
            ASTFn* fn = (ASTFn*)fncall->fn;
            ASTType* returntype = (ASTType*)fn->block.ctx.resolve("returntype");

            // TODO type check the arguments

            if (!returntype) {
                return &t_void;
            }
            return returntype;
        }
        case AST_ASSIGNMENT: {
            ASTBinaryOp* bin = (ASTBinaryOp*)node;
            if (bin->type)
                return bin->type;

            ASTType* lhst = gettype(ctx, bin->lhs);
            ASTType* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            if (!is_valid_lvalue(bin->lhs)) {
                ctx.error({ .code = ERR_NOT_AN_LVALUE, .nodes = { lhst, } });
                return nullptr;
            }

            if (implicit_cast(ctx, &bin->rhs, lhst))
                return (bin->type = rhst);
            break;
        }
        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)node;
            if (bin->type)
                return bin->type;

            ASTType* lhst = gettype(ctx, bin->lhs);
            ASTType* rhst = gettype(ctx, bin->rhs);
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
            ASTMemberAccess* ma = (ASTMemberAccess*)node;
            if (ma->type)
                return ma->type;

            ASTStruct* s = (ASTStruct*)gettype(ctx, ma->lhs);
            MUST (s);
            if (s->nodetype != AST_STRUCT) {
                ctx.error({ .code = ERR_NO_SUCH_MEMBER, });
                return nullptr;
            }

            for (auto& member : s->members.entries) {
                if (!strcmp(member.name, ma->member_name)) {
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


bool typecheck(Context& ctx, ASTNode* node) {
    switch (node->nodetype) {
        case AST_BLOCK: {
            ASTBlock* block = (ASTBlock*)node;
            for (const auto& stmt : block->statements)
                MUST (typecheck(block->ctx, stmt));
            return true;
        }
        case AST_FN: {
            ASTFn* fn = (ASTFn*)node;
            return typecheck(fn->block.ctx, &fn->block);
        }
        case AST_RETURN: {
            // TODO child void functions right now inherit the returntype of the 
            // parent funciton if the child fn's return type is void
            // this is wrong
            ASTReturn* ret = (ASTReturn*)node;
            ASTType* rettype = (ASTType*)ctx.resolve("returntype");

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
            ASTIf* ifs = (ASTIf*)node;
            MUST (typecheck(ctx, ifs->condition));
            MUST (typecheck(ctx, ifs->condition));
            return true;
        }
        case AST_STRUCT: {
            //ASTStruct* s = (ASTStruct*)node;
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
        MUST (typecheck(global, decl.node));
    return true;
}
