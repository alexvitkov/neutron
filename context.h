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

struct AST_Fn;

struct Context {
    map<DeclarationKey, AST_Node*> declarations;

    GlobalContext* global;
    Context* parent;

    AST_Fn* fn; // the function that this context is a part of
    arr<Context*> children;

    Context(Context* parent);
    Context(Context&) = delete;
    Context(Context&&) = delete;

    AST_Node* resolve(DeclarationKey key);
    bool declare(DeclarationKey key, AST_Node* value);
    void error(Error err);

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args);

    struct RecursiveDefinitionsIterator {
        i64 index = 0;
        arr<Context*> remaining;
    };

    // In the temp allocator we store:
    //    1. AST_UnresolvedIds
    //           When we hit an identifier we create a AST_UnresolvedId node
    //       The typer runs a pass (resolve_unresolved_reference), which replaces those
    //       with whatever those identifiers happen to refer to
    //
    //    2. Temporary function types
    //           When we parse the function we gradually fill out its type
    //       It can later on turn out that two functions have the same type, so we have two different
    //       type nodes that mean the same function type.
    //           This is is bad, as we check type equality bo comparing their pointers
    //       So before the typing stage we run another pass on all the functions that looks for 
    //       (see make_function_type_unique)
    //
    //    After those passes, the valeus allocated here are no longer relevant and they're released
    //    If something is only needed until before the typing stage, you should allocate it here
    template <typename T, typename ... Ts>
    T* alloc_temp(Ts &&...args);
};

struct GlobalContext : Context {
	arr<Error> errors;
    arr<AST_UnresolvedId*> unresolved;

    map<const char*, AST_StringLiteral*> literals;

    linear_alloc allocator, temp_allocator;

    // This is silly, but there are two ways of storing information about a node's location
    //
    // definition_locations[node] tells us where a node is defined
    //
    // However that node may be referenced from a lot of different places in the tree
    // We don't have a Reference node, we just slap a pointer to the original node
    // when we need to reference it
    //
    // The issue is that we still need location info for those references to print nice errors
    // if "AST_Node* some_node = ..." is referencing some node,
    // the address of the 'some_node' pointer itself is what we use as a key in reference_locations
    map<AST_Node*, Location> definition_locations;
    map<AST_Node**, Location> reference_locations;

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
bool parse_source_file(Context& global, SourceFile& sf);

#endif // guard
