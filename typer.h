#ifndef TYPER_H
#define TYPER_H

#include "common.h"
#include "ast.h"

enum TypeKind : u8 {
	PRIMITIVE_SIGNED,
	PRIMITIVE_UNSIGNED,
	PRIMITIVE_FLOAT,
	STRUCT
};


struct Type;

struct Element {
	const Type* type; 
	const char* name;
};

struct Type {
	ASTNodeType nodetype = AST_TYPE;
	TypeKind kind;
	u32 size;
	std::vector<Element> elements;
};

extern std::vector<Type> defined_types;

void init_typer();

extern ASTType t_u8;
extern ASTType t_u16;
extern ASTType t_u32;
extern ASTType t_u64;
extern ASTType t_i8;
extern ASTType t_i16;
extern ASTType t_i32;
extern ASTType t_i64;
extern ASTType t_f32;
extern ASTType t_f64;

#endif // guard
