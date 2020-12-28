#include "common.h"

bool Context::ok() {
	for (const auto& err : errors) {
	if (err.severity == Error::ERROR)
		return false;
	}
	return true;
}

bool Context::is_global() {
    return global == this;
}

void Error::print() {
	printf("error: %s\n", this->message);

}

bool Context::declare(const char* name, ASTNode* value) {
    std::string str(name);

    auto it = defines.find(str);
    if (it != defines.end()) {
        global->errors.push_back({ Error::PARSER, Error::ERROR, "redefinition" });
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
