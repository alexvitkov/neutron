#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

struct GlobalContext;
struct AST_UnresolvedId;

struct Context {
    struct NameNodePair {
        const char* name;
        AST_Node* node;
    };

    struct DeclMeta {
        Token location;
    };

    arr<NameNodePair> declarations_arr;
    map<const char*, AST_Node*> declarations_table;
    map<AST_Node*, DeclMeta> declarations_meta;

    GlobalContext* global;
    Context* parent;

	bool ok();
    bool is_global();

    // Returns AST_Node* or null on failure
    AST_Node* resolve(const char* name);

    // Returns AST_Node* or creates a new ASTUnresolveNode* on failure
    AST_Node* try_resolve(const char* name);

    bool declare(const char* name, AST_Node* value, Token nameToken);

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
    arr<AST_UnresolvedId*> unresolved;
    inline GlobalContext() : Context(nullptr) {}
};

bool parse_all(Context& global);

#endif // guard
