#ifndef TYPER_H
#define TYPER_H

#include "common.h"
#include "ast.h"

extern ASTPrimitiveType t_bool;
extern ASTPrimitiveType t_u8;
extern ASTPrimitiveType t_u16;
extern ASTPrimitiveType t_u32;
extern ASTPrimitiveType t_u64;
extern ASTPrimitiveType t_i8;
extern ASTPrimitiveType t_i16;
extern ASTPrimitiveType t_i32;
extern ASTPrimitiveType t_i64;
extern ASTPrimitiveType t_f32;
extern ASTPrimitiveType t_f64;
extern ASTPrimitiveType t_type;
extern ASTPrimitiveType t_void;

bool typecheck_all(Context& global);
ASTType* typecheck(Context& ctx, ASTNode* node);

#endif // guard
