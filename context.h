#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

struct GlobalContext;
struct AST_UnresolvedId;
struct AST_FnType;
struct AST_StringLiteral;

// VOLATILE If you drastically change this, you will have to change the map_hash and map_equals functions defined in context.cpp
struct DeclarationKey {
    // This must come first, as when we sometimes initialize the DeclarationKey like this: { the_name }
    const char* name;
    
    union {
        // For functions we can have two fns with the same name but different signatures.
        AST_FnType* fn_type;

        // String literals get translated to nameless static arrays,
        // that we index by the AST_StringLiteral that is responsible for making them
        AST_StringLiteral* string_literal;
    };
};

struct Context {
    map<DeclarationKey, AST_Node*> declarations;

    GlobalContext* global;
    Context* parent;

    map<const char*, AST_StringLiteral*> literals;

    Context(Context* parent);
    Context(Context&) = delete;
    Context(Context&&) = delete;

    AST_Node* resolve(DeclarationKey key);
    bool declare(DeclarationKey key, AST_Node* value);
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
