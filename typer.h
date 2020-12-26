#ifndef TYPER_H
#define TYPER_H

#include "common.h"
#include <vector>

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

#endif // guard
