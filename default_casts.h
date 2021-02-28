#ifndef DEFAULT_CASTS_H
#define DEFAULT_CASTS_H

#include "context.h"
#include "number.h"
#include "ast.h"
#include "typer.h"

struct CastJob : Job {
    AST_Value  *source;
    AST_Value **result;
    bool (*run_fn) (CastJob *self);

    inline CastJob(AST_GlobalContext &global, AST_Value *source, AST_Value **result, bool (*run_fn) (CastJob*)) 
        : Job(global), source(source), result(result), run_fn(run_fn) {}

    bool run(Message *msg) override;
    std::wstring get_name() override;
};

bool number_literal_to_u64(CastJob *self);
bool number_literal_to_u32(CastJob *self);
bool number_literal_to_u16(CastJob *self);
bool number_literal_to_u8 (CastJob *self);

#endif // guard
