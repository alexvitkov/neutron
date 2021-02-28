#include "cast.h"
#include <iostream>
#include <sstream>

bool CastJob::run(Message *msg) {
    return run_fn(this);
}

std::wstring CastJob::get_name() {
    return L"CastJob";
}


MatchFnCallJob::MatchFnCallJob(AST_GlobalContext &global, AST_FnCall *fncall, AST_Fn *fn)
    : Job(global), fncall(fncall), fn(fn), casted_args(fncall->args.size) {}


std::wstring MatchFnCallJob::get_name() {
    std::wostringstream s;
    s << "MatchFnCallJob<" << fncall << ">";
    return s.str();
}

bool MatchFnCallJob::run(Message *msg) {
    AST_FnType *fntype = (AST_FnType*)fn->type;

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
                CastJobMethod run_fn;
                if (!global.casts.find({ fncall->args[i]->type, param_type }, &run_fn)) {
                    // TODO stop the remaining dependencies
                    return false;
                }
                int add;
                if (global.default_cast_priorities.find({ fncall->args[i]->type, param_type }, &add)) {
                    priority += add;
                    npriority += 1;
                }

                CastJob cast(global, fncall->args[i], &casted_args[i], run_fn);

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

    MatchFnCallJobOverMessage matched_msg;
    matched_msg.msgtype = MSG_FN_MATCHED;
    matched_msg.job = this;
    global.send_message(&matched_msg);

    return true;
}






bool number_literal_to_u64(CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
    u64 num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        self->set_error_flag();
        return false;
    }
    AST_SmallNumber *sn = self->global.alloc<AST_SmallNumber>(&t_u64);
    sn->u64_val = num;
    *self->result = sn;
    return true;
}

bool number_literal_to_u32(CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
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
