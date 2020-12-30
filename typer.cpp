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
ASTPrimitiveType t_fn   (PRIMITIVE_TYPE,    0, "fntype"); // TODO remove this
ASTPrimitiveType t_void (PRIMITIVE_TYPE,    0, "void");

bool implicit_cast(Context& ctx, ASTNode** dst, ASTType* type) {
    ASTType* dstt = gettype(ctx, *dst);
    MUST(dstt);

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
        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)node;
            if (bin->type)
                return bin->type;

            ASTType* lhst = gettype(ctx, bin->lhs);
            ASTType* rhst = gettype(ctx, bin->rhs);
            MUST(lhst && rhst);

            if (implicit_cast(ctx, &bin->rhs, lhst))
                return (bin->type = lhst);
            if (!(prec[bin->op] & ASSIGNMENT) && implicit_cast(ctx, &bin->lhs, rhst))
                return (bin->type = rhst);

            ctx.error({
                .code = ERR_INCOMPATIBLE_TYPES,
                .nodes = {
                    lhst,
                    rhst
                }
            });
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
                MUST(typecheck(block->ctx, stmt));
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
        case AST_BINARY_OP: {
            return gettype(ctx, node);
        }
        case AST_IF: {
            ASTIf* ifs = (ASTIf*)node;
            MUST(typecheck(ctx, ifs->condition));
            MUST(typecheck(ctx, ifs->condition));
            return true;
        }
        default:
            return true;
    }
}

bool typecheck_all(Context& global) {
    for (auto& decl : global.defines)
        MUST(typecheck(global, decl.second));
    return true;
}
