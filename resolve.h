#ifndef RESOLVE_H
#define RESOLVE_H

#include "context.h"

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




struct OpResolveJob : Job {
    AST_Call          *fncall;

    AST_Value *new_lhs, *new_rhs;
    struct TIR_Builder *new_builder;
    int prio = 0;
    int pending_matches = 0;

    OpResolveJob(AST_GlobalContext &global, AST_Call *call);
    bool run(Message *msg) override;
    std::wstring get_name() override;

    RunJobResult try_binary_builder(struct TIR_Builder *builder, AST_Type *target_type, bool cast_rhs);
    RunJobResult finish();
};


#endif
