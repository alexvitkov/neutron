#include "common.h"
#include "ast.h"
#include "error.h"

Context::Context(Context* parent)
    : parent(parent), 
      global(parent ? parent->global : (GlobalContext*)this), 
      fn(parent ? parent->fn : nullptr) 
{ 
    if (parent)
        parent->children.push(this);
}

u32 map_hash(DeclarationKey key) {
    u32 hash = 0;
    if (key.name)
        hash = map_hash(key.name);

    hash ^= map_hash(key.fn_type);

    return hash;
}

u32 map_equals(DeclarationKey lhs, DeclarationKey rhs) {
    if (!lhs.name != !rhs.name)
        return false;

    if (lhs.name && rhs.name && strcmp(lhs.name, rhs.name))
        return false;

    return lhs.fn_type == rhs.fn_type;
}

bool Context::declare(DeclarationKey key, AST_Node* value) {
    // Throw an error if another value with the same name has been declared
    AST_Node* prev_decl;
    if (declarations.find(key, &prev_decl)) {
        error({
            .code = ERR_ALREADY_DEFINED,
            .nodes = { prev_decl, value }
        });
        return false;
    }

    declarations.insert(key, value);
    return true;
}

AST_Node* Context::resolve(DeclarationKey key) {
    AST_Node* node;
    if (!declarations.find(key, &node)) {
        if (parent)
            return parent->resolve(key);
        return nullptr;
    }
    return node;
}

void Context::error(Error err) {
    global->errors.push(err);
}
