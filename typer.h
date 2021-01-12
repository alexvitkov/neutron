#ifndef TYPER_H
#define TYPER_H

#include "common.h"
#include "ast.h"

extern AST_PrimitiveType t_bool;
extern AST_PrimitiveType t_u8;
extern AST_PrimitiveType t_u16;
extern AST_PrimitiveType t_u32;
extern AST_PrimitiveType t_u64;
extern AST_PrimitiveType t_i8;
extern AST_PrimitiveType t_i16;
extern AST_PrimitiveType t_i32;
extern AST_PrimitiveType t_i64;
extern AST_PrimitiveType t_f32;
extern AST_PrimitiveType t_f64;
extern AST_PrimitiveType t_type;
extern AST_PrimitiveType t_void;

bool typecheck_all(Context& global);
AST_Type* gettype(Context& ctx, AST_Node* node);

#endif // guard
