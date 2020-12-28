#include "typer.h"

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
