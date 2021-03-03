#ifndef CAST_H
#define CAST_H

#include "context.h"
#include "number.h"
#include "ast.h"
#include "typer.h"



struct CastJob : Job {
    AST_Value *src;
    AST_Value *result;
    AST_Type  *dsttype;
    int prio;

    CastJob(AST_Context *ctx, AST_Value *value, AST_Type *dsttype);
    bool run(Message *msg) override;
    std::wstring get_name() override;
};


#endif // guard
