#include "ast.h"
#include "resolve.h"
#include "cast.h"
#include "tir.h"
#include "tir_builtins.h"

#include <iostream>
#include <sstream>



CallResolveJob::CallResolveJob(AST_Context &ctx, AST_Call *call) 
    : Job(ctx.global), context(&ctx) , fncall(call)
{
    flags = (JobFlags)(flags | JOB_WAITING_MSG);
}

IdResolveJob::IdResolveJob(AST_Context &ctx, AST_UnresolvedId **id) 
    : Job(ctx.global), context(&ctx) , unresolved_id(id)
{
    flags = (JobFlags)(flags | JOB_WAITING_MSG);
}

OpResolveJob::OpResolveJob(AST_GlobalContext &ctx, AST_Call *call) 
    : Job(ctx), fncall(call)
{
    flags = (JobFlags)(flags | JOB_WAITING_MSG);
}



struct MatchCallJob : Job {
    AST_Context *context;
    AST_Fn      *fn;
    AST_Call    *fncall;
    int          prio;

    arr<AST_Value*> casted_args;

    u64 parent_job;

    inline MatchCallJob(AST_Context *ctx, AST_Fn *fn, AST_Call *call, JobOnCompleteCallback on_complete) 
        : Job(ctx->global), 
          context(ctx), 
          fn(fn), 
          fncall(call), 
          casted_args(fncall->args.size)
    {
        casted_args.size = fncall->args.size;
        this->on_complete = on_complete;
        prio = 0;
    }

    bool run(Message *msg) override {
        AST_FnType *fntype = fn->fntype();

        MUST (fntype->param_types.size >= fncall->args.size);
        if (!fntype->is_variadic)
            MUST (fntype->param_types.size == fncall->args.size);

        if (prio == 0) {
            prio = 1000;
            bool wait = false;
            for (u32 i = 0; i < fntype->param_types.size; i++) {
                AST_Type *paramtype = fntype->param_types[i];
                AST_Type *argtype = fncall->args[i]->type;

                CastJob cast_job(context, fncall->args[i], paramtype, [](Job *self, Job *parent) {
                    MatchCallJob *p = (MatchCallJob*)parent;
                    CastJob *c = (CastJob*)self;
                    p->casted_args[(u64)self->input] = (AST_Value*)self->result;
                    p->prio += c->prio;
                });

                cast_job.input = (void*)(u64)i;
                if (!run_child<MatchCallJob, CastJob>(cast_job, true)) // TODO kill the other cast jobs if this fails
                    wait = true;
            }
            if (wait)
                return false;
        } 
        
        return true;
    }

    std::wstring get_name() override {
        return L"MatchCallJob";
    }
};



bool CallResolveJob::spawn_match_job(AST_Fn *fn) {
    pending_matches++;

    MatchCallJob the_job(context, fn, this->fncall, [](Job *_self, Job *_parent) {
        MatchCallJob *self = (MatchCallJob*)_self;
        CallResolveJob *parent = (CallResolveJob*)_parent;

        parent->pending_matches --;

        if (parent->prio < self->prio) {
            if (!parent->new_args.size) {
                parent->new_args.realloc(parent->fncall->args.size);
                parent->new_args.size = (parent->fncall->args.size);
            }
            for (int i = 0; i < parent->fncall->args.size; i++)
                parent->new_args[i] = self->casted_args[i];

            parent->new_fn = self->fn;
            parent->prio   = self->prio;

        } else if (parent->prio == self->prio) {
            // TODO ERROR
            NOT_IMPLEMENTED();
        }

        if (parent->pending_matches == 0) {
            if (parent->context && parent->context->closed) {
                parent->context = parent->context->parent;
                parent->read_scope();
            }
        }
    });

    the_job.input = 0;
    if (run_child<CallResolveJob, MatchCallJob>(the_job, false)) // TODO JOB
        return true;

    the_job.input = (void*)1;
    return false;
}


