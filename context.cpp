#include "common.h"
#include "error.h"

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

bool Context::declare(const char* name, ASTNode* value) {
    ASTNode* node = defines_table.find(name);
    if (node) {
        error({
            .code = ERR_ALREADY_DEFINED,
        });
        return false;
    }

    defines_arr.push({name, value});
    defines_table.insert(name, value);
    return true;
}

ASTNode* Context::resolve(const char* name) {
    ASTNode* node = defines_table.find(name);
    if (!node) {
        if (parent)
            return parent->resolve(name);
        return nullptr;
    }
    return node;
}

ASTNode* Context::resolve(char* name, int len) {
    ASTNode* node = defines_table.find(name, len);
    if (!node) {
        if (parent)
            return parent->resolve(name, len);
        return nullptr;
    }
    return node;
}

void Context::error(Error err) {
    global->errors.push(err);
}
