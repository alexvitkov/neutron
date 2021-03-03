#include "cast.h"
#include "tir.h"
#include "tir_builtins.h"
#include <iostream>
#include <sstream>


u32 map_hash(TypePair pair) { return map_hash(pair.lhs) ^ (map_hash(pair.rhs) << 1); }
bool map_equals(TypePair a, TypePair b) { return a.lhs == b.lhs && a.rhs == b.rhs; }

u32 map_hash(BinaryOpKey key) { return map_hash(key.lhs) ^ (map_hash(key.rhs) << 1) ^ key.op; }
bool map_equals(BinaryOpKey a, BinaryOpKey b) { return a.lhs == b.lhs && a.rhs == b.rhs && a.op == b.op; }

u32 map_hash(UnaryOpKey key) { return map_hash(key.inner) ^ key.op; }
bool map_equals(UnaryOpKey a, UnaryOpKey b) { return a.inner == b.inner && a.op == b.op; }

CastJob::CastJob(AST_Context *ctx, AST_Value *value, AST_Type *dsttype)
    : Job (ctx->global), src(value), dsttype(dsttype), prio(0)
{
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
        MUST_OR_FAIL_JOB (builtin_casts.find({ src->type, dsttype }, &cast));
        MUST_OR_FAIL_JOB (cast.fn(global, src, &result));
        prio = cast.priority;
        return true;
    }
    NOT_IMPLEMENTED();
    return false;
}
