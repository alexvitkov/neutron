#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

struct GlobalContext;
struct AST_UnresolvedId;
struct AST_StringLiteral;

struct Context {
    struct NameNodePair {
        const char* name;
        AST_Node* node;
    };

    arr<NameNodePair> declarations_arr;
    map<const char*, AST_Node*> declarations_table;

    GlobalContext* global;
    Context* parent;

    map<const char*, AST_StringLiteral*> literals;

    Context(Context* parent);
    Context(Context&) = delete;
    Context(Context&&) = delete;

    AST_Node* resolve(const char* name);
    bool declare(const char* name, AST_Node* value, Token nameToken);
    void error(Error err);

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args);

    template <typename T, typename ... Ts>
    T* alloc_temp(Ts &&...args);
};

struct GlobalContext : Context {
	arr<Error> errors;
    arr<AST_UnresolvedId*> unresolved;
    linear_alloc allocator, temp_allocator;

    map<AST_Node*, Location> node_locations;

    map<AST_Node**, Location> node_ptr_locations;

    inline GlobalContext() : Context(nullptr) {}
};

template <typename T, typename ... Ts>
T* Context::alloc(Ts &&...args) {
    T* buf = (T*)global->allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

template <typename T, typename ... Ts>
T* Context::alloc_temp(Ts &&...args) {
    T* buf = (T*)global->temp_allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

Location location_of(Context& ctx, AST_Node** node);
bool parse_all(Context& global);

#endif // guard
