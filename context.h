#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

struct AST_GlobalContext;
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

u32 map_hash(DeclarationKey key);
u32 map_equals(DeclarationKey lhs, DeclarationKey rhs);

struct AST_Fn; 
struct AST_Type; 
struct AST_PointerType; 
struct AST_ArrayType;

struct CompileTarget {
    u64 pointer_size;
    u64 coerce_target_size;
};

struct Namespace {
    map<DeclarationKey, AST_Node*> declarations;
    arr<Namespace*> used;

    bool finished_with_declarations;
};

struct AST_Context : AST_Node {
    map<DeclarationKey, AST_Node*> declarations;
    arr<AST_Node*> statements;

    AST_GlobalContext* global;
    AST_Context* parent;

    AST_Fn* fn; // the function that this context is a part of
    arr<AST_Context*> children;

    AST_Context(AST_Context* parent);
    AST_Context(AST_Context&) = delete;
    AST_Context(AST_Context&&) = delete;

    AST_Node* resolve(DeclarationKey key);
    bool declare(DeclarationKey key, AST_Node* value);
    void error(Error err);

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args);

    struct RecursiveDefinitionsIterator {
        i64 index = 0;
        arr<AST_Context*> remaining;
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

    AST_PointerType *get_pointer_type(AST_Type* pointed_type);
    AST_ArrayType   *get_array_type(AST_Type* base_type, u64 length);
    AST_FnType      *make_function_type_unique(AST_FnType* temp_type);
};

// Don't insert empty spaces here, it will waste memory
enum MessageType : u8 {
    MSG_NEW_DECLARATION = 0,

    MESSAGES_COUNT_PLUS_ONE
};

#define MESSAGES_COUNT (MESSAGES_COUNT_PLUS_ONE - 1)

struct Message {
    MessageType msgtype;
};

struct NewDeclarationMessage : Message {
    AST_Context *context;
    const char *name;
    AST_Node *node;
};

enum JobFlags : u32 {
    JOB_DONE = 0x01,
};

struct Job {
    // those two return true if the job is finished after the run/receive_message call returns
    virtual bool run() = 0;
    virtual bool receive_message(Message *msg);

    AST_GlobalContext *global;
    JobFlags flags = (JobFlags)0;

    arr<Job*> dependent_jobs;
    u32 dependencies_left = 0;

    void add_dependency(Job* dependency);
    void subscribe(MessageType msgtype); // TODO this won't scale - MessageType is too coarse

    Job(AST_GlobalContext *global);

    virtual std::wstring get_name();
};

struct AST_GlobalContext : AST_Context {
	arr<Error> errors;
    arr<AST_UnresolvedId*> unresolved;

    map<AST_Node*, AST_Node*> global_initial_values;

    map<const char*, AST_StringLiteral*> literals;

    linear_alloc allocator, temp_allocator;

    CompileTarget target = { 8, 8 };

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
    map<AST_Node*,  Location> definition_locations;
    map<AST_Node**, Location> reference_locations;

    // TODO DS This should be a hashset if someone ever makes one
    //
    // The reason those maps exist is to make sure function/pointer types are unique
    // Types MUST be comparable by checking their pointers with ==,
    // so we need those maps to make sure we don't create the same
    // composite type twice.
    map<AST_FnType*, AST_FnType*> fn_types_hash;
    map<AST_Type*, AST_PointerType*> pointer_types;

    inline AST_GlobalContext() : AST_Context(nullptr), subscribers(MESSAGES_COUNT) {
        for (u32 i = 0; i < MESSAGES_COUNT; i++)
            subscribers.push(arr<Job*>());
    }

    arr<Job*> ready_jobs;
    arr<arr<Job*>> subscribers;
    u32 jobs_count;

    void add_job(Job *job);
    bool run_jobs();
    void send_message(Message *msg);
};

template <typename T, typename ... Ts>
T* AST_Context::alloc(Ts &&...args) {
    T* buf = (T*)global->allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

template <typename T, typename ... Ts>
T* AST_Context::alloc_temp(Ts &&...args) {
    T* buf = (T*)global->temp_allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

struct ResolveJob : Job {
    AST_UnresolvedId *id;
    AST_Context *context;

    ResolveJob(AST_UnresolvedId *id, AST_Context *ctx);
    bool run() override;
    bool receive_message(Message *msg) override;
};

Location location_of(AST_Context& ctx, AST_Node** node);
bool parse_all(AST_Context& global);
bool parse_source_file(AST_Context& global, SourceFile& sf);

#endif // guard
