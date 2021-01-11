#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

struct GlobalContext;
struct ASTUnresolvedId;

struct Context {
    struct NameNodePair {
        const char* name;
        ASTNode* node;
    };

    struct DeclMeta {
        Token location;
    };

    arr<NameNodePair> declarations_arr;
    map<const char*, ASTNode*> declarations_table;
    map<ASTNode*, DeclMeta> declarations_meta;

    GlobalContext* global;
    Context* parent;

	bool ok();
    bool is_global();

    // Returns ASTNode* or null on failure
    ASTNode* resolve(const char* name);

    // Returns ASTNode* or creates a new ASTUnresolveNode* on failure
    ASTNode* try_resolve(const char* name);

    bool declare(const char* name, ASTNode* value, Token nameToken);

    Context(Context* parent);
    
    Context(Context&) = delete;
    Context(Context&&) = delete;

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args) {
        T* buf = (T*)malloc(sizeof(T));
        new (buf) T (args...);
        return buf;
    }

    template <typename T, typename ... Ts>
    T* alloc_temp(Ts &&...args) {
        T* buf = (T*)malloc(sizeof(T));
        new (buf) T (args...);
        return buf;
    }

    void error(Error err);
};

struct GlobalContext : Context {
	arr<Error> errors;
    arr<ASTUnresolvedId*> unresolved;
    inline GlobalContext() : Context(nullptr) {}
};

bool parse_all(Context& global);

#endif // guard
