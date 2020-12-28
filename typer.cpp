#include "typer.h"

ASTPrimitiveType t_u8  { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_UNSIGNED, .size = 1, .name = "u8"  };
ASTPrimitiveType t_u16 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_UNSIGNED, .size = 2, .name = "u16" };
ASTPrimitiveType t_u32 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_UNSIGNED, .size = 4, .name = "u32" };
ASTPrimitiveType t_u64 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_UNSIGNED, .size = 8, .name = "u64" };
ASTPrimitiveType t_i8  { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_SIGNED,   .size = 1, .name = "i8"  };
ASTPrimitiveType t_i16 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_SIGNED,   .size = 2, .name = "i16" };
ASTPrimitiveType t_i32 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_SIGNED,   .size = 4, .name = "i32" };
ASTPrimitiveType t_i64 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_SIGNED,   .size = 8, .name = "i64" };
ASTPrimitiveType t_f32 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_FLOAT,    .size = 4, .name = "f32" };
ASTPrimitiveType t_f64 { .nodetype = AST_PRIMITIVE_TYPE, .kind = PRIMITIVE_FLOAT,    .size = 8, .name = "f64" };
