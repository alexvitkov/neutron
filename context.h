#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"

#include <unordered_map>
#include <string>

struct Context {
	std::unordered_map<std::string, ASTNode*> defines;
};

#endif // guard
