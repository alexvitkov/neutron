#include "ast.h"

void print(std::ostream& o, TypeList& tl) {
    for (const auto& entry : tl.entries) {
        o << entry.name << ": " << entry.value << ", ";
    }
    o << "\b\b \b"; // clear the last comma
}

std::ostream& operator<<(std::ostream& o, ASTNode* node) {
    switch (node->nodetype) {
        case AST_TYPE: return o << (ASTFn*)node;
        case AST_FN:   return o << (ASTType*)node;
        default:       return o << '[' << node->nodetype << ']';
    }
}

std::ostream& operator<<(std::ostream& o, ASTFn* node) {
    ASTFn* fn = (ASTFn*)node;
    o << "fn ";
    if (fn->name)
        o << fn->name;
    o << '(';
    print(o, fn->args);
    o << ") { }";
    return o;
}

std::ostream& operator<<(std::ostream& o, ASTType* node) {
    if (node->name)
        o << node->name;
    else
        o << "[TYPE]";
    return o;
}
