#include "common.h"
#include "ast.h"
#include "error.h"

Context::Context(Context* parent)
    : parent(parent), global(parent ? parent->global : (GlobalContext*)this) { }


bool Context::ok() {
    for (const auto& err : global->errors) {
        if (err.severity == SEVERITY_FATAL)
            return false;
    }
    return true;
}

bool Context::is_global() {
    return global == this;
}

bool Context::declare(const char* name, ASTNode* value, Token nameToken) {
    // Throw an error if another value with the same name has been declared
    ASTNode* prev_decl;
    if (declarations_table.find(name, &prev_decl)) {
        error({
            .code = ERR_ALREADY_DEFINED,
            .tokens = { nameToken, declarations_meta[prev_decl].location }
        });
        return false;
    }

    declarations_arr.push({name, value});
    declarations_table.insert(name, value);
    declarations_meta.insert(value, {.location = nameToken});
    return true;
}

ASTNode* Context::resolve(const char* name) {
    ASTNode* node;
    if (!declarations_table.find(name, &node)) {
        if (parent)
            return parent->resolve(name);
        return nullptr;
    }
    return node;
}

ASTNode* Context::try_resolve(const char* name) {
    ASTNode* node = resolve(name);
    if (node)
        return node;
    else {
        ASTUnresolvedId* id = alloc_temp<ASTUnresolvedId>(name, *this);
        global->unresolved.push(id);
        return id;
    }
}

void Context::error(Error err) {
    global->errors.push(err);
}