DeclarationKey CallResolveJob::get_decl_key() {
    DeclarationKey key = {};
    if (fncall->fn) {
        key.name = ((AST_UnresolvedId*)fncall->fn)->name;
    } else {
        assert (fncall->op);
        key.op = fncall->op;
    }
    return key;
}

bool key_compatible(DeclarationKey &lhs, DeclarationKey &rhs) {
    MUST (lhs.name);
    MUST (rhs.name);
    MUST (!strcmp(lhs.name, rhs.name));
    return true;
}

RunJobResult CallResolveJob::read_scope() {
    if (!context) {
        if (prio > 0) {
            for (int i = 0; i < fncall->args.size; i++)
                fncall->args[i] = new_args[i];
            fncall->fn = new_fn;
            fncall->type = ((AST_FnType*)fncall->fn->type)->returntype;
            job_done();
            return RUN_DONE;
        } else {
            // TODO ERROR
            set_error_flag();
            return RUN_FAIL;
        }
    }

    bool heaped = false;
    CallResolveJob *self = this;
    DeclarationKey key = self->get_decl_key();

    for (auto &kvp : self->context->declarations)
        if (kvp.value IS AST_FN && key_compatible(key, kvp.key))
            if (!self->spawn_match_job((AST_Fn*)kvp.value) && !heaped) {
                heaped = true;
                self = (CallResolveJob*)heapify<CallResolveJob>()->job();
            }
    
    if (self->context) {
        if (pending_matches == 0 && self->context->closed) {
            self->context = self->context->parent;
            return self->read_scope();
        }
        if (!self->context->closed) {
            context->subscribers.push(heapify<CallResolveJob>());
        }
    } else {
        return self->read_scope();
    }
    return RUN_AGAIN;
}

bool IdResolveJob::run(Message *msg) {
Top:
    if (!msg) {
        if (!context) {
            // TODO ERROR
            set_error_flag();
            return false;
        }

        AST_Node *decl;
        if (context->declarations.find({ .name = (*unresolved_id)->name }, &decl)) {
            *(AST_Node**)unresolved_id = decl;
            return true;
        }   

        if (context->closed) {
            context = context->parent;
            return run(nullptr);
        } else {
            context->subscribers.push(heapify<CallResolveJob>());
            return RUN_AGAIN;
        }
    }

    switch (msg->msgtype) {
        case MSG_NEW_DECLARATION: {
            NewDeclarationMessage *nd = (NewDeclarationMessage*)msg;
            assert (nd->context == context);

            if (nd->key.name && !strcmp(nd->key.name, (*unresolved_id)->name)) {
                *(AST_Node**)unresolved_id = nd->node;
                return true;
            }
            return false;
        }
        case MSG_SCOPE_CLOSED: {
            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;
            assert(sc->scope == context);
            context = context->parent;

            msg = nullptr;
            goto Top;
        }
        default:
            UNREACHABLE;
    }
}

bool CallResolveJob::run(Message *msg) {
    CallResolveJob *self = (CallResolveJob*)this;
    if (!msg) {
        switch (self->read_scope()) {
            case RUN_DONE:
                return true;
            case RUN_FAIL:
                return false;
            case RUN_AGAIN:
                return false;
        }
    }

    switch (msg->msgtype) {
        case MSG_NEW_DECLARATION: {
            NewDeclarationMessage *decl = (NewDeclarationMessage*)msg;
            MUST (decl->context == context);

            DeclarationKey key = get_decl_key();

            if (key_compatible(key, decl->key)) {
                if (decl->node IS AST_FN)
                    spawn_match_job((AST_Fn*)decl->node);
                return false;
            }
        }

        case MSG_SCOPE_CLOSED: {
            ScopeClosedMessage *sc = (ScopeClosedMessage*)msg;

            MUST (pending_matches == 0);
            MUST (sc->scope == context);

            context = context->parent;
            switch (read_scope()) {
                case RUN_DONE:
                    return true;
                case RUN_AGAIN:
                case RUN_FAIL:
                    return false;
            }
        }

        default:
            UNREACHABLE;
    }
}

