#include "common.h"
#include "ast.h"
#include "error.h"

Context::Context(Context* parent)
    : parent(parent), global(parent ? parent->global : (GlobalContext*)this) { }


bool Context::declare(const char* name, AST_Node* value, Token nameToken) {
    // Throw an error if another value with the same name has been declared
    AST_Node* prev_decl;
    if (declarations_table.find(name, &prev_decl)) {
        error({
            .code = ERR_ALREADY_DEFINED,
            .tokens = { nameToken },
            .nodes = { prev_decl }
        });
        return false;
    }

    declarations_arr.push({name, value});
    declarations_table.insert(name, value);
    return true;
}

AST_Node* Context::resolve(const char* name) {
    AST_Node* node;
    if (!declarations_table.find(name, &node)) {
        if (parent)
            return parent->resolve(name);
        return nullptr;
    }
    return node;
}

void Context::error(Error err) {
    global->errors.push(err);
}
