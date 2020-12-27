#include "typer.h"

std::vector<Type> defined_types;

ASTType t_u8  { .nodetype = AST_TYPE, .name = "u8"  };
ASTType t_u16 { .nodetype = AST_TYPE, .name = "u16" };
ASTType t_u32 { .nodetype = AST_TYPE, .name = "u32" };
ASTType t_u64 { .nodetype = AST_TYPE, .name = "u64" };
ASTType t_i8  { .nodetype = AST_TYPE, .name = "i8"  };
ASTType t_i16 { .nodetype = AST_TYPE, .name = "i16" };
ASTType t_i32 { .nodetype = AST_TYPE, .name = "i32" };
ASTType t_i64 { .nodetype = AST_TYPE, .name = "i64" };
ASTType t_f32 { .nodetype = AST_TYPE, .name = "f32" };
ASTType t_f64 { .nodetype = AST_TYPE, .name = "f64" };

// TODO pointers to Type should be permanent
// vector is a really bad fit for this
void init_typer() {
	defined_types.reserve(10000);
	defined_types.push_back({ .kind = PRIMITIVE_UNSIGNED, .size = 1 }); // u8
	defined_types.push_back({ .kind = PRIMITIVE_UNSIGNED, .size = 8 }); // u64
	defined_types.push_back({ .kind = PRIMITIVE_FLOAT, .size = 8 });    // f64
}

