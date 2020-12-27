#ifndef AST_H
#define AST_H

#include "common.h"
#include <iostream>

enum ASTNodeType {
	AST_NONE = 0,
	AST_TYPE,
    AST_FN,
};

struct ASTNode {
	ASTNodeType nodetype;
};

struct ASTType {
	ASTNodeType nodetype;
    const char* name;
};

struct TypeList {
    struct Entry {
        const char* name;
        ASTType* value;
    };
    std::vector<Entry> entries;
};

struct ASTFn {
	ASTNodeType nodetype;
    char* name;
    TypeList args;
};

std::ostream& operator<<(std::ostream& o, ASTNode* node);
std::ostream& operator<<(std::ostream& o, ASTType* node);
std::ostream& operator<<(std::ostream& o, ASTFn* node);

#endif // guard
