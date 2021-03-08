#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"
#include "error.h"

#include <atomic>

struct Job;
struct HeapJob;
typedef void (*JobOnCompleteCallback)(Job *self, Job *parent);

struct CastJob;
struct AST_Context;
struct AST_GlobalContext;

struct AST_Value;
struct AST_UnresolvedId;
struct AST_StringLiteral;
struct AST_Fn; 
struct AST_FnLike; 
struct AST_Call;
struct AST_FnType;
struct AST_Type; 
struct AST_PointerType; 
struct AST_ArrayType;



// VOLATILE - map_hash and map_equals MUST be updated after chaning this
struct DeclarationKey {
    const char* name;    // must come first, as we initialize the sturct like { the_name }
    TokenType op;        // used for operators
    AST_FnType* fn_type; // used for function overloads
    AST_StringLiteral* string_literal;
};

u32 map_hash(DeclarationKey key);
u32 map_equals(DeclarationKey &lhs, DeclarationKey &rhs);

u32 map_hash(AST_FnType *fntype);
bool map_equals(AST_FnType *lhs, AST_FnType *rhs);

struct CompileTarget {
    u64 pointer_size;
    u64 coerce_target_size;
};

struct Namespace {
    map<DeclarationKey, AST_Node*> declarations;
    arr<Namespace*> used;

    bool finished_with_declarations;
};

typedef bool (*CastJobMethod) (CastJob *self);

struct AST_Context : AST_Node {
    map<DeclarationKey, AST_Node*> declarations;
    bucketed_arr<AST_Node*> statements;

    AST_GlobalContext &global;
    AST_Context *parent;

    AST_FnLike* fn; // the function that this context is a part of
    arr<AST_Context*> children;

    AST_Context(AST_Context* parent);
    AST_Context(AST_Context&) = delete;
    AST_Context(AST_Context&&) = delete;

    AST_Node* resolve(DeclarationKey key);
    bool declare(DeclarationKey key, AST_Node* value, bool sendmsg);
    void error(Error err);

    // those will receive message for new declarations & scope closed
    arr<HeapJob*> subscribers;

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args);

    // The scope is closed when it's guarenteed that nothing else will be
    // declared inside it. If a scope is closed a ResolveJob waiting for a
    // new declaration inside the scope can stop, because that declaration
    // is not coming.
    bool closed = false;
    // The number of hanging declarations that prevent the scope from being closed.
    // When we parse a function for ex. this is incremented, and then when that
    // function is typechecked and declared this is decremented. When it then
    // reaches 0 after decrementing, the scope can be closed.
    int hanging_declarations = 0;

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


    void             decrement_hanging_declarations();
    void             close();
};

// Don't insert empty spaces here, it will waste memory
enum MessageType : u8 {
    MSG_NEW_DECLARATION = 0,
    MSG_SCOPE_CLOSED    = 1,
    MSG_FN_MATCHED      = 2,

    MESSAGES_COUNT,
};

struct Message {
    MessageType msgtype;
};

struct ScopeClosedMessage : Message {
    AST_Context *scope;
};

struct NewDeclarationMessage : Message {
    AST_Context *context;
    DeclarationKey key;
    AST_Node *node;
};

struct DependencyFinishedMessage : Message {
    Job *dependency;
};

enum JobFlags : u32 {
    JOB_DONE  = 0x01,
    JOB_ERROR = 0x02,
    JOB_SOFT  = 0x04,
    JOB_WAITING_MSG = 0x08,
};


struct AST_GlobalContext : AST_Context {
	arr<Error> errors;
    arr<AST_UnresolvedId*> unresolved;

    // When a function is parsed we cannot immediately declare it because
    // we don't know its type yet. This is not an issue for vars,
    // because they don't have overloads and are declared immediately
    struct FnToDeclare {
        AST_Context &scope;
        AST_Fn      *fn;
    };
    arr<FnToDeclare> fns_to_declare;

    map<const char*, AST_StringLiteral*> literals;

    linear_alloc allocator, temp_allocator;
    struct TIR_Context *tir_context;

    CompileTarget target = { 8, 8 };

    // there are two ways of storing information about a node's location
    // - definition_locations[node] tells us where a node is defined.
    // but that is referenced from a lot of different places in the AST and
    // we don't have a reference node, so 
    // - to track locations of references we use a pointer to the reference
    // the catch is that now that must be const as well
    map<AST_Node*,  Location> definition_locations;
    map<AST_Node**, Location> reference_locations;

    // function/pointer types must be unique
    // AST_Type*s MUST be comparable by checking their pointers with ==,
    // so we need those maps to make sure we don't create the same composite type twice.
    map<AST_FnType*, AST_FnType*> fn_types_hash; // TODO DS - thish should be a hashset
    map<AST_Type*, AST_PointerType*> pointer_types;

    arr<HeapJob*> ready_jobs;
    map<u64, HeapJob*> jobs_by_id;

    u32 jobs_count; 
    std::atomic<int> next_job_id = { 1 };

    AST_GlobalContext();

    void add_job(HeapJob *job);
    bool run_jobs();
    void send_message(arr<HeapJob*>& jobs, Message *msg);
};

struct HeapJob {
    inline Job *job() { return (Job*)_the_job; };
    u32 dependencies_left = 0;
    arr<u64> dependent_jobs;

