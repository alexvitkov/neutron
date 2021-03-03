#include "cast.h"
#include "tir.h"
#include <iostream>
#include <sstream>

template <typename T>
inline bool number_literal_to_unsigned(AST_GlobalContext &global, AST_Value *src, AST_Value **dst, AST_Type *t) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)src;
    assert(nl IS AST_NUMBER);

    T num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        return false;
    }

    AST_SmallNumber *sn = global.alloc<AST_SmallNumber>(t);
    sn->u64_val = num;
    *dst = sn;
    return true;
}

bool number_literal_to_u64(AST_GlobalContext &global, AST_Value *src, AST_Value **dst) {
    return number_literal_to_unsigned<u64>(global, src, dst, &t_u64);
}

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
        MUST (global.builtin_casts.find({ src->type, dsttype }, &cast));
        MUST (cast.fn(global, src, &result));
        prio = cast.priority;
        return true;
    }
    set_error_flag();
    return false;
}

/*
MatchCallJob::MatchCallJob(AST_GlobalContext &global, AST_Call *fncall, AST_Fn *fn)
    : Job(global), fncall(fncall), fn(fn), casted_args(fncall->args.size) {}


std::wstring MatchCallJob::get_name() {
    std::wostringstream s;
    s << "MatchCallJob<" << fncall << ">";
    return s.str();
}

bool MatchCallJob::run(Message *msg) {
    AST_FnType *fntype = fn->fntype();

    u64 num_args = fncall->args.size;
    u64 num_params = fntype->param_types.size;

    if (!fntype->is_variadic) {
        MUST (num_args == num_params);
    } else {
        MUST (num_args >= num_params);
    }


    if (casted_args.size == 0) {
        casted_args.size = casted_args.capacity;
        for (int i = 0; i < num_args; i++) {
            AST_Type *param_type = i < num_params ? fntype->param_types[i] : &t_any8;

            if (param_type == fncall->args[i]->type) {
                casted_args[i] = fncall->args[i];
            } else {
                ResolveJob find_cast_job(global, nullptr);

                if (!global.casts.find({ fncall->args[i]->type, param_type }, &run_fn)) {
                    // TODO stop the remaining dependencies
                    return false;
                }
                int add;
                if (global.default_cast_priorities.find({ fncall->args[i]->type, param_type }, &add)) {
                    priority += add;
                    npriority += 1;
                }

                Job *heap_job = cast.run_stackjob<CastJob>();
                if (heap_job) {
                    add_dependency(heap_job);
                } else if (cast.flags & JOB_ERROR) {
                    // TODO stop the remaining dependencies
                    return false;
                }
            }
        }
    }

    if (dependencies_left > 0) {
        return false;
    }

    MatchCallJobOverMessage matched_msg;
    matched_msg.msgtype = MSG_FN_MATCHED;
    matched_msg.job = this;
    global.send_message(&matched_msg);

    return true;
}





bool number_literal_to_u32(CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
    assert(nl IS AST_NUMBER);
    u32 num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        self->set_error_flag();
        return false;
    }
    AST_SmallNumber *sn = self->global.alloc<AST_SmallNumber>(&t_u64);
    sn->u64_val = num;
    *self->result = sn;
    return true;
}

bool number_literal_to_u16(CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
    assert(nl IS AST_NUMBER);
    u16 num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        self->set_error_flag();
        return false;
    }
    AST_SmallNumber *sn = self->global.alloc<AST_SmallNumber>(&t_u64);
    sn->u64_val = num;
    *self->result = sn;
    return true;
}

bool number_literal_to_u8(CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
    assert(nl IS AST_NUMBER);
    u8 num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        self->set_error_flag();
        return false;
    }
    AST_SmallNumber *sn = self->global.alloc<AST_SmallNumber>(&t_u64);
    sn->u64_val = num;
    *self->result = sn;
    return true;
}
*/

TIR_Function add_u8_u8 = {
    {  }
};
