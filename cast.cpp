#include "cast.h"
#include "tir.h"
#include "tir_builtins.h"
#include <iostream>
#include <sstream>


u32 map_hash(TypePair pair) { return map_hash(pair.lhs) ^ (map_hash(pair.rhs) << 1); }
bool map_equals(TypePair a, TypePair b) { return a.lhs == b.lhs && a.rhs == b.rhs; }


CastJob::CastJob(AST_Context *ctx, AST_Value *value, AST_Type *dsttype, JobOnCompleteCallback on_complete)
    : Job (ctx->global), src(value), dsttype(dsttype), prio(0)
{
    this->on_complete = on_complete;
}

std::wstring CastJob::get_name() {
    std::wostringstream s;
    s << "CastJob<" << src << " to " << dsttype << ">";
    return s.str();
}

bool CastJob::run(Message *msg) {
    if (dsttype == src->type) {
        prio = 100;
        result = src;
        return true;
    } else {
        BuiltinCast cast;
        if (!builtin_casts.find({ src->type, dsttype }, &cast)) {
            set_error_flag();
            return false;
        }
        if (!cast.fn(global, src, (AST_Value**)&result)) {
            set_error_flag();
            return false;
        }
        prio = cast.priority;
        return true;
    }
    NOT_IMPLEMENTED();
    return false;
}