RunJobResult OpResolveJob::try_binary_builder(TIR_Builder *builder, AST_Type *targettype, bool cast_rhs) {

    struct Data {
        bool cast_rhs;
        AST_Value *other;
        TIR_Builder *builder;
    };

    pending_matches ++;
    CastJob the_cast(&global, fncall->args[cast_rhs ? 1 : 0], targettype, [](Job *_self, Job *_parent) {
        CastJob *self = (CastJob*)_self;
        OpResolveJob *parent = (OpResolveJob*)_parent;
        Data *data = (Data*)self->input;

        if (self->prio > parent->prio) {
            parent->prio = self->prio;
            parent->new_builder = data->builder;

            parent->new_lhs = (AST_Value*)self->result;
            parent->new_rhs = data->other;
            if (data->cast_rhs)
                std::swap(parent->new_lhs, parent->new_rhs);
        }

        parent->pending_matches --;
        if (parent->pending_matches == 0) {
            // TODO check if global is clsoed for new declarations
            parent->finish();
        }

    });

    // TODO LEAK
    Data *asdf = new Data();
    the_cast.input = asdf;
    asdf->cast_rhs = cast_rhs;
    asdf->other = this->fncall->args[cast_rhs ? 0 : 1];
    asdf->builder = builder;

    bool r = run_child<CastJob>(the_cast, false); // TODO JOB
    return RUN_AGAIN;
}


RunJobResult OpResolveJob::finish() {
    if (prio) {
        fncall->args[0] = new_lhs;
        fncall->args[1] = new_rhs;
        fncall->type    = new_builder->rettype;
        fncall->builder = new_builder;
        job_done();
        return RUN_DONE;
    } else {
        set_error_flag();
        return RUN_FAIL;
    }
}

bool OpResolveJob::run(Message *msg) {
    if (!msg) {
        switch (fncall->args.size) {
            case 2: {
                TIR_Builder *builder = nullptr;

                if (binary_builders.find( 
                    { fncall->op, fncall->args[0]->type, fncall->args[1]->type }, 
                    &builder)) 
                {
                    fncall->builder = builder;
                    fncall->type = builder->rettype;
                    job_done();
                    return RUN_DONE;
                }

                if (!builder) {
                    pending_matches++; // WORKAROUND
                    // TODO this is slow
                    for (auto & b : binary_builders) {
                        if (b.key.op != fncall->op)
                            continue;
                        if (b.key.lhs != fncall->args[0]->type && b.key.rhs != fncall->args[1]->type)
                            continue;

                        if (b.key.lhs == fncall->args[0]->type)
                            try_binary_builder(b.value, b.key.rhs, true);
                        else
                            try_binary_builder(b.value, b.key.lhs, false);
                    }
                    pending_matches--;
                }

                RunJobResult rr = finish();
                return rr == RUN_DONE;
            }
            case 1: { NOT_IMPLEMENTED(); }
            default: UNREACHABLE;
        }
    }

    switch (msg->msgtype) {
        case MSG_SCOPE_CLOSED: {
            NOT_IMPLEMENTED();
        }
        default:
            UNREACHABLE;
    }
};


std::wstring IdResolveJob::get_name() {
    std::wostringstream stream;
    stream << "IdResolveJob<" << unresolved_id << L">";
    return stream.str();
}

std::wstring CallResolveJob::get_name() {
    std::wostringstream stream;
    stream << "ResolveJob<" << fncall << L">";
    return stream.str();
}

std::wstring OpResolveJob::get_name() {
    std::wostringstream stream;
    stream << "OpResolveJob<" << fncall << L">";
    return stream.str();
}
