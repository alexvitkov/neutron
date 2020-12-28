#include "typer.h"

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

ASTType* typecheck(Context& ctx, ASTNode* node);

bool implicit_cast(Context& ctx, ASTNode** dst, ASTType* type) {
    ASTType* dstt = typecheck(ctx, *dst);
    if (!dstt)
        return false;

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
        *dst = ctx.alloc<ASTCast>(r, *dst);
        return true;
    }
    return false;
}


ASTType* typecheck(Context& ctx, ASTNode* node) {
    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE:
            return &t_type;
        case AST_FN: {
            ASTFn* fn = (ASTFn*)node;
            for (const auto& stmt : fn->block->statements) {
                if (!typecheck(*fn->block->ctx, stmt))
                    return nullptr;
            }
            return &t_fn;
        }
        case AST_VAR: {
            return ((ASTVar*)node)->type;
        }
        case AST_CAST: {
            return ((ASTCast*)node)->newtype;
        }
        case AST_RETURN: {
            ASTReturn* ret = (ASTReturn*)node;
            ASTType* rettype = (ASTType*)ctx.resolve("returntype");
            if (!implicit_cast(ctx, &ret->value, rettype)) {
                return nullptr;
            }
            return rettype;
        }
        case AST_BINARY_OP: {
            ASTBinaryOp* bin = (ASTBinaryOp*)node;

            ASTType* lhst = typecheck(ctx, bin->lhs);
            ASTType* rhst = typecheck(ctx, bin->rhs);

            if (!lhst || !rhst) {
                ctx.global->errors.push_back({Error::TYPER, Error::ERROR, "incompatible types"});
                return nullptr;
            }
            
            if (implicit_cast(ctx, &bin->rhs, lhst))
                return lhst;

            if (!(prec[bin->op] & ASSIGNMENT) && implicit_cast(ctx, &bin->lhs, rhst))
                return rhst;

            ctx.global->errors.push_back({Error::TYPER, Error::ERROR, "incompatible types"});
            return nullptr;
        }
        case AST_NUMBER: {
            ASTNumber* num = (ASTNumber*)node;
            return num->type;
        }
        default: {
                ctx.global->errors.push_back({Error::TYPER, Error::ERROR, "unsupported node type"});
                return nullptr;
        }
    }
}

bool typecheck_all(Context& global) {
    for (auto& decl : global.defines) {
        if (!typecheck(global, decl.second))
            return false;
    }
    return true;
}
