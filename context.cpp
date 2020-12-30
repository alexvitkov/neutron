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
    std::string str(name);

    auto it = defines.find(str);
    if (it != defines.end()) {
        // global->errors.push_back({ Error::PARSER, Error::ERROR, "redefinition" });
        error({
            .code = ERR_ALREADY_DEFINED,
        });
        return false;
    }

    defines.insert(std::make_pair<>(name, value));
    return true;
}

ASTNode* Context::resolve(const char* name) {
    std::string str(name);

    // TODO this should not be using std::string
    // nor std::unordered_map
    // we're making a lot of copies
    auto it = defines.find(str);
    if (it == defines.end()) {
        if (parent)
            return parent->resolve(name);
        return nullptr;

    }
    return it->second;
}

ASTNode* Context::resolve(char* name, int length) {
    char old = name[length];
    name[length] = 0;
    ASTNode* result = resolve(name);
    name[length] = old; 
    return result;
}

void Context::error(Error err) {
    global->errors.push_back(err);
}
