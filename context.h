#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "ds.h"

struct GlobalContext;

struct Context {
    struct NameNodePair {
        const char* name;
        ASTNode* node;
    };
    arr<NameNodePair> defines_arr;
    strtable<ASTNode*> defines_table;

    GlobalContext* global;
    Context* parent;

	bool ok();
    bool is_global();

    ASTNode* resolve(const char* name);
    ASTNode* resolve(char* name, int length);
    bool declare(const char* name, ASTNode* value);

    inline Context(Context* parent)
        : parent(parent), global(parent ? parent->global : (GlobalContext*)this) { }

    Context(Context&) = delete;
    Context(Context&&) = delete;

    template <typename T, typename ... Ts>
    T* alloc(Ts &&...args) {
        T* buf = (T*)malloc(sizeof(T));
        new (buf) T (args...);
        return buf;
    }

    void error(Error err);
};

struct GlobalContext : Context {
	arr<Error> errors;
    inline GlobalContext() : Context(nullptr) {}
};

bool parse_all_files(Context& global);

#endif // guard