    void add_dependency(HeapJob* dependency);

    char _the_job[0];
};

enum RunJobResult {
    RUN_FAIL = 0,
    RUN_DONE,
    RUN_AGAIN
};

struct Job {
    u64 id;
    void *input, *result;

    AST_GlobalContext &global;
    JobOnCompleteCallback on_complete = nullptr;
    JobFlags flags = (JobFlags)0;

    Job(AST_GlobalContext &global);
    void error(Error err);

    // returns true if the job is finished after the run call returns
    virtual bool run(Message *msg) = 0;
    virtual std::wstring get_name() = 0;


    template <typename JobT>
    HeapJob *heapify() {
        HeapJob *heap_job;

        if (!global.jobs_by_id.find(id, &heap_job)) {
            heap_job = (HeapJob*)malloc(sizeof(HeapJob) + sizeof(JobT));
            new (heap_job) HeapJob();
            new (heap_job->job()) (JobT) (std::move(*(JobT*)this));
            global.jobs_by_id[id] = heap_job;
        }

        return heap_job;
    }


    // Jobs can often be completed immediately
    // so allocating them on the heap often doesn't make sense.
    //
    // job on the stack can be run with this method, you need to pass it its type
    // If it can't be completed immediately it will be moved to the heap and
    // added to the job queue. in that case, the pointer on the heap is returned
    // so you can wait on it
    template <typename JobT>
    HeapJob *run_stackjob() {
        if (run(nullptr)) {
            if (debug_jobs)
                wcout << dim << "Finished stackjob " << resetstyle << get_name() << "\n";
            if (on_complete) {
                on_complete(this, nullptr);
            }
            flags = (JobFlags)(flags | JOB_DONE);
            return nullptr;
        }

        if (flags & JOB_ERROR) {
            return nullptr;
        }

        HeapJob *heap_job = heapify<JobT>();
        global.add_job(heap_job);

        return heap_job;
    }

    template <typename MyT, typename ChildT>
    bool run_child(ChildT &child) {
        if (child.run(nullptr)) {
            if (debug_jobs) {

                HeapJob *hj;
                if (global.jobs_by_id.find(child.id, &hj)) {
                    wcout << red << "Stackjob finished but was already heapified:" << resetstyle << child.get_name() << "\n";
                    wcout.flush();
                    assert(!"ASDFASASFAFSFA");
                }

                wcout << dim << "Finished stackjob " << resetstyle << child.get_name() << "\n";
            }
            if (child.on_complete) {
                child.on_complete(&child, this);
            }
            child.flags = (JobFlags)(flags | JOB_DONE);
            return true;
        }

        if (child.flags & JOB_ERROR) {
            set_error_flag();
            return false;
        }

        HeapJob *child_heap_job = child.template heapify<ChildT>();
        HeapJob *this_heap_hob  = heapify<MyT>();
        this_heap_hob->add_dependency(child_heap_job);
        global.add_job(this_heap_hob);

        return false;
    }


    inline void set_error_flag() {
        flags = (JobFlags)(flags | JOB_ERROR);
    }

    inline void job_done() {
        flags = (JobFlags)((flags | JOB_DONE) & ~JOB_WAITING_MSG);
    }
};

template <typename T, typename ... Ts>
T* AST_Context::alloc(Ts &&...args) {
    T* buf = (T*)global.allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

template <typename T, typename ... Ts>
T* AST_Context::alloc_temp(Ts &&...args) {
    T* buf = (T*)global.temp_allocator.alloc(sizeof(T));
    new (buf) T (args...);
    return buf;
}

struct IdResolveJob : Job {
    AST_UnresolvedId **unresolved_id;
    AST_Context       *context;

    IdResolveJob(AST_Context &ctx, AST_UnresolvedId **id);

    bool run(Message *msg) override;
    std::wstring get_name() override;
};

struct CallResolveJob : Job {
    AST_Call          *fncall;
    AST_Context       *context;

    int pending_matches = 0;
    int prio = 0;

    arr<AST_Value*> new_args;
    AST_Fn         *new_fn;

    CallResolveJob(AST_Context &ctx, AST_Call *call);
    bool run(Message *msg) override;
    std::wstring get_name() override;

    bool spawn_match_job(AST_Fn *fn);
    DeclarationKey get_decl_key();

    bool jump_to_parent_scope_if_needed();
    RunJobResult read_scope();

};

struct JobGroup : Job {
    std::wstring name;

    bool run(Message *msg) override;
    JobGroup(AST_GlobalContext &ctx, std::wstring name);
    std::wstring get_name() override;
};

Location location_of(AST_Context& ctx, AST_Node** node);
bool parse_all(AST_Context& global);
bool parse_source_file(AST_Context& global, SourceFile& sf);

#define WAIT(job, mytype, jobtype, ...)              \
{                                                \
    HeapJob *heap_job = job.run_stackjob<jobtype>(); \
    if (heap_job) {                              \
        HeapJob *this_heap_job = heapify<mytype>();  \
        this_heap_job->add_dependency(heap_job); \
        __VA_ARGS__                              \
        return false;                            \
    } else if (job.flags & JOB_ERROR) {          \
        set_error_flag();                        \
        return false;                            \
    }                                            \
}

#endif // guard
