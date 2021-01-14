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

    arr<NameNodePair> declarations_arr;
    map<const char*, AST_Node*> declarations_table;

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

    map<AST_Node*, Location> node_locations;

    map<AST_Node**, Location> node_ptr_locations;

    inline GlobalContext() : Context(nullptr) {}
};

Location location_of(Context& ctx, AST_Node** node);
bool parse_all(Context& global);

#endif // guard
